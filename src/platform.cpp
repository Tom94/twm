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
	return format("{} ({})", trim(std::system_category().message(code)), code);
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

COLORREF to_colorref(uint32_t color) { return RGB((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF); }

void set_window_border_color(HWND handle, COLORREF color) {
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

bool is_autostart_enabled() {
	HKEY key;
	if (HRESULT res = RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &key) != ERROR_SUCCESS) {
		log_warning("Could not open registry key for reading: {}", error_string(res));
		return false;
	}

	auto guard = ScopeGuard([&]() { RegCloseKey(key); });

	wchar_t registry_path[MAX_PATH];
	DWORD n_bytes = sizeof(registry_path);
	if (RegQueryValueExW(key, L"twm", 0, nullptr, (LPBYTE)registry_path, &n_bytes) != ERROR_SUCCESS) {
		return false;
	}

	wchar_t executable_path[MAX_PATH];
	DWORD len = GetModuleFileNameW(nullptr, executable_path, MAX_PATH);
	if (len == 0 || len == MAX_PATH) {
		log_warning("Could not get module file name");
		return false;
	}

	return !wcscmp(registry_path, executable_path);
}

bool set_autostart_enabled(bool value) {
	HKEY key;
	if (HRESULT res = RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_WRITE, &key) != ERROR_SUCCESS) {
		log_warning("Could not open registry key for writing: {}", error_string(res));
		return false;
	}

	auto guard = ScopeGuard([&]() { RegCloseKey(key); });

	if (value) {
		log_debug("Enabling autostart");

		wchar_t path[MAX_PATH];
		DWORD len = GetModuleFileNameW(nullptr, path, MAX_PATH);
		if (len == 0 || len == MAX_PATH) {
			log_warning("Could not get module file name");
			return false;
		}

		if (HRESULT res = RegSetValueExW(key, L"twm", 0, REG_SZ, (const BYTE*)path, sizeof(wchar_t) * (len + 1)) != ERROR_SUCCESS) {
			log_warning("Could not set registry value: {}", error_string(res));
			return false;
		}
	} else {
		log_debug("Disabling autostart");

		if (HRESULT res = RegDeleteValueW(key, L"twm") != ERROR_SUCCESS) {
			log_warning("Could not delete registry value: {}", error_string(res));
			return false;
		}
	}

	return true;
}

} // namespace twm
