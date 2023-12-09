// This file was developed by Thomas Müller <thomas94@gmx.net>.
// It is published under the GPU GPLv3 license; see LICENSE.txt for details.

#include <twm/common.h>
#include <twm/config.h>
#include <twm/hotkey.h>
#include <twm/logging.h>
#include <twm/math.h>
#include <twm/platform.h>

#include <chrono>
#include <fstream>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <variant>

// Saves so much typing
using namespace std;

namespace twm {

Config cfg = {};

class Window {
	string m_name = "";
	Rect m_rect = {};
	HWND m_handle = nullptr;
	clock::time_point m_last_interacted_time = {};
	bool m_marked_for_deletion = false;

	Window(HWND handle) : m_name{get_window_text(handle)}, m_rect{get_window_frame_bounds(handle)}, m_handle{handle} {}

	// Returns true if the name changed. Also apply global style
	// settings to the window.
	bool update(const Window& other) {
		update_border_color(focused() == this);
		set_window_rounded_corners(m_handle, RoundedCornerPreference::Disabled);

		string old_name = m_name;
		Rect old_rect = m_rect;
		m_name = other.m_name;
		m_rect = other.m_rect;
		m_marked_for_deletion = false;
		return m_name != old_name || m_rect != old_rect;
	}

	void mark_for_deletion() { m_marked_for_deletion = true; }
	bool marked_for_deletion() const { return m_marked_for_deletion; }
	void update_last_interacted_time() { m_last_interacted_time = clock::now(); }

public:
	friend class Desktop;

	static Window* focused();
	static bool focus_adjacent(Direction dir);
	static bool focus_adjacent_or_default(Direction dir);
	static bool swap_adjacent(Direction dir);
	static bool move_to_adjacent_desktop(Direction dir);
	static Window* get(HWND handle);

	Window* get_adjacent(Direction dir) const;

	bool focus() {
		Window* prev_focused = focused();
		if (!focus_window(m_handle)) {
			return false;
		}

		if (prev_focused) {
			prev_focused->update_border_color(false);
		}

		update_border_color(true);

		m_last_interacted_time = clock::now();
		return true;
	}

	void set_border_color(BorderColor color) { set_window_border_color(m_handle, color); }
	void set_rounded_corners(RoundedCornerPreference rounded) { set_window_rounded_corners(m_handle, rounded); }

	void update_border_color(bool is_focused) {
		if (cfg.draw_focus_border) {
			set_border_color(is_focused ? BorderColor::LightGray : BorderColor::DarkGray);
		} else {
			set_border_color(BorderColor::Default);
		}
	}

	auto last_focus_time() const { return m_last_interacted_time; }

	bool terminate() const { return terminate_process(m_handle); }
	bool close() const { return close_window(m_handle); }

	const string& name() const { return m_name; }
	const Rect& rect() const { return m_rect; }
	bool set_rect(const Rect& r) {
		if (!set_window_frame_bounds(m_handle, r)) {
			return false;
		}

		m_rect = r;
		return true;
	}

	HWND handle() const { return m_handle; }
};

struct BspNode {
	struct Children {
		unique_ptr<BspNode> left, right;
	};

	variant<HWND, Children> payload;
};

class Desktop {
	unordered_map<HWND, Window> m_windows = {};
	unique_ptr<BspNode> m_root = {};
	HWND m_last_focus = nullptr;
	GUID m_id = {};

	bool can_be_managed(const Window& w) {
		return !w.name().empty() && !IsIconic(w.handle()) && IsWindowVisible(w.handle());
	}

	bool try_manage(HWND handle, bool is_focused) {
		auto w = Window{handle};

		if (!can_be_managed(w)) {
			return false;
		}

		auto [it, inserted] = m_windows.insert({handle, w});
		if (!inserted) {
			it->second.update(w);
		}

		if (is_focused) {
			it->second.update_last_interacted_time();
			m_last_focus = handle;
		}

		return true;
	}

	void unmanage(HWND handle) { m_windows.erase(handle); }

	void pre_update() {
		for (auto& [_, w] : m_windows) {
			w.mark_for_deletion();
		}
	}

	void post_update() {
		erase_if(m_windows, [](const auto& item) { return item.second.marked_for_deletion(); });
		if (m_windows.count(m_last_focus) == 0) {
			m_last_focus = nullptr;
		}
	}

public:
	Desktop(const GUID& id) : m_id{id} {}

	Desktop(const Desktop& other) = delete;
	Desktop(Desktop&& other) = default;
	Desktop& operator=(const Desktop& other) = delete;
	Desktop& operator=(Desktop&& other) = default;

