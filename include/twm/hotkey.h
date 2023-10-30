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

public:
	~Hotkeys() { clear(); }

	void add(const std::string& keycombo, Callback cb);
	void trigger(int id) const;
	void clear();
};

} // namespace twm
