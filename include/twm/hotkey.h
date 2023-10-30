// This file was developed by Thomas Müller <thomas94@gmx.net>.
// It is published under the GPU GPLv3 license; see LICENSE.txt for details.

#pragma once

#include <functional>
#include <string>
#include <vector>

namespace twm {

using Callback = std::function<void()>;

struct Hotkey {
	int id;
	std::function<void()> cb;
};

class Hotkeys {
	std::vector<Hotkey> m_hotkeys;

	Hotkeys() {}

public:
	~Hotkeys() { clear(); }

	static auto& global() {
		static Hotkeys hotkeys;
		return hotkeys;
	}

	void add(const std::string& keycombo, Callback cb);
	void trigger(int id) const;
	void clear();
};

} // namespace twm