	static auto& all() {
		static unordered_map<GUID, Desktop> desktops = {};
		return desktops;
	}

	// ID of the desktop the user is currently looking at.
	static auto& current_id() {
		static optional<GUID> current_desktop_id = {};
		return current_desktop_id;
	}

	static void update_all() {
		current_id() = {};
		for (auto& [_, d] : all()) {
			d.pre_update();
		}

		HWND current_focus = GetForegroundWindow();
		EnumWindows(
			[](__in HWND handle, __in LPARAM param) {
				optional<GUID> opt_desktop_id = get_window_desktop_id(handle);
				if (!opt_desktop_id.has_value()) {
					// Window does not seem to belong to any desktop... can't be managed by this app.
					return TRUE;
				}

				GUID desktop_id = opt_desktop_id.value();

				// If the window's desktop already exists, query it. Otherwise, create
				// a new desktop object, keep track of it in `desktops`, and use that one.
				auto insert_result = all().insert({desktop_id, Desktop{desktop_id}});
				auto& desktop = insert_result.first->second;
				if (!desktop.try_manage(handle, handle == (HWND)param)) {
					// If the desktop can't manage the window, don't consider it as candidate for current desktop.
					return TRUE;
				}

				// The Windows API does not give us a direct way to query the currently active desktop, but it
				// allows us to check whether a given Window is on the current desktop. So if we find such a
				// window, we can deduce that its desktop's GUID is the currently active desktop.
				if (!current_id().has_value() && is_window_on_current_desktop(handle)) {
					current_id() = desktop_id;
				}

				return TRUE;
			},
			(LPARAM)current_focus
		);

		for (auto& [_, d] : all()) {
			d.post_update();
		}

		erase_if(all(), [](const auto& item) { return item.second.empty(); });
	}

	static Desktop* current() { return current_id().has_value() ? &all().at(current_id().value()) : nullptr; }

	static Desktop* get(HWND handle) {
		for (auto& [_, d] : all()) {
			if (d.get_window(handle)) {
				return &d;
			}
		}

		return nullptr;
	}

	static Desktop* get(GUID id) {
		auto it = all().find(id);
		return it != all().end() ? &it->second : nullptr;
	}

	static void focus_adjacent(Direction dir) {
		if (dir != Direction::Left && dir != Direction::Right) {
			throw runtime_error{"Desktops can only be focused left or right"};
		}

		// HACK HACK HACK: Windows does not provide an API to switch to adjacent
		// desktops, so we send the default hotkey combination for switching desktops
		// to the system. This has the potential for all sorts of breakage like keyboard
		// race conditions, conflicts with user-held keys, or changes in the shortcut.
		// In the future, we should probably use IVirtualDesktopManagerInternal, despite
		// it being an API that may break at any point...
		Hotkeys::send_to_system(format("ctrl-win-{}", dir == Direction::Left ? "left" : "right"));

		// After switching desktops, re-run a full update to ensure the current desktop
		// is correctly registered.
		Desktop::update_all();
	}

	Window* last_focus_or_default() {
		if (auto it = m_windows.find(m_last_focus); it != m_windows.end()) {
			return &it->second;
		}

		if (!m_windows.empty()) {
			return &m_windows.begin()->second;
		}

		return nullptr;
	}

	// Returns true if focus changed
	bool ensure_focus() {
		if (m_windows.count(GetForegroundWindow()) > 0) {
			return false;
		}

		auto* w = last_focus_or_default();
		return w && w->focus();
	}

	bool move_window_to_here(HWND handle) {
		if (!move_window_to_desktop(handle, m_id)) {
			return false;
		}

		if (auto* prev_desktop = Desktop::get(handle)) {
			prev_desktop->unmanage(handle);
		}

		return try_manage(handle, false);
	}

	Window* get_window(HWND handle) {
		auto it = m_windows.find(handle);
		return it != m_windows.end() ? &it->second : nullptr;
	}

