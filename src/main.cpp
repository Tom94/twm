// This file was developed by Thomas Müller <thomas94@gmx.net>.
// It is published under the GPU GPLv3 license; see LICENSE.txt for details.

#include <algorithm>
#include <chrono>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <variant>

#define NOMINMAX
#include <ShlObj.h>
#include <Windows.h>
#ifdef far
#	undef far
#	undef near
#endif

// Saves so much typing
using namespace std;

// Helper math
struct Vec2 {
	Vec2(float c = 0.0f) : Vec2(c, c) {}
	Vec2(float x, float y) : x{x}, y{y} {}

	static Vec2 zero() { return Vec2{0.0f}; }
	static Vec2 ones() { return Vec2{1.0f}; }

	Vec2& operator/=(float other) { return *this = *this / other; }
	Vec2& operator*=(float other) { return *this = *this * other; }
	Vec2& operator-=(float other) { return *this = *this - other; }
	Vec2& operator+=(float other) { return *this = *this + other; }

	Vec2 operator/(float other) const { return {x / other, y / other}; }
	Vec2 operator*(float other) const { return {x * other, y * other}; }
	Vec2 operator-(float other) const { return {x - other, y - other}; }
	Vec2 operator+(float other) const { return {x + other, y + other}; }

	Vec2& operator/=(const Vec2& other) { return *this = *this / other; }
	Vec2& operator*=(const Vec2& other) { return *this = *this * other; }
	Vec2& operator-=(const Vec2& other) { return *this = *this - other; }
	Vec2& operator+=(const Vec2& other) { return *this = *this + other; }

	Vec2 operator/(const Vec2& other) const { return {x / other.x, y / other.y}; }
	Vec2 operator*(const Vec2& other) const { return {x * other.x, y * other.y}; }
	Vec2 operator-(const Vec2& other) const { return {x - other.x, y - other.y}; }
	Vec2 operator+(const Vec2& other) const { return {x + other.x, y + other.y}; }

	bool operator==(const Vec2& other) const { return x == other.x && y == other.y; }
	bool operator!=(const Vec2& other) const { return x != other.x || y != other.y; }

	float length_sq() const { return x * x + y * y; }
	float length() const { return sqrt(length_sq()); }
	float prod() const { return x * y; }
	float sum() const { return x + y; }
	float max() const { return std::max(x, y); }
	float min() const { return std::min(x, y); }
	int max_axis() const { return x > y ? 0 : 1; }
	int min_axis() const { return x > y ? 1 : 0; }

	float x, y;
};

struct Rect {
	Vec2 top_left;
	Vec2 bottom_right;

	Rect(const RECT& r) :
		top_left{static_cast<float>(r.left), static_cast<float>(r.top)},
		bottom_right{static_cast<float>(r.right), static_cast<float>(r.bottom)} {}

	bool operator==(const Rect& other) const {
		return top_left == other.top_left && bottom_right == other.bottom_right;
	}

	bool operator!=(const Rect& other) const {
		return top_left != other.top_left || bottom_right != other.bottom_right;
	}

	Vec2 center() const { return (top_left + bottom_right) / 2.0f; }
	Vec2 size() const { return bottom_right - top_left; }
	Vec2 area() const { return size().prod(); }
};

std::ostream& operator<<(std::ostream& os, const Vec2& v) {
	return os << "[" << v.x << ", " << v.y << "]";
}

std::ostream& operator<<(std::ostream& os, const Rect& r) {
	return os << "[top_left=" << r.top_left << ", bottom_right=" << r.bottom_right << "]";
}

// Convenience string processing functions
string utf16_to_utf8(const wstring& utf16) {
	string utf8;
	if (!utf16.empty()) {
		int size = WideCharToMultiByte(CP_UTF8, 0, &utf16[0], (int)utf16.size(), NULL, 0, NULL, NULL);
		utf8.resize(size, 0);
		WideCharToMultiByte(CP_UTF8, 0, &utf16[0], (int)utf16.size(), &utf8[0], size, NULL, NULL);
	}
	return utf8;
}

wstring utf8_to_utf16(const string& utf8) {
	wstring utf16;
	if (!utf8.empty()) {
		int size = MultiByteToWideChar(CP_UTF8, 0, &utf8[0], (int)utf8.size(), NULL, 0);
		utf16.resize(size, 0);
		MultiByteToWideChar(CP_UTF8, 0, &utf8[0], (int)utf8.size(), &utf16[0], size);
	}
	return utf16;
}

