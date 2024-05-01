// This file was developed by Thomas Müller <thomas94@gmx.net>.
// It is published under the GPU GPLv3 license; see LICENSE.txt for details.

#pragma once

#include <twm/common.h>
#include <twm/math.h>

#include <optional>
#include <string>

namespace twm {

int last_error_code();
std::string error_string(int code);
std::string last_error_string();

// On Windows, the size of a window can be expressed in
// multiple ways: either as a "rect" or as an "(extended)
// frame bound". The frame bound refers to what a user would
// think of the size of the window, whereas the rect also
// includes shadows and stylistic elements (i.e. tends to
// be a bit larger). In most situations, it is therefore
// recommended to use the *_window_frame_bounds functions
// and not the *_window_rect functions.
bool set_window_rect(HWND handle, const Rect& r);
Rect get_window_rect(HWND handle);
bool set_window_frame_bounds(HWND handle, const Rect& r);
Rect get_window_frame_bounds(HWND handle);

enum class RoundedCornerPreference {
	Default = 0,
	Disabled = 1,
	Enabled = 2,
};

void set_window_rounded_corners(HWND handle, RoundedCornerPreference rounded);

COLORREF to_colorref(uint32_t color);

enum class BorderColor : COLORREF {
	Black = 0x00000000,
	DarkGray = 0x00333333,
	Gray = 0x00666666,
	LightGray = 0x00999999,
	White = 0x00FFFFFF,
	Blue = 0x00ca7a0a,
	Green = 0x0000FF00,
	Red = 0x000000FF,
	Pink = 0x00B4549C,
	None = 0xFFFFFFFE,
	Default = 0xFFFFFFFF,
};

void set_window_border_color(HWND handle, COLORREF color);
void set_system_dropshadow(bool enabled);

bool focus_window(HWND handle); // returns false if the window could not be focused

std::string get_window_text(HWND handle);
bool terminate_process(HWND handle);
bool close_window(HWND handle);

std::optional<GUID> get_window_desktop_id(HWND handle);
bool is_window_on_current_desktop(HWND handle);
bool move_window_to_desktop(HWND handle, const GUID& desktop_id);

} // namespace twm