	Window* get_adjacent_window(HWND handle, Direction dir) {
		auto* w = get_window(handle);
		if (!w) {
			return nullptr;
		}

		size_t axis = dir == Direction::Left || dir == Direction::Right ? 0 : 1;

		Window* best_candidate = nullptr;
		float best_distance = numeric_limits<float>::infinity();
		clock::time_point most_recently_interacted = {};

		float center = w->rect().center()[axis];

		for (auto& [_, ow] : m_windows) {
			float dist = w->rect().distance_with_axis_preference(axis, ow.rect());
			float in_axis_dist = center - ow.rect().center()[axis];

			float closeness_tolerance = 2;

			bool is_on_correct_side = abs(in_axis_dist) > closeness_tolerance &&
				(in_axis_dist > 0) == (dir == Direction::Up || dir == Direction::Left);

			bool is_among_closest_or_equally_close_and_more_recent = dist < best_distance - closeness_tolerance ||
				(abs(dist - best_distance) < closeness_tolerance && ow.last_focus_time() > most_recently_interacted);

			if (w != &ow && is_on_correct_side && is_among_closest_or_equally_close_and_more_recent) {
				best_distance = dist;
				most_recently_interacted = ow.last_focus_time();
				best_candidate = &ow;
			}
		}

		return best_candidate;
	}

	bool empty() const { return m_windows.empty(); }

	const GUID& id() const { return m_id; }

