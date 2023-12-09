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
		cfg.must_focus = file["must_focus"].value_or(cfg.must_focus);

		if (auto hotkeys = file["hotkeys"]) {
			cfg.hotkeys.clear();

			for (auto hotkey : *hotkeys.as_table()) {
				if (auto action = hotkey.second.as_string()) {
					cfg.hotkeys.add(hotkey.first.str(), **action);
				}
			}
		}
	}

	void Config::load_from_file(const filesystem::path& path) {
		ifstream f{path};
		load_cfg(*this, toml::parse(f, path.string()));
	}

	void Config::load_from_string(string_view content) {
		load_cfg(*this, toml::parse(content));
	}
}
