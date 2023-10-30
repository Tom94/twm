// This file was developed by Thomas Müller <thomas94@gmx.net>.
// It is published under the GPU GPLv3 license; see LICENSE.txt for details.

#include <twm/common.h>
#include <twm/logging.h>
#include <twm/math.h>

#include <algorithm>
#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <variant>

// Saves so much typing
using namespace std;

namespace twm {

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
	static void focus_adjacent(Direction dir);
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

		const Window* best_candidate = nullptr;
		float best_distance = numeric_limits<float>::infinity();

		float center = w->rect().center()[axis];

		for (auto& [_, ow] : m_windows) {
			float dist = w->rect().distance_with_axis_preference(axis, ow.rect());
			bool is_on_correct_side = (center - ow.rect().center()[axis] > 0) ==
				(dir == Direction::Up || dir == Direction::Left);

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

void Window::focus_adjacent(Direction dir) {
	if (auto* focused = Window::focused()) {
		if (auto* adj = focused->get_adjacent(dir)) {
			adj->focus();
		}
	}
}

const Window* Window::get(HWND handle) {
	auto* desktop = Desktop::get(handle);
	return desktop ? desktop->get_window(handle) : nullptr;
}

const Window* Window::get_adjacent(Direction dir) const {
	auto* desktop = Desktop::get(m_handle);
	return desktop ? desktop->get_adjacent_window(m_handle, dir) : nullptr;
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

} // namespace twm

int main() {
	using namespace twm;

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

		hotkeys.add("alt+h", []() { Window::focus_adjacent(Direction::Left); });
		hotkeys.add("alt+j", []() { Window::focus_adjacent(Direction::Down); });
		hotkeys.add("alt+k", []() { Window::focus_adjacent(Direction::Up); });
		hotkeys.add("alt+l", []() { Window::focus_adjacent(Direction::Right); });

		while (true) {
			hotkeys.check_for_triggers();
			this_thread::sleep_for(1ms);
		}
	} catch (const runtime_error& e) {
		log_error(format("Uncaught exception: {}", e.what()));
	}
}