	void print() const {
		for (auto& [_, w] : m_windows) {
			log_info(w.name());
		}
	}
};

Window* Window::focused() { return Window::get(GetForegroundWindow()); }

bool Window::focus_adjacent(Direction dir) {
	if (auto* focused = Window::focused()) {
		if (auto* adj = focused->get_adjacent(dir)) {
			return adj->focus();
		}
	}

	return false;
}

bool Window::focus_adjacent_or_default(Direction dir) {
	if (focus_adjacent(dir)) {
		return true;
	}

	if (auto* d = Desktop::current()) {
		return d->ensure_focus();
	}

	return false;
}

bool Window::swap_adjacent(Direction dir) {
	if (auto* focused = Window::focused()) {
		if (auto* adj = focused->get_adjacent(dir)) {
			adj->update_last_interacted_time();
			focused->update_last_interacted_time();

			Rect tmp = focused->rect();

			bool success = true;
			success &= focused->set_rect(adj->rect());
			success &= adj->set_rect(tmp);
			return success;
		}
	}

	return false;
}

// The following is currently broken. Windows does not give permission
// to move windows that aren't owned by this process -- tough luck.
// TODO: map IVirtualDesktopManagerInternal and use it instead while
// dealing with potential breakage.
bool Window::move_to_adjacent_desktop(Direction dir) {
	if (auto* focused = Window::focused()) {
		HWND handle = focused->handle();
		Desktop::focus_adjacent(dir);
		if (auto* desktop = Desktop::current()) {
			if (desktop->move_window_to_here(handle)) {
				auto* window = desktop->get_window(handle);
				TWM_ASSERT(window);
				return window->focus();
			}
		}
	}

	return false;
}

Window* Window::get(HWND handle) {
	auto* desktop = Desktop::get(handle);
	return desktop ? desktop->get_window(handle) : nullptr;
}

Window* Window::get_adjacent(Direction dir) const {
	auto* desktop = Desktop::get(m_handle);
	return desktop ? desktop->get_adjacent_window(m_handle, dir) : nullptr;
}

enum class Action {
	Focus,
	Swap,
	MoveToDesktop,
	Close,
	Terminate,
	Reload,
};

Action to_action(string_view str) {
	auto lstr = to_lower(str);
	if (lstr == "focus") {
		return Action::Focus;
	} else if (lstr == "swap") {
		return Action::Swap;
	} else if (lstr == "move_to_desktop") {
		return Action::MoveToDesktop;
	} else if (lstr == "close") {
		return Action::Close;
	} else if (lstr == "terminate") {
		return Action::Terminate;
	} else if (lstr == "reload") {
		return Action::Reload;
	}

	throw runtime_error{format("Invalid action: {}", str)};
}

enum class Target {
	Window,
	Desktop,
};

Target to_target(string_view str) {
	auto lstr = to_lower(str);
	if (lstr == "window") {
		return Target::Window;
	} else if (lstr == "desktop") {
		return Target::Desktop;
	}

	throw runtime_error{format("Invalid target: {}", str)};
}

void save_config_to_appdata() {
	if (char* appdata = getenv("APPDATA")) {
		if (filesystem::exists(appdata)) {
			log_info("Saving config to {}\\twm\\twm.toml", appdata);

			auto config_dir = filesystem::path{appdata} / "twm";
			try {
				filesystem::create_directories(config_dir);
				auto config_path = filesystem::path{appdata} / "twm" / "twm.toml";
				ofstream f{config_path};
				cfg.save(f);
			} catch (const filesystem::filesystem_error& e) {
				log_error(format("Failed to save config: {}", e.what()));
			}
		}
	}
}

void reload() {
	// Try the following configs in order of priority:
	// 1. twm.toml in the current working directory
	// 2. TWM_CONFIG_PATH environment variable
	// 3. %APPDATA%\twm\twm.toml
	// 4. default config (and try to save it to %APPDATA%\twm\twm.toml)

	filesystem::path config_path = "twm.toml";
	if (!filesystem::exists(config_path)) {
		if (char* env_config_path = getenv("TWM_CONFIG_PATH")) {
			config_path = env_config_path;
		}
	}

	if (!filesystem::exists(config_path)) {
		if (char* appdata = getenv("APPDATA")) {
			config_path = filesystem::path{appdata} / "twm" / "twm.toml";
		}
	}

	if (!filesystem::exists(config_path)) {
		log_info("No config file found. Using default config.");
		cfg.load_default();
		// save_config_to_appdata();
		return;
	}

	log_info("Loading config from {}", config_path.string());
	cfg.load_from_file(config_path);

	if (cfg.disable_drop_shadows) {
		set_system_dropshadow(false);
	}
}

void invoke_action(string_view action) {
	log_debug(format("Invoking action: {}", action));

	auto parts = split(action, " ");
	if (parts.size() < 1) {
		throw runtime_error{"Invalid action. Must be of the form <focus|swap|move_to_desktop|close|terminate|reload>"};
	}

	auto action_type = to_action(parts[0]);
	switch (action_type) {
		case Action::Focus: {
			if (parts.size() != 3) {
				throw runtime_error{"Invalid focus. Syntax: focus <window|desktop> <top|bottom|left|right>"};
			}

			auto target = to_target(parts[1]);
			auto dir = to_direction(parts[2]);
			switch (target) {
				case Target::Window: Window::focus_adjacent_or_default(dir); break;
				case Target::Desktop: Desktop::focus_adjacent(dir); break;
			}
		} break;
		case Action::Swap: {
			if (parts.size() != 3) {
				throw runtime_error{"Invalid swap. Syntax: swap <window|desktop> <top|bottom|left|right>"};
			}

			auto target = to_target(parts[1]);
			auto dir = to_direction(parts[2]);
			switch (target) {
				case Target::Window: Window::swap_adjacent(dir); break;
				case Target::Desktop: throw runtime_error{"Cannot swap desktops"}; break;
			}
		} break;
		case Action::MoveToDesktop: {
			if (parts.size() != 3) {
				throw runtime_error{"Invalid move_to_desktop. Syntax: move_to_desktop <window|desktop> <left|right>"};
			}

			auto target = to_target(parts[1]);
			auto dir = to_direction(parts[2]);
			switch (target) {
				case Target::Window: Window::move_to_adjacent_desktop(dir); break;
				case Target::Desktop: throw runtime_error{"Cannot move desktops"}; break;
			}
		} break;
		case Action::Close: {
			if (parts.size() != 2) {
				throw runtime_error{"Invalid close. Syntax: close window"};
			}

			auto target = to_target(parts[1]);
			switch (target) {
				case Target::Window: {
					if (auto* w = Window::focused()) {
						w->close();
					}
				} break;
				case Target::Desktop: throw runtime_error{"Cannot close desktops"}; break;
			}
		} break;
		case Action::Terminate: {
			if (parts.size() != 2) {
				throw runtime_error{"Invalid terminate. Syntax: terminate window"};
			}

			auto target = to_target(parts[1]);
			switch (target) {
				case Target::Window: {
					if (auto* w = Window::focused()) {
						w->terminate();
					}
				} break;
				case Target::Desktop: throw runtime_error{"Cannot terminate desktops"}; break;
			}
		} break;
		case Action::Reload: reload(); break;
		default: throw runtime_error{format("Invalid action: {}", action)};
	}
}

void tick() {
	{
		static auto last_update = clock::now();

		auto now = clock::now();
		if (now - last_update > cfg.update_interval()) {
			Desktop::update_all();
			last_update = now;
		}
	}

	MSG msg = {};
	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE) != 0) {
		switch (msg.message) {
			case WM_HOTKEY: {
				// Ensure our information about desktops and their contained windows is as up-to-date as
				// possible before triggering a hotkey to minimize potential for erroneous behavior.
				Desktop::update_all();
				invoke_action(cfg.hotkeys.action_of((int)msg.wParam));
			} break;
			default: {
				log_debug(format("PeekMessage: unknown message ID {}", msg.message));
			} break;
		}
	}
}

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
		reload();

		while (true) {
			tick();
			this_thread::sleep_for(cfg.tick_interval());
		}
	} catch (const runtime_error& e) {
		log_error(format("Uncaught exception: {}", e.what()));
		return -1;
	}

	return 0;
}

} // namespace twm

int main() { return twm::main(); }
