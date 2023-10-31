// This file was developed by Thomas Müller <thomas94@gmx.net>.
// It is published under the GPU GPLv3 license; see LICENSE.txt for details.

#include <twm/common.h>
#include <twm/platform.h>

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

	return result;
}

string last_error_string() { return trim(error_string(last_error_code())); }

void set_window_rect(HWND handle, const Rect& r) {
	if (SetWindowPos(
			handle,
			nullptr,
			(LONG)r.top_left.x,
			(LONG)r.top_left.y,
			(LONG)r.size().x,
			(LONG)r.size().y,
			SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER
		) == 0) {
		throw runtime_error{format("Could not set rect: {}", last_error_string())};
	}
}

Rect get_window_rect(HWND handle) {
	RECT r;
	if (GetWindowRect(handle, &r) == 0) {
		throw runtime_error{string{"Could not obtain rect: "} + last_error_string()};
	}

	return {r};
}

string get_window_text(HWND handle) {
	int name_length = GetWindowTextLengthW(handle);
	if (name_length <= 0 || last_error_code() != 0) {
		SetLastError(0);
		return "";
	}

	wstring wname;
	wname.resize(name_length + 1);
	GetWindowTextW(handle, wname.data(), (int)wname.size());
	return utf16_to_utf8(wname);
}

auto query_desktop_manager() {
	const CLSID CLSID_ImmersiveShell = {
		0xC2F03A33, 0x21F5, 0x47FA, {0xB4, 0xBB, 0x15, 0x63, 0x62, 0xA2, 0xF2, 0x39}
    };

	IServiceProvider* service_provider = NULL;

	HRESULT hr = CoCreateInstance(
		CLSID_ImmersiveShell, NULL, CLSCTX_LOCAL_SERVER, __uuidof(IServiceProvider), (PVOID*)&service_provider
	);

	if (FAILED(hr)) {
		throw runtime_error{string{"Failed to get immersive shell service provider: "} + to_string(hr)};
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
	GUID desktop_id;
	HRESULT result = desktop_manager()->GetWindowDesktopId(handle, &desktop_id);
	if (result != S_OK || equal_to<GUID>{}(desktop_id, GUID{})) {
		return {};
	}

	return desktop_id;
}

bool is_window_on_current_desktop(HWND handle) {
	BOOL is_current_desktop = 0;
	HRESULT r = desktop_manager()->IsWindowOnCurrentVirtualDesktop(handle, &is_current_desktop);
	return r == S_OK && is_current_desktop != 0;
}

} // namespace twm
