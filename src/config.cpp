// This file was developed by Thomas Müller <thomas94@gmx.net>.
// It is published under the GPU GPLv3 license; see LICENSE.txt for details.

#include <twm/config.h>

#include <toml++/toml.hpp>

#include <fstream>

using namespace std;

namespace twm {

void load_cfg(Config& cfg, const toml::table& file) {
	cfg.tick_interval_seconds = file["tick_interval_seconds"].value_or(cfg.tick_interval_seconds);
	cfg.update_interval_seconds = file["update_interval_seconds"].value_or(cfg.update_interval_seconds);
	cfg.disable_drop_shadows = file["disable_drop_shadows"].value_or(cfg.disable_drop_shadows);
	cfg.disable_rounded_corners = file["disable_rounded_corners"].value_or(cfg.disable_rounded_corners);
	cfg.draw_focus_border = file["draw_focus_border"].value_or(cfg.draw_focus_border);

	auto read_color = [](const auto& v) -> optional<uint32_t> {
		if (auto osv = v.template value<string_view>()) {
			string_view sv = ltrim(trim(*osv), "#");
			if (sv.starts_with("0x") || sv.starts_with("0X")) {
				sv.remove_prefix(2);
			}

			if (sv.size() != 6) {
				return nullopt;
			}

			uint32_t color = 0;
			for (char c : sv) {
				color <<= 4;
				if (c >= '0' && c <= '9') {
					color += c - '0';
				} else if (c >= 'a' && c <= 'f') {
					color += c - 'a' + 10;
				} else if (c >= 'A' && c <= 'F') {
					color += c - 'A' + 10;
				} else {
					return nullopt;
				}
			}

			return color;
		} else if (auto num = v.template value<uint32_t>()) {
			return *num;
		}

		return nullopt;
	};

	cfg.focused_border_color = read_color(file["focused_border_color"]).value_or(cfg.focused_border_color);
	cfg.unfocused_border_color = read_color(file["unfocused_border_color"]).value_or(cfg.unfocused_border_color);

	if (auto hotkeys = file["hotkeys"]) {
		cfg.hotkeys.clear();
		for (auto hotkey : *hotkeys.as_table()) {
			if (auto action = hotkey.second.as_string()) {
				cfg.hotkeys.add(hotkey.first.str(), **action);
			}
		}
	}
}

void Config::load_default() {
	load_from_string(R"(
		[hotkeys]
		alt-left = "focus window left"
		alt-down = "focus window down"
		alt-up = "focus window up"
		alt-right = "focus window right"

		alt-shift-left = "swap window left"
		alt-shift-down = "swap window down"
		alt-shift-up = "swap window up"
		alt-shift-right = "swap window right"

		alt-h = "focus window left"
		alt-j = "focus window down"
		alt-k = "focus window up"
		alt-l = "focus window right"

		alt-shift-h = "swap window left"
		alt-shift-j = "swap window down"
		alt-shift-k = "swap window up"
		alt-shift-l = "swap window right"

		alt-1 = "focus desktop left"
		alt-2 = "focus desktop right"

		alt-shift-q = "close window"
		ctrl-alt-shift-q = "terminate window"

		alt-shift-r = "reload"
	)");
}

void Config::load_from_file(const filesystem::path& path) {
	ifstream f{path};
	load_cfg(*this, toml::parse(f, path.string()));
}

void Config::load_from_string(string_view content) {
	load_cfg(*this, toml::parse(content));
}

void Config::save(ostream& out) const {
	auto file = toml::table{
		{"tick_interval_seconds", tick_interval_seconds},
		{"update_interval_seconds", update_interval_seconds},
		{"disable_drop_shadows", disable_drop_shadows},
		{"disable_rounded_corners", disable_rounded_corners},
		{"draw_focus_border", draw_focus_border},
		{"focused_border_color", focused_border_color},
		{"unfocused_border_color", unfocused_border_color},
	};

	toml::table hotkeys_table;
	for (const auto& hotkey : hotkeys.hotkeys()) {
		hotkeys_table.insert(hotkey.keycombo, hotkey.action);
	}

	file.insert("hotkeys", hotkeys_table);

	out << file;
}

}
