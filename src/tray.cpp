// This file was developed by Thomas Müller <thomas94@gmx.net>.
// It is published under the GPU GPLv3 license; see LICENSE.txt for details.

#include <twm/common.h>
#include <twm/icon.h>
#include <twm/logging.h>
#include <twm/platform.h>
#include <twm/tray.h>

#include <string>

using namespace std;

namespace twm {

TrayPresence::TrayPresence(HINSTANCE instance) {
	static const string class_name = "twm_tray_window";
	static atomic<uint32_t> uid_counter = 0;
	m_uid = uid_counter++;

	if (m_uid == 0) {
		// This is technically not thread safe, but doubt that'll ever be relevant
		WNDCLASS wc = {};
		wc.lpfnWndProc = tray_window_proc;
		wc.hInstance = instance;
		wc.lpszClassName = class_name.c_str();

		RegisterClass(&wc);
	}

	m_invisible_window = CreateWindowEx(
		0,
		class_name.c_str(),
		class_name.c_str(),
		0,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		HWND_DESKTOP,
		nullptr,
		instance,
		nullptr
	);

	if (m_invisible_window == nullptr) {
		throw runtime_error{format("Failed to create invisible window for tray icon: {}", last_error_string())};
	}

	NOTIFYICONDATA nid = {};
	nid.cbSize = sizeof(NOTIFYICONDATA);
	nid.hWnd = m_invisible_window;
	nid.uID = m_uid; // Unique identifier of the tray icon
	nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	nid.uCallbackMessage = WM_TRAYICON_MSG;
	nid.hIcon = (HICON)LoadImage(instance, MAKEINTRESOURCE(IDI_TWM_ICON), IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR);
	if (nid.hIcon == nullptr) {
		throw runtime_error{format("Failed to load icon: {}", last_error_string())};
	}

	strcpy_s(nid.szTip, "twm");

	if (!Shell_NotifyIcon(NIM_ADD, &nid)) {
		throw runtime_error{"Failed to add tray icon"};
	}
}

TrayPresence::~TrayPresence() {
	NOTIFYICONDATA nid = {};
	nid.cbSize = sizeof(NOTIFYICONDATA);
	nid.hWnd = m_invisible_window;
	nid.uID = m_uid;

	if (!Shell_NotifyIcon(NIM_DELETE, &nid)) {
		log_warning("Failed to remove tray icon");
	}

	if (!DestroyWindow(m_invisible_window)) {
		log_warning("Failed to destroy invisible window: {}", last_error_string());
	}
}

LRESULT CALLBACK TrayPresence::tray_window_proc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	enum TrayMessage : uint32_t {
		IDM_EXIT,
		IDM_AUTOSTART_ENABLE,
		IDM_AUTOSTART_DISABLE,
	};

	switch (message) {
		case WM_TRAYICON_MSG: {
			if (lParam == WM_RBUTTONUP) {
				// User has right-clicked the tray icon, display the context menu
				bool autostart = is_autostart_enabled();

				HMENU menu = CreatePopupMenu();
				InsertMenu(
					menu,
					(UINT)-1,
					MF_BYPOSITION | MF_STRING | (autostart ? MF_CHECKED : 0),
					autostart ? IDM_AUTOSTART_DISABLE : IDM_AUTOSTART_ENABLE,
					"Start with Windows"
				);
				InsertMenu(menu, (UINT)-1, MF_BYPOSITION | MF_STRING, IDM_EXIT, "Exit");

				POINT pt;
				GetCursorPos(&pt);
				SetForegroundWindow(hWnd);
				TrackPopupMenu(menu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hWnd, NULL);
				PostMessage(hWnd, WM_NULL, 0, 0); // Ensure that menu will close
				DestroyMenu(menu);
			}
		} break;
		case WM_DESTROY:
		case WM_CLOSE:
		case WM_QUIT: {
			log_debug("Tray received WM_DESTROY/CLOSE/QUIT. Exiting...");
			PostQuitMessage(0);
		} break;
		case WM_COMMAND: {
			switch (LOWORD(wParam)) {
				case IDM_AUTOSTART_ENABLE: {
					log_debug("Enabling autostart");
					set_autostart_enabled(true);
				} break;
				case IDM_AUTOSTART_DISABLE: {
					log_debug("Disabling autostart");
					set_autostart_enabled(false);
				} break;
				case IDM_EXIT: {
					log_debug("Tray received IDM_EXIT. Exiting...");
					PostQuitMessage(0);
				} break;
			}
		} break;
		default: {
			return DefWindowProc(hWnd, message, wParam, lParam);
		} break;
	}

	return 0;
}

} // namespace twm
