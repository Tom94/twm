// This file was developed by Thomas Müller <thomas94@gmx.net>.
// It is published under the GPU GPLv3 license; see LICENSE.txt for details.

#include <algorithm>
#include <chrono>
#include <functional>
#include <iostream>
#include <optional>
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

enum class Severity : uint8_t {
	Debug,
	Info,
	Warning,
	Error,
};

auto min_severity = Severity::Info;

void log(Severity severity, const string& str) {
	if (severity < min_severity) {
		return;
	}

	switch (severity) {
		case Severity::Debug: cout << format("DEBUG: {}\n", str); break;
		case Severity::Info: cout << format("INFO: {}\n", str); break;
		case Severity::Warning: cerr << format("WARNING: {}\n", str); break;
		case Severity::Error: cerr << format("ERROR: {}\n", str); break;
	}
}

void log_debug(const string& str) { log(Severity::Debug, str); }
void log_info(const string& str) { log(Severity::Info, str); }
void log_warning(const string& str) { log(Severity::Warning, str); }
void log_error(const string& str) { log(Severity::Error, str); }

#define STRINGIFY(x) #x
#define STR(x) STRINGIFY(x)
#define FILE_LINE __FILE__ ":" STR(__LINE__)
#define TWM_ASSERT(x)                                                \
	do {                                                             \
		if (!(x)) {                                                  \
			throw runtime_error{string{FILE_LINE " " #x " failed"}}; \
		}                                                            \
	} while (0);

// Helper math
struct Vec2 {
	float x, y;

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
	float max() const { return ::max(x, y); }
	float min() const { return ::min(x, y); }
	int max_axis() const { return x > y ? 0 : 1; }
	int min_axis() const { return x > y ? 1 : 0; }

	float operator[](size_t idx) const {
		TWM_ASSERT(idx == 0 || idx == 1);
		return idx == 0 ? x : y;
	}
};

struct Rect {
	Vec2 top_left = {};
	Vec2 bottom_right = {};

	Rect() = default;
	Rect(const RECT& r) :
		top_left{static_cast<float>(r.left), static_cast<float>(r.top)},
		bottom_right{static_cast<float>(r.right), static_cast<float>(r.bottom)} {}

	bool operator==(const Rect& other) const {
		return top_left == other.top_left && bottom_right == other.bottom_right;
	}

	bool operator!=(const Rect& other) const {
		return top_left != other.top_left || bottom_right != other.bottom_right;
	}

	float distance_with_axis_preference(uint32_t axis, const Rect& other) const {
		auto off_axis = (axis + 1) % 2;
		auto c = center();
		auto oc = other.center();
		return abs(c[axis] - oc[axis]) + max(0.0f, abs(c[off_axis] - oc[off_axis]) - size()[off_axis] / 2);
	}

	Vec2 center() const { return (top_left + bottom_right) / 2.0f; }
	Vec2 size() const { return bottom_right - top_left; }
	Vec2 area() const { return size().prod(); }
};

ostream& operator<<(ostream& os, const Vec2& v) { return os << "[" << v.x << ", " << v.y << "]"; }

ostream& operator<<(ostream& os, const Rect& r) {
	return os << "[top_left=" << r.top_left << ", bottom_right=" << r.bottom_right << "]";
}

// hash_combine from https://stackoverflow.com/a/50978188
template <typename T> T xorshift(T n, int i) { return n ^ (n >> i); }

inline uint32_t distribute(uint32_t n) {
	uint32_t p = 0x55555555ul; // pattern of alternating 0 and 1
	uint32_t c = 3423571495ul; // random uneven integer constant;
	return c * xorshift(p * xorshift(n, 16), 16);
}

inline uint64_t distribute(uint64_t n) {
	uint64_t p = 0x5555555555555555ull;   // pattern of alternating 0 and 1
	uint64_t c = 17316035218449499591ull; // random uneven integer constant;
	return c * xorshift(p * xorshift(n, 32), 32);
}

template <typename T, typename S>
constexpr typename enable_if<is_unsigned<T>::value, T>::type rotl(const T n, const S i) {
	const T m = (numeric_limits<T>::digits - 1);
	const T c = i & m;
	return (n << c) | (n >> (((T)0 - c) & m)); // this is usually recognized by the compiler to mean rotation
}

template <typename T> size_t hash_combine(size_t seed, const T& v) {
	return rotl(seed, numeric_limits<size_t>::digits / 3) ^ distribute(hash<T>{}(v));
}

namespace std {
template <> struct hash<GUID> {
	size_t operator()(const GUID& x) const {
		return (size_t)x.Data1 * 73856093 + (size_t)x.Data2 * 19349663 + (size_t)x.Data3 * 83492791 +
			*(uint64_t*)x.Data4 * 25165843;
	}
};
template <> struct equal_to<GUID> {
	size_t operator()(const GUID& a, const GUID& b) const {
		return a.Data1 == b.Data1 && a.Data2 == b.Data2 && a.Data3 == b.Data3 &&
			*(uint64_t*)a.Data4 == *(uint64_t*)b.Data4;
	}
};
} // namespace std

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

string trim(string s) { return ltrim(rtrim(s)); }

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
	ScopeGuard(const function<void()>& callback) : m_callback{callback} {}
	ScopeGuard(function<void()>&& callback) : m_callback{move(callback)} {}
	ScopeGuard& operator=(const ScopeGuard& other) = delete;
	ScopeGuard(const ScopeGuard& other) = delete;
	ScopeGuard& operator=(ScopeGuard&& other) {
		swap(m_callback, other.m_callback);
		return *this;
	}

	ScopeGuard(ScopeGuard&& other) { *this = move(other); }
	~ScopeGuard() {
		if (m_callback) {
			m_callback();
		}
	}

	void disarm() { m_callback = {}; }

private:
	function<void()> m_callback;
};

// Windows error handling
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

	CoInitialize(NULL);
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

enum class Direction {
	Up,
	Down,
	Left,
	Right,
};

// Window management
class Window {
	string m_name = "";
	Rect m_rect = {};
	HWND m_handle = NULL;
	GUID m_desktop_id = {};
	bool m_marked_for_deletion = false;

	Window(HWND handle, const GUID& desktop_id) :
		m_name{get_window_text(handle)}, m_rect{get_window_rect(handle)}, m_handle{handle}, m_desktop_id{desktop_id} {}

	// Returns true if the name changed
	bool update(const Window& other) {
		string old_name = m_name;
		Rect old_rect = m_rect;
		m_name = other.m_name;
		m_rect = other.m_rect;
		m_marked_for_deletion = false;
		return m_name != old_name || m_rect != old_rect;
	}

	void mark_for_deletion() { m_marked_for_deletion = true; }
	bool marked_for_deletion() const { return m_marked_for_deletion; }

public:
	friend class Desktop;

	static const Window* focused();
	static const Window* get(HWND handle);

	const Window* get_adjacent(Direction dir) const;

	bool focus() const { return SetForegroundWindow(m_handle) != 0; }

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

class Desktop {
	unordered_map<HWND, Window> m_windows;
	unique_ptr<BspNode> m_root;
	GUID m_id;

	bool can_be_managed(const Window& w) {
		return !w.name().empty() && !IsIconic(w.handle()) && IsWindowVisible(w.handle());
	}

	bool try_manage(HWND handle) {
		auto w = Window{handle, m_id};

		if (!can_be_managed(w)) {
			return false;
		}

		auto [it, inserted] = m_windows.insert({handle, w});
		if (!inserted) {
			it->second.update(w);
		}

		return true;
	}

	void mark_windows_for_deletion() {
		for (auto& [_, w] : m_windows) {
			w.mark_for_deletion();
		}
	}

	void delete_marked_windows() {
		erase_if(m_windows, [](const auto& item) { return item.second.marked_for_deletion(); });
	}

	const GUID& id() const { return m_id; }

public:
	Desktop(const GUID& id) : m_id{id} {}

	Desktop(const Desktop& other) = delete;
	Desktop(Desktop&& other) = default;
	Desktop& operator=(const Desktop& other) = delete;
	Desktop& operator=(Desktop&& other) = default;

	static Desktop* current();
	static Desktop* get(HWND handle);
	static Desktop* get(GUID id);

	static void update_all();

	const Window* get_window(HWND handle) const {
		auto it = m_windows.find(handle);
		return it != m_windows.end() ? &it->second : nullptr;
	}

	const Window* get_adjacent_window(HWND handle, Direction dir) const {
		auto* w = get_window(handle);
		if (!w) {
			return nullptr;
		}

		size_t axis = dir == Direction::Left || dir == Direction::Right ? 0 : 1;
		float sign = dir == Direction::Up || dir == Direction::Left ? 1 : -1;
		const Window* best_candidate = nullptr;
		float best_distance = numeric_limits<float>::infinity();

		Vec2 center = w->rect().center();

		for (auto& [_, ow] : m_windows) {
			float dist = w->rect().distance_with_axis_preference(axis, ow.rect());

			bool is_on_correct_side = sign * (center[axis] - ow.rect().center()[axis]) > 0;
			if (w != &ow && is_on_correct_side && dist < best_distance) {
				best_distance = dist;
				best_candidate = &ow;
			}
		}

		return best_candidate;
	}

	bool empty() const { return m_windows.empty(); }

	void print() const {
		for (auto& [_, w] : m_windows) {
			log_info(w.name());
		}
	}
};

unordered_map<GUID, Desktop> desktops;
optional<GUID> current_desktop_id = {}; // ID of the desktop the user is currently looking at.

void Desktop::update_all() {
	current_desktop_id = {};
	for (auto& [_, d] : desktops) {
		d.mark_windows_for_deletion();
	}

	EnumWindows(
		[](__in HWND handle, __in LPARAM) {
			optional<GUID> opt_desktop_id = get_window_desktop_id(handle);
			if (!opt_desktop_id.has_value()) {
				// Window does not seem to belong to any desktop... can't be managed by this app.
				return TRUE;
			}

			GUID desktop_id = opt_desktop_id.value();

			// If the window's desktop already exists, query it. Otherwise, create
			// a new desktop object, keep track of it in `desktops`, and use that one.
			auto insert_result = desktops.insert({desktop_id, Desktop{desktop_id}});
			auto& desktop = insert_result.first->second;
			if (!desktop.try_manage(handle)) {
				// If the desktop can't manage the window, don't consider it as candidate for current desktop.
				return TRUE;
			}

			// If we haven't yet figured out which desktop is the current one, check
			// whether the window we are currently looking at is on the current desktop.
			// If so, use that window's desktop.
			if (!current_desktop_id.has_value()) {
				BOOL is_current_desktop = 0;
				HRESULT r = desktop_manager()->IsWindowOnCurrentVirtualDesktop(handle, &is_current_desktop);
				if (r == S_OK && is_current_desktop != 0) {
					current_desktop_id = desktop_id;
				}
			}

			return TRUE;
		},
		0
	);

	for (auto& [_, d] : desktops) {
		d.delete_marked_windows();
	}

	erase_if(desktops, [](const auto& item) { return item.second.empty(); });
}

Desktop* Desktop::current() {
	return current_desktop_id.has_value() ? &desktops.at(current_desktop_id.value()) : nullptr;
}

Desktop* Desktop::get(HWND handle) {
	for (auto& [_, d] : desktops) {
		if (d.get_window(handle)) {
			return &d;
		}
	}

	return nullptr;
}

Desktop* Desktop::get(GUID id) {
	auto it = desktops.find(id);
	return it != desktops.end() ? &it->second : nullptr;
}

const Window* Window::focused() { return Window::get(GetForegroundWindow()); }

const Window* Window::get(HWND handle) {
	auto* desktop = Desktop::get(handle);
	return desktop ? desktop->get_window(handle) : nullptr;
}

const Window* Window::get_adjacent(Direction dir) const {
	auto* desktop = Desktop::get(m_handle);
	return desktop ? desktop->get_adjacent_window(m_handle, dir) : nullptr;
}

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

		// Ensure our information about desktops and their contained windows is as up-to-date as
		// possible before triggering a hotkey to minimize potential for erroneous behavior.
		Desktop::update_all();
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
					log_warning(format("Unknown message: {}", msg.message));
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

int main() {
	// Required for IVirtualDesktopManager
	CoInitialize(nullptr);

	// This app represents strings in UTF8 -- the following call makes sure
	// that non-ASCII characters print correctly in the terminal.
	SetConsoleOutputCP(CP_UTF8);

	// Reset the error state of the windows API such that later API calls don't
	// mistakenly get treated as having errored out.
	SetLastError(0);

	try {
		Hotkeys hotkeys;

		hotkeys.add("alt+h", []() { Window::focused()->get_adjacent(Direction::Left)->focus(); });
		hotkeys.add("alt+j", []() { Window::focused()->get_adjacent(Direction::Down)->focus(); });
		hotkeys.add("alt+k", []() { Window::focused()->get_adjacent(Direction::Up)->focus(); });
		hotkeys.add("alt+l", []() { Window::focused()->get_adjacent(Direction::Right)->focus(); });

		while (true) {
			hotkeys.check_for_triggers();
			this_thread::sleep_for(1ms);
		}
	} catch (const runtime_error& e) {
		log_error(format("Uncaught exception: {}", e.what()));
	}
}
