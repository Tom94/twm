// This file was developed by Thomas Müller <thomas94@gmx.net>.
// It is published under the GPU GPLv3 license; see LICENSE.txt for details.

#pragma once

#include <string>
#include <vector>

namespace twm {

enum class SendMode {
	PressAndRelease,
	Press,
	Release,
};

struct Hotkey {
	int id;
	std::string action;
	std::string keycombo;
};

class Hotkeys {
	std::vector<Hotkey> m_hotkeys;

public:
	~Hotkeys() { clear(); }

	// Sends the given keycombo to the system. Useful to trigger
	// system-wide shortcuts for functionality that is not exposed
	// by the Windows API (e.g. for switching virtual desktops).
	static void send_to_system(const std::string& keycombo, SendMode mode = SendMode::PressAndRelease);

	void add(std::string_view keycombo, std::string_view action);
	std::string_view action_of(int id) const;
	void clear();

	const std::vector<Hotkey>& hotkeys() const { return m_hotkeys; }
};

} // namespace twm
