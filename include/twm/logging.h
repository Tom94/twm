// This file was developed by Thomas Müller <thomas94@gmx.net>.
// It is published under the GPU GPLv3 license; see LICENSE.txt for details.

#pragma once

#include <string>
#include <type_traits>

namespace twm {

enum class Severity : uint8_t {
	Debug,
	Info,
	Warning,
	Error,
};

void log(Severity severity, const std::string& str);

void log_debug(const std::string& str);
void log_info(const std::string& str);
void log_warning(const std::string& str);
void log_error(const std::string& str);

} // namespace twm