string to_lower(string str) {
	transform(begin(str), end(str), begin(str), [](unsigned char c) { return (char)tolower(c); });
	return str;
}

string ltrim(string s) {
	s.erase(s.begin(), find_if(s.begin(), s.end(), [](char c) { return !isspace(c); }));
	return s;
}

string rtrim(string s) {
	s.erase(find_if(s.rbegin(), s.rend(), [](char c) { return !isspace(c); }).base(), s.end());
	return s;
}

string trim(string s) {
	return ltrim(rtrim(s));
}

template <typename T> string join(const T& components, const string& delim) {
	ostringstream s;
	for (const auto& component : components) {
		if (&components[0] != &component) {
			s << delim;
		}
		s << component;
	}

	return s.str();
}

vector<string> split(string text, const string& delim) {
	vector<string> result;
	size_t begin = 0;
	while (true) {
		size_t end = text.find_first_of(delim, begin);
		if (end == string::npos) {
			result.emplace_back(text.substr(begin));
			return result;
		} else {
			result.emplace_back(text.substr(begin, end - begin));
			begin = end + 1;
		}
	}

	return result;
}

class ScopeGuard {
public:
	ScopeGuard() = default;
	ScopeGuard(const std::function<void()>& callback) : m_callback{callback} {}
	ScopeGuard(std::function<void()>&& callback) : m_callback{std::move(callback)} {}
	ScopeGuard& operator=(const ScopeGuard& other) = delete;
	ScopeGuard(const ScopeGuard& other) = delete;
	ScopeGuard& operator=(ScopeGuard&& other) {
		std::swap(m_callback, other.m_callback);
		return *this;
	}

	ScopeGuard(ScopeGuard&& other) { *this = std::move(other); }
	~ScopeGuard() {
		if (m_callback) {
			m_callback();
		}
	}

	void disarm() { m_callback = {}; }

private:
	std::function<void()> m_callback;
};

// Windows error handling
int last_error_code() {
	return GetLastError();
}

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

string last_error_string() {
	return trim(error_string(last_error_code()));
}

// Hotkey management
using Callback = function<void()>;

struct Hotkey {
	int id;
	function<void()> cb;
};

unordered_map<string, UINT> string_to_modifier = {
	{"ctrl",    MOD_CONTROL},
	{"control", MOD_CONTROL},
	{"alt",     MOD_ALT    },
	{"super",   MOD_WIN    },
	{"win",     MOD_WIN    },
	{"shift",   MOD_SHIFT  },
};

unordered_map<string, UINT> string_to_keycode = {
	{"back",      VK_BACK  },
	{"backspace", VK_BACK  },
	{"tab",       VK_TAB   },
	{"return",    VK_RETURN},
	{"enter",     VK_RETURN},
	{"escape",    VK_ESCAPE},
	{"esc",       VK_ESCAPE},
	{"space",     VK_SPACE },
};

class Hotkeys {
	vector<Hotkey> m_hotkeys;

public:
	~Hotkeys() { clear(); }

	void add(const string& keycombo, Callback cb) {
		int id = (int)m_hotkeys.size();
		auto parts = split(keycombo, "+");
		UINT mod = 0;
		UINT keycode = 0;

		// keycombo is of the form mod1+mod2+...+keycode
		// case insensitive and with optional spaces. Parse below.
		for (const auto& part : parts) {
			auto name = to_lower(trim(part));

			if (string_to_modifier.count(name) > 0) {
				mod |= string_to_modifier[name];
				continue;
			}

			// If none of the above modifiers is given, that means we are given a keycode.
			// Only one keycode per keybinding is allowed -- check for this.
			if (keycode != 0) {
				throw runtime_error{string{"Error registering "} + keycombo + ": more than one keycode"};
			}

			if (string_to_keycode.count(name) > 0) {
				keycode = string_to_keycode[name];
				continue;
			}

			if (name.size() != 1) {
				throw runtime_error{string{"Error registering "} + keycombo + ": unknown keycode"};
			}

			keycode = (char)toupper(name[0]);
		}

		if (RegisterHotKey(nullptr, id, mod, keycode) == 0) {
			throw runtime_error{string{"Error registering "} + keycombo + ": " + last_error_string()};
		}

		m_hotkeys.emplace_back(id, cb);
	}

	void trigger(int id) const {
		if (id < 0 || id >= (int)m_hotkeys.size()) {
			throw runtime_error{"Invalid hotkey id"};
		}

		m_hotkeys[id].cb();
	}

