// This file was developed by Thomas Müller <thomas94@gmx.net>.
// It is published under the GPU GPLv3 license; see LICENSE.txt for details.

#pragma once

#include <twm/hotkey.h>

#include <chrono>
#include <filesystem>

namespace twm {

struct Config {
	float tick_interval_seconds = 0.005f;
	float update_interval_seconds = 0.1f;
	bool must_focus = true;
	Hotkeys hotkeys;

	void load(const std::filesystem::path& path);

	clock::duration tick_interval() const { return std::chrono::duration_cast<clock::duration>(std::chrono::duration<float>(tick_interval_seconds)); }
	clock::duration update_interval() const { return std::chrono::duration_cast<clock::duration>(std::chrono::duration<float>(update_interval_seconds)); }
};

} // namespace twm
