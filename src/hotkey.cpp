// This file was developed by Thomas Müller <thomas94@gmx.net>.
// It is published under the GPU GPLv3 license; see LICENSE.txt for details.

#include <twm/common.h>
#include <twm/hotkey.h>
#include <twm/platform.h>

#include <format>
#include <iostream>
#include <string>

using namespace std;

namespace twm {

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

void Hotkeys::add(const string& keycombo, Callback cb) {
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
			throw runtime_error{format("Error registering {}: duplicate keycode {}", keycombo, keycode)};
		}

		if (string_to_keycode.count(name) > 0) {
			keycode = string_to_keycode[name];
			continue;
		}

		if (name.size() != 1) {
			throw runtime_error{format("Error registering {}: unknown keycode {}", keycombo, keycode)};
		}

		keycode = (char)toupper(name[0]);
	}

	if (RegisterHotKey(nullptr, id, mod, keycode) == 0) {
		throw runtime_error{format("Error registering {}: {}", keycombo, last_error_string())};
	}

	m_hotkeys.emplace_back(id, cb);
}

void Hotkeys::trigger(int id) const {
	if (id < 0 || id >= (int)m_hotkeys.size()) {
		throw runtime_error{"Invalid hotkey id"};
	}

	m_hotkeys[id].cb();
}

void Hotkeys::clear() {
	for (size_t i = 0; i < m_hotkeys.size(); ++i) {
		// We do not care about errors in the unregistering process here.
		// Simply try to unbind all hotkeys and hope for the best -- there
		// is nothing we can do if unbinding fails.
		UnregisterHotKey(nullptr, (int)i);
	}

	m_hotkeys.clear();
}

} // namespace twm
