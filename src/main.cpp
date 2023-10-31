// This file was developed by Thomas Müller <thomas94@gmx.net>.
// It is published under the GPU GPLv3 license; see LICENSE.txt for details.

#include <twm/common.h>
#include <twm/hotkey.h>
#include <twm/logging.h>
#include <twm/math.h>
#include <twm/platform.h>

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

using clock = chrono::steady_clock;

enum class Direction {
	Up,
	Down,
	Left,
	Right,
};

class Window {
	string m_name = "";
	Rect m_rect = {};
	HWND m_handle = nullptr;
	clock::time_point m_last_interacted_time = {};
	GUID m_desktop_id = {};

	bool m_marked_for_deletion = false;

	Window(HWND handle, const GUID& desktop_id) :
		m_name{get_window_text(handle)}, m_rect{get_window_frame_bounds(handle)}, m_handle{handle}, m_desktop_id{desktop_id} {}

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
	void update_last_interacted_time() { m_last_interacted_time = clock::now(); }

public:
	friend class Desktop;

	static Window* focused();
	static bool focus_adjacent(Direction dir);
	static bool focus_adjacent_or_default(Direction dir);
	static void swap_adjacent(Direction dir);
	static Window* get(HWND handle);

	Window* get_adjacent(Direction dir) const;

	bool focus() {
		bool success = SetForegroundWindow(m_handle) != 0;
		if (success) {
			m_last_interacted_time = clock::now();
		}

		return success;
	}

	auto last_focus_time() const { return m_last_interacted_time; }

	bool terminate() const { return terminate_process(m_handle); }
	bool close() const { return close_window(m_handle); }

	const string& name() const { return m_name; }
	const Rect& rect() const { return m_rect; }
	void set_rect(const Rect& r) {
		set_window_frame_bounds(m_handle, r);
		m_rect = r;
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
		auto w = Window{handle, m_id};

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

	const GUID& id() const { return m_id; }

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

			bool is_on_correct_side = (center - ow.rect().center()[axis] > 0) ==
				(dir == Direction::Up || dir == Direction::Left);

			bool is_closest_or_equally_close_and_more_recent = dist < best_distance ||
				(dist == best_distance && ow.last_focus_time() > most_recently_interacted);

			if (w != &ow && is_on_correct_side && is_closest_or_equally_close_and_more_recent) {
				best_distance = dist;
				most_recently_interacted = ow.last_focus_time();
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

void Window::swap_adjacent(Direction dir) {
	if (auto* focused = Window::focused()) {
		if (auto* adj = focused->get_adjacent(dir)) {
			adj->update_last_interacted_time();
			focused->update_last_interacted_time();

			Rect tmp = focused->rect();
			focused->set_rect(adj->rect());
			adj->set_rect(tmp);
		}
	}
}

Window* Window::get(HWND handle) {
	auto* desktop = Desktop::get(handle);
	return desktop ? desktop->get_window(handle) : nullptr;
}

Window* Window::get_adjacent(Direction dir) const {
	auto* desktop = Desktop::get(m_handle);
	return desktop ? desktop->get_adjacent_window(m_handle, dir) : nullptr;
}

struct Config {
	clock::duration tick_interval = 5ms;
	clock::duration update_interval = 100ms;
	bool must_focus = true;
};

void tick(const Config& cfg) {
	{
		static auto last_update = clock::now();

		auto now = clock::now();
		if (now - last_update > cfg.update_interval) {
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
				Hotkeys::global().trigger((int)msg.wParam);
			} break;
			default: {
				log_warning(format("Unknown message: {}", msg.message));
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
		Config cfg = {};

		Hotkeys::global().add("alt+h", []() { Window::focus_adjacent_or_default(Direction::Left); });
		Hotkeys::global().add("alt+j", []() { Window::focus_adjacent_or_default(Direction::Down); });
		Hotkeys::global().add("alt+k", []() { Window::focus_adjacent_or_default(Direction::Up); });
		Hotkeys::global().add("alt+l", []() { Window::focus_adjacent_or_default(Direction::Right); });

		Hotkeys::global().add("alt+shift+h", []() { Window::swap_adjacent(Direction::Left); });
		Hotkeys::global().add("alt+shift+j", []() { Window::swap_adjacent(Direction::Down); });
		Hotkeys::global().add("alt+shift+k", []() { Window::swap_adjacent(Direction::Up); });
		Hotkeys::global().add("alt+shift+l", []() { Window::swap_adjacent(Direction::Right); });

		Hotkeys::global().add("alt+shift+q", []() {
			if (auto* w = Window::focused()) {
				w->close();
			}
		});

		Hotkeys::global().add("ctrl+alt+shift+q", []() {
			if (auto* w = Window::focused()) {
				w->terminate();
			}
		});

		while (true) {
			tick(cfg);
			this_thread::sleep_for(cfg.tick_interval);
		}
	} catch (const runtime_error& e) {
		log_error(format("Uncaught exception: {}", e.what()));
		return -1;
	}

	return 0;
}

} // namespace twm

int main() { return twm::main(); }
