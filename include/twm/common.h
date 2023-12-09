// This file was developed by Thomas Müller <thomas94@gmx.net>.
// It is published under the GPU GPLv3 license; see LICENSE.txt for details.

#pragma once

#include <chrono>
#include <functional>
#include <sstream>
#include <string>

#define NOMINMAX
#include <ShlObj.h>
#include <Windows.h>
#ifdef far
#	undef far
#	undef near
#endif

// hash and equality implementations for Windows API's GUID type to make it useable as hash map key.
namespace std {
template <> struct hash<GUID> {
	size_t operator()(const GUID& x) const {
		return (size_t)x.Data1 * 73856093 + (size_t)x.Data2 * 19349663 + (size_t)x.Data3 * 83492791 +
			*(uint64_t*)x.Data4 * 25165843;
	}
};
template <> struct equal_to<GUID> {
	size_t operator()(const GUID& a, const GUID& b) const {
		return a.Data1 == b.Data1 && a.Data2 == b.Data2 && a.Data3 == b.Data3 &&
			*(uint64_t*)a.Data4 == *(uint64_t*)b.Data4;
	}
};
} // namespace std

namespace twm {

using clock = std::chrono::steady_clock;

#define STRINGIFY(x) #x
#define STR(x) STRINGIFY(x)
#define FILE_LINE __FILE__ ":" STR(__LINE__)
#define TWM_ASSERT(x)                                             \
	do {                                                          \
		if (!(x)) {                                               \
			throw std::runtime_error{FILE_LINE " " #x " failed"}; \
		}                                                         \
	} while (0);

class ScopeGuard {
public:
	ScopeGuard() = default;
	ScopeGuard(const std::function<void()>& callback) : m_callback{callback} {}
	ScopeGuard(std::function<void()>&& callback) : m_callback{std::move(callback)} {}
	ScopeGuard& operator=(const ScopeGuard& other) = delete;
	ScopeGuard(const ScopeGuard& other) = delete;
	ScopeGuard& operator=(ScopeGuard&& other) {
		swap(m_callback, other.m_callback);
		return *this;
	}

	ScopeGuard(ScopeGuard&& other) { *this = std::move(other); }
	~ScopeGuard() {
		if (m_callback) {
			m_callback();
		}
	}

	void disarm() { m_callback = {}; }

private:
	std::function<void()> m_callback;
};

std::string utf16_to_utf8(const std::wstring& utf16);
std::wstring utf8_to_utf16(const std::string& utf8);
std::string to_lower(std::string str);
std::string ltrim(std::string s);
std::string rtrim(std::string s);
std::string trim(std::string s);

std::vector<std::string> split(std::string text, const std::string& delim);

template <typename T> std::string join(const T& components, const std::string& delim) {
	std::ostringstream s;
	for (const auto& component : components) {
		if (&components[0] != &component) {
			s << delim;
		}

		s << component;
	}

	return s.str();
}

enum class Direction {
	Up,
	Down,
	Left,
	Right,
};

Direction opposite(Direction dir);
std::string to_string(Direction dir);
Direction from_string(const std::string& str);

} // namespace twm