	void check_for_triggers() const {
		MSG msg = {};
		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE) != 0) {
			switch (msg.message) {
				case WM_HOTKEY: {
					trigger((int)msg.wParam);
				} break;
				default: {
					std::cout << "Unknown message: " << msg.message << std::endl;
				} break;
			}
		}
	}

	void clear() {
		for (size_t i = 0; i < m_hotkeys.size(); ++i) {
			// We do not care about errors in the unregistering process here.
			// Simply try to unbind all hotkeys and hope for the best -- there
			// is nothing we can do if unbinding fails.
			UnregisterHotKey(nullptr, (int)i);
		}

		m_hotkeys.clear();
	}
};

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

auto query_virtual_desktop_manager() {
	const CLSID CLSID_ImmersiveShell = {
		0xC2F03A33, 0x21F5, 0x47FA, {0xB4, 0xBB, 0x15, 0x63, 0x62, 0xA2, 0xF2, 0x39}
    };

	IServiceProvider* service_provider = NULL;

	CoInitialize(NULL);
	HRESULT hr = CoCreateInstance(
		CLSID_ImmersiveShell, NULL, CLSCTX_LOCAL_SERVER, __uuidof(IServiceProvider), (PVOID*)&service_provider
	);

	if (FAILED(hr)) {
		throw runtime_error{string{"Failed to get immersive shell service provider: "} + to_string(hr)};
	}

	auto guard = ScopeGuard([&]() { service_provider->Release(); });

	IVirtualDesktopManager* virtual_desktop_manager;
	hr = service_provider->QueryService(__uuidof(IVirtualDesktopManager), &virtual_desktop_manager);

	if (FAILED(hr)) {
		throw runtime_error{"Failed to get virtual desktop manager."};
	}

	SetLastError(0);
	return virtual_desktop_manager;
}

IVirtualDesktopManager* virtual_desktop() {
	static auto virtual_desktop = query_virtual_desktop_manager();
	return virtual_desktop;
}

// Window management
class Window {
	string m_name;
	Rect m_rect;
	HWND m_handle;

public:
	Window(HWND handle) : m_name{get_window_text(handle)}, m_rect{get_window_rect(handle)}, m_handle{handle} {}

	// Returns true if the name changed
	bool update() {
		string old_name = m_name;
		Rect old_rect = m_rect;
		m_name = get_window_text(m_handle);
		m_rect = get_window_rect(m_handle);
		return m_name != old_name || m_rect != old_rect;
	}

	bool can_be_managed() {
		BOOL on_current_desktop = 0;
		virtual_desktop()->IsWindowOnCurrentVirtualDesktop(m_handle, &on_current_desktop);
		return m_name.size() > 0 && !IsIconic(m_handle) && IsWindowVisible(m_handle) && on_current_desktop;
	}

	const string& name() const { return m_name; }
	const Rect& rect() const { return m_rect; }

	HWND handle() const { return m_handle; }
};

struct BspNode {
	struct Children {
		unique_ptr<BspNode> left, right;
	};

	variant<HWND, Children> payload;
};

class Workspace {
	unordered_map<HWND, Window> m_windows;
	unique_ptr<BspNode> m_root;

public:
	Workspace() {}

	~Workspace() {}

	void manage(Window window) {
		m_windows.insert({window.handle(), window});
		std::cout << window.name() << ": " << window.rect() << std::endl;
	}
} workspace;

BOOL CALLBACK on_enum_window(__in HWND handle, __in LPARAM) {
	auto window = Window{handle};

	if (window.can_be_managed()) {
		workspace.manage(window);
	}

	return TRUE;
}

BOOL CALLBACK on_enum_desktop(_In_ LPWSTR desktop_name, _In_ LPARAM) {
	string name = utf16_to_utf8(desktop_name);
	std::cout << "Desktop: " << name << std::endl;
	return TRUE;
}

int main() {
	CoInitialize(nullptr);
	SetConsoleOutputCP(CP_UTF8);
	SetLastError(0);

	try {
		Hotkeys hotkeys;
		// vector<Workspace> workspaces;

		hotkeys.add("alt+h", []() { std::cout << "left" << std::endl; });
		hotkeys.add("alt+j", []() { std::cout << "down" << std::endl; });
		hotkeys.add("alt+k", []() { std::cout << "up" << std::endl; });
		hotkeys.add("alt+l", []() { std::cout << "right" << std::endl; });

		EnumWindows(on_enum_window, 0);

		while (true) {
			hotkeys.check_for_triggers();
			this_thread::sleep_for(1ms);
		}
	} catch (const runtime_error& e) {
		std::cerr << "Uncaught exception: " << e.what() << std::endl;
	}
}
