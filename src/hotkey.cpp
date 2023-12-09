// This file was developed by Thomas Müller <thomas94@gmx.net>.
// It is published under the GPU GPLv3 license; see LICENSE.txt for details.

#include <twm/common.h>
#include <twm/hotkey.h>
#include <twm/logging.h>
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

// Modifiert also have VK keycodes. Those should not be
// used in RegisterHotKey, but _should_ be used in SendInputs.
unordered_map<string, UINT> string_to_keycode = {
	{"up",        VK_UP     },
	{"down",      VK_DOWN   },
	{"left",      VK_LEFT   },
	{"right",     VK_RIGHT  },
	{"back",      VK_BACK   },
	{"backspace", VK_BACK   },
	{"tab",       VK_TAB    },
	{"return",    VK_RETURN },
	{"enter",     VK_RETURN },
	{"escape",    VK_ESCAPE },
	{"esc",       VK_ESCAPE },
	{"space",     VK_SPACE  },
	{"ctrl",      VK_CONTROL},
	{"control",   VK_CONTROL},
	{"alt",       VK_MENU   },
	{"super",     VK_LWIN   },
	{"win",       VK_LWIN   },
	{"shift",     VK_SHIFT  },
};

vector<INPUT> keys_to_inputs(const string& keycombo, SendMode mode) {
	vector<INPUT> inputs;
	for (const auto& part : split(keycombo, "+")) {
		auto name = to_lower(trim(part));

		INPUT in = {};
		in.type = INPUT_KEYBOARD;

		if (auto it = string_to_keycode.find(name); it != string_to_keycode.end()) {
			in.ki.wVk = (WORD)it->second;
		} else {
			in.ki.wVk = (char)toupper(name[0]);
		}

		in.ki.dwFlags = mode == SendMode::Release ? KEYEVENTF_KEYUP : 0;
		inputs.emplace_back(in);
	}

	if (mode == SendMode::PressAndRelease) {
		size_t n_keys = inputs.size();
		for (size_t i = 0; i < n_keys; ++i) {
			INPUT in = inputs[n_keys - i - 1];
			in.ki.dwFlags = KEYEVENTF_KEYUP;
			inputs.emplace_back(in);
		}
	}

	return inputs;
}

UINT held_mods() {
	UINT result = 0;
	if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
		result |= MOD_CONTROL;
	}

	if (GetAsyncKeyState(VK_MENU) & 0x8000) {
		result |= MOD_ALT;
	}

	if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
		result |= MOD_SHIFT;
	}

	if (GetAsyncKeyState(VK_LWIN) & 0x8000) {
		result |= MOD_WIN;
	}

	return result;
}

vector<INPUT> mods_to_inputs(UINT mods, SendMode mode) {
	vector<string> mod_names;
	if (mods & MOD_ALT) {
		mod_names.emplace_back("alt");
	}

	if (mods & MOD_CONTROL) {
		mod_names.emplace_back("ctrl");
	}

	if (mods & MOD_WIN) {
		mod_names.emplace_back("win");
	}

	if (mods & MOD_SHIFT) {
		mod_names.emplace_back("shift");
	}

	return keys_to_inputs(join(mod_names, "+"), mode);
}

void Hotkeys::send_to_system(const string& keycombo, SendMode mode) {
	// Since the hotkey might invoke other system-wide hotkeys underneath,
	// e.g. to access functionality that is not exposed by the Windows API
	// such as virtual desktop switching, let us temporarily let go of any
	// currently held hotkeys to avoid interference.
	uint32_t mods = held_mods();
	auto inputs = mods_to_inputs(mods, SendMode::Release);
	auto tmp = keys_to_inputs(keycombo, mode);
	inputs.insert(inputs.end(), tmp.begin(), tmp.end());
	tmp = mods_to_inputs(mods, SendMode::Press);
	inputs.insert(inputs.end(), tmp.begin(), tmp.end());

	if (UINT n_sent = SendInput((UINT)inputs.size(), inputs.data(), sizeof(INPUT)); n_sent != inputs.size()) {
		throw runtime_error{format("SendInput failed: {}", last_error_string())};
	}
}

void Hotkeys::add(const string& keycombo, Callback cb) {
	int id = (int)m_hotkeys.size();
	auto parts = split(keycombo, "+");
	UINT mod = 0;
	UINT keycode = 0;

	// keycombo is of the form mod1+mod2+...+keycode
	// case insensitive and with optional spaces. Parse below.
	for (const auto& part : parts) {
		auto name = to_lower(trim(part));

		if (auto it = string_to_modifier.find(name); it != string_to_modifier.end()) {
			mod |= it->second;
			continue;
		}

		// If none of the above modifiers is given, that means we are given a keycode.
		// Only one keycode per keybinding is allowed -- check for this.
		if (keycode != 0) {
			throw runtime_error{format("Error registering {}: duplicate keycode {}", keycombo, keycode)};
		}

		if (auto it = string_to_keycode.find(name); it != string_to_keycode.end()) {
			keycode = it->second;
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

	m_hotkeys.emplace_back(id, cb, keycombo);
}

void Hotkeys::trigger(int id) const {
	if (id < 0 || id >= (int)m_hotkeys.size()) {
		throw runtime_error{"Invalid hotkey id"};
	}

	const Hotkey& hk = m_hotkeys[id];
	hk.cb();
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
