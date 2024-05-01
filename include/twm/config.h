// This file was developed by Thomas Müller <thomas94@gmx.net>.
// It is published under the GPU GPLv3 license; see LICENSE.txt for details.

#pragma once

#include <twm/common.h>
#include <twm/hotkey.h>

#include <chrono>
#include <filesystem>

namespace twm {

struct Config {
	float tick_interval_seconds = 0.005f;
	float update_interval_seconds = 0.1f;
	bool disable_drop_shadows = false;
	bool disable_rounded_corners = false;
	bool draw_focus_border = false;
	uint32_t focused_border_color = 0x999999;
	uint32_t unfocused_border_color = 0x333333;
	Hotkeys hotkeys;

	void load_default();
	void load_from_file(const std::filesystem::path& path);
	void load_from_string(std::string_view path);
	void save(std::ostream& out) const;

	clock::duration tick_interval() const { return std::chrono::duration_cast<clock::duration>(std::chrono::duration<float>(tick_interval_seconds)); }
	clock::duration update_interval() const { return std::chrono::duration_cast<clock::duration>(std::chrono::duration<float>(update_interval_seconds)); }
};

} // namespace twm
