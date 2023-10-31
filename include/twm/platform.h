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
Rect get_window_rect(HWND handle);
void set_window_rect(HWND handle, const Rect& r);
std::string get_window_text(HWND handle);
bool terminate_process(HWND handle);
bool close_window(HWND handle);

std::optional<GUID> get_window_desktop_id(HWND handle);
bool is_window_on_current_desktop(HWND handle);

} // namespace twm
