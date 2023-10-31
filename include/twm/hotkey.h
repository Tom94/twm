// This file was developed by Thomas Müller <thomas94@gmx.net>.
// It is published under the GPU GPLv3 license; see LICENSE.txt for details.

#pragma once

#include <functional>
#include <string>
#include <vector>

namespace twm {

using Callback = std::function<void()>;

enum class SendMode {
	PressAndRelease,
	Press,
	Release,
};

struct Hotkey {
	int id;
	std::function<void()> cb;
	std::string keycombo;
};

class Hotkeys {
	std::vector<Hotkey> m_hotkeys;

	Hotkeys() {}

public:
	~Hotkeys() { clear(); }

	// Sends the given keycombo to the system. Useful to trigger
	// system-wide shortcuts for functionality that is not exposed
	// by the Windows API (e.g. for switching virtual desktops).
	static void send(const std::string& keycombo, SendMode mode = SendMode::PressAndRelease);

	static auto& global() {
		static Hotkeys hotkeys;
		return hotkeys;
	}

	void add(const std::string& keycombo, Callback cb);
	void trigger(int id) const;
	void clear();
};

} // namespace twm
