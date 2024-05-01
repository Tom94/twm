// This file was developed by Thomas Müller <thomas94@gmx.net>.
// It is published under the GPU GPLv3 license; see LICENSE.txt for details.

#include <twm/common.h>

#include <algorithm>
#include <format>

using namespace std;

namespace twm {

// Convenience string processing functions
string utf16_to_utf8(const wstring& utf16) {
	string utf8;
	if (!utf16.empty()) {
		int size = WideCharToMultiByte(CP_UTF8, 0, &utf16[0], (int)utf16.size(), NULL, 0, NULL, NULL);
		utf8.resize(size, 0);
		WideCharToMultiByte(CP_UTF8, 0, &utf16[0], (int)utf16.size(), &utf8[0], size, NULL, NULL);
	}
	return utf8;
}

wstring utf8_to_utf16(const string& utf8) {
	wstring utf16;
	if (!utf8.empty()) {
		int size = MultiByteToWideChar(CP_UTF8, 0, &utf8[0], (int)utf8.size(), NULL, 0);
		utf16.resize(size, 0);
		MultiByteToWideChar(CP_UTF8, 0, &utf8[0], (int)utf8.size(), &utf16[0], size);
	}
	return utf16;
}

string to_lower(string_view str) {
	std::string result{str};
	transform(begin(result), end(result), begin(result), [](unsigned char c) { return (char)tolower(c); });
	return result;
}

string_view ltrim(string_view s, const string_view& chars) {
	auto pos = s.find_first_not_of(chars);
	if (pos == string_view::npos) {
		return {};
	}

	s.remove_prefix(pos);
	return s;
}

string_view rtrim(string_view s, const string_view& chars) {
	auto pos = s.find_last_not_of(chars);
	if (pos == string_view::npos) {
		return {};
	}

	s.remove_suffix(s.size() - pos - 1);
	return s;
}

string_view trim(string_view s, const string_view& chars) { return ltrim(rtrim(s, chars), chars); }

vector<string> split(string_view text, const string& delim) {
	vector<string> result;
	size_t begin = 0;
	while (true) {
		size_t end = text.find_first_of(delim, begin);
		if (end == string::npos) {
			result.emplace_back(text.substr(begin));
			return result;
		} else {
			result.emplace_back(text.substr(begin, end - begin));
			begin = end + 1;
		}
	}

	return result;
}

Direction opposite(Direction dir) {
	switch (dir) {
		case Direction::Up: return Direction::Down;
		case Direction::Down: return Direction::Up;
		case Direction::Left: return Direction::Right;
		case Direction::Right: return Direction::Left;
		default: throw runtime_error{"opposite: invalid dir"};
	}
}

std::string to_string(Direction dir) {
	switch (dir) {
		case Direction::Up: return "up";
		case Direction::Down: return "down";
		case Direction::Left: return "left";
		case Direction::Right: return "right";
		default: throw runtime_error{"to_string: invalid dir"};
	}
}

Direction to_direction(const std::string& str) {
	std::string lstr = to_lower(str);
	if (lstr == "up") {
		return Direction::Up;
	} else if (lstr == "down") {
		return Direction::Down;
	} else if (lstr == "left") {
		return Direction::Left;
	} else if (lstr == "right") {
		return Direction::Right;
	}

	throw runtime_error{format("to_direction: invalid dir", str)};
}

} // namespace twm
