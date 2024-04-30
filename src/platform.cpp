// This file was developed by Thomas Müller <thomas94@gmx.net>.
// It is published under the GPU GPLv3 license; see LICENSE.txt for details.

#include <twm/common.h>
#include <twm/logging.h>
#include <twm/platform.h>

#include <dwmapi.h>
#include <winuser.h>

using namespace std;

namespace twm {

int last_error_code() { return GetLastError(); }

string error_string(int code) {
	char* s = NULL;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		code,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPSTR)&s,
		0,
		NULL
	);

	string result = s;
	LocalFree(s);

	return trim(result);
}

string last_error_string() { return error_string(last_error_code()); }

bool set_window_rect(HWND handle, const Rect& r) {
	if (SetWindowPos(
			handle,
			nullptr,
			(LONG)r.top_left.x,
			(LONG)r.top_left.y,
			(LONG)r.size().x,
			(LONG)r.size().y,
			SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER
		) == 0) {
		log_warning("Could not set rect: {}", last_error_string());
		return false;
	}

	return true;
}

Rect get_window_rect(HWND handle) {
	if (RECT r; GetWindowRect(handle, &r) == 0) {
		throw runtime_error{format("Could not obtain rect: {}", last_error_string())};
	} else {
		return {r};
	}
}

bool set_window_frame_bounds(HWND handle, const Rect& r) {
	// In an ideal world, we would use the Windows API to directly set the
	// window's frame bounds, but, alas, no such API function exists. As a
	// workaround, we compute the current margin between the window's rect
	// and frame bound and use it to derive a rect that corresponds to the
	// target frame bounds.
	Rect margin = get_window_rect(handle) - get_window_frame_bounds(handle);
	return set_window_rect(handle, r + margin);
}

Rect get_window_frame_bounds(HWND handle) {
	if (RECT r; HRESULT result = DwmGetWindowAttribute(handle, DWMWA_EXTENDED_FRAME_BOUNDS, &r, sizeof(r)) != S_OK) {
		static bool warned = false;
		if (!warned) {
			log_warning(
				"DwmGetWindowAttribute(DWMWA_EXTENDED_FRAME_BOUNDS) failed: {}. Falling back to GetWindowRect.",
				error_string(result)
			);
			warned = true;
		}

		return get_window_rect(handle);
	} else {
		return {r};
	}
}

void set_window_rounded_corners(HWND handle, RoundedCornerPreference rounded) {
	static const uint32_t WINDOW_CORNER_PREFERENCE = 33;
	DwmSetWindowAttribute(handle, WINDOW_CORNER_PREFERENCE, &rounded, sizeof(rounded));
}

void set_window_border_color(HWND handle, BorderColor color) {
	static const uint32_t BORDER_COLOR = 34;
	DwmSetWindowAttribute(handle, BORDER_COLOR, &color, sizeof(color));
}

void set_system_dropshadow(bool enabled) {
	if (!SystemParametersInfo(SPI_SETDROPSHADOW, 0, (PVOID)enabled, SPIF_SENDCHANGE)) {
		log_warning("Could not set dropshadow: {}", last_error_string());
	}
}

bool focus_window(HWND handle) { return SetForegroundWindow(handle) != 0; }

string get_window_text(HWND handle) {
	if (int name_length = GetWindowTextLengthW(handle) <= 0 || last_error_code() != 0) {
		SetLastError(0);
		return "";
	} else {
		wstring wname;
		wname.resize(name_length + 1);
		GetWindowTextW(handle, wname.data(), (int)wname.size());
		return utf16_to_utf8(wname);
	}
}

bool terminate_process(HWND handle) {
	if (DWORD process_id = 0; GetWindowThreadProcessId(handle, &process_id) == 0 || process_id == 0) {
		return false;
	} else {
		HANDLE process = OpenProcess(PROCESS_TERMINATE, 0, process_id);
		return process && TerminateProcess(process, 0) != 0;
	}
}

bool close_window(HWND handle) { return PostMessage(handle, WM_CLOSE, 0, 0) != 0; }

auto query_desktop_manager() {
	const CLSID CLSID_ImmersiveShell = {
		0xC2F03A33,
		0x21F5,
		0x47FA,
		{0xB4, 0xBB, 0x15, 0x63, 0x62, 0xA2, 0xF2, 0x39}
	};

	IServiceProvider* service_provider = NULL;

	HRESULT hr = CoCreateInstance(
		CLSID_ImmersiveShell,
		NULL,
		CLSCTX_LOCAL_SERVER,
		__uuidof(IServiceProvider),
		(PVOID*)&service_provider
	);

	if (FAILED(hr)) {
		throw runtime_error{format("Failed to get immersive shell service provider: {}", std::to_string(hr))};
	}

	auto guard = ScopeGuard([&]() { service_provider->Release(); });

	IVirtualDesktopManager* desktop_manager;
	hr = service_provider->QueryService(__uuidof(IVirtualDesktopManager), &desktop_manager);

	if (FAILED(hr)) {
		throw runtime_error{"Failed to get virtual desktop manager."};
	}

	SetLastError(0);
	return desktop_manager;
}

IVirtualDesktopManager* desktop_manager() {
	static auto desktop = query_desktop_manager();
	return desktop;
}

optional<GUID> get_window_desktop_id(HWND handle) {
	if (
		GUID desktop_id;
		desktop_manager()->GetWindowDesktopId(handle, &desktop_id) != S_OK || equal_to<GUID>{}(desktop_id, GUID{})
	) {
		return {};
	} else {
		return desktop_id;
	}
}

bool is_window_on_current_desktop(HWND handle) {
	BOOL is_current_desktop = 0;
	HRESULT r = desktop_manager()->IsWindowOnCurrentVirtualDesktop(handle, &is_current_desktop);
	return r == S_OK && is_current_desktop != 0;
}

bool move_window_to_desktop(HWND handle, const GUID& desktop_id) {
	if (HRESULT res = desktop_manager()->MoveWindowToDesktop(handle, desktop_id) != S_OK) {
		log_warning("Failed to move window to desktop: {}", error_string(res));
		return false;
	} else {
		return true;
	}
}

} // namespace twm
