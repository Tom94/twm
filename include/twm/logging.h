// This file was developed by Thomas Müller <thomas94@gmx.net>.
// It is published under the GPU GPLv3 license; see LICENSE.txt for details.

#pragma once

#include <format>
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

template <typename... Ts> requires(sizeof...(Ts) > 0) void log_format(Severity severity, const std::format_string<Ts...>& str, Ts&&... args) {
	log(severity, std::format(str, std::forward<Ts>(args)...));
}

template <typename... Ts> requires(sizeof...(Ts) > 0) void log_debug(const std::format_string<Ts...>& str, Ts&&... args) {
	log_format(Severity::Debug, str, std::forward<Ts>(args)...);
}

template <typename... Ts> requires(sizeof...(Ts) > 0) void log_info(const std::format_string<Ts...>& str, Ts&&... args) {
	log_format(Severity::Info, str, std::forward<Ts>(args)...);
}

template <typename... Ts> requires(sizeof...(Ts) > 0) void log_warning(const std::format_string<Ts...>& str, Ts&&... args) {
	log_format(Severity::Warning, str, std::forward<Ts>(args)...);
}

template <typename... Ts> requires(sizeof...(Ts) > 0) void log_error(const std::format_string<Ts...>& str, Ts&&... args) {
	log_format(Severity::Error, str, std::forward<Ts>(args)...);
}

} // namespace twm
