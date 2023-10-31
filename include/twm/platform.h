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

std::string get_window_text(HWND handle);
bool terminate_process(HWND handle);
bool close_window(HWND handle);

std::optional<GUID> get_window_desktop_id(HWND handle);
bool is_window_on_current_desktop(HWND handle);
bool move_window_to_desktop(HWND handle, const GUID& desktop_id);

} // namespace twm
