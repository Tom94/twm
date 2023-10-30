// This file was developed by Thomas Müller <thomas94@gmx.net>.
// It is published under the GPU GPLv3 license; see LICENSE.txt for details.

#pragma once

#include <twm/common.h>

#include <cmath>
#include <format>
#include <string_view>

namespace twm {

struct Vec2 {
	float x, y;

	Vec2(float c = 0.0f) : Vec2(c, c) {}
	Vec2(float x, float y) : x{x}, y{y} {}

	static Vec2 zero() { return Vec2{0.0f}; }
	static Vec2 ones() { return Vec2{1.0f}; }

	Vec2& operator/=(float other) { return *this = *this / other; }
	Vec2& operator*=(float other) { return *this = *this * other; }
	Vec2& operator-=(float other) { return *this = *this - other; }
	Vec2& operator+=(float other) { return *this = *this + other; }

	Vec2 operator/(float other) const { return {x / other, y / other}; }
	Vec2 operator*(float other) const { return {x * other, y * other}; }
	Vec2 operator-(float other) const { return {x - other, y - other}; }
	Vec2 operator+(float other) const { return {x + other, y + other}; }

	Vec2& operator/=(const Vec2& other) { return *this = *this / other; }
	Vec2& operator*=(const Vec2& other) { return *this = *this * other; }
	Vec2& operator-=(const Vec2& other) { return *this = *this - other; }
	Vec2& operator+=(const Vec2& other) { return *this = *this + other; }

	Vec2 operator/(const Vec2& other) const { return {x / other.x, y / other.y}; }
	Vec2 operator*(const Vec2& other) const { return {x * other.x, y * other.y}; }
	Vec2 operator-(const Vec2& other) const { return {x - other.x, y - other.y}; }
	Vec2 operator+(const Vec2& other) const { return {x + other.x, y + other.y}; }

	bool operator==(const Vec2& other) const { return x == other.x && y == other.y; }
	bool operator!=(const Vec2& other) const { return x != other.x || y != other.y; }

	float length_sq() const { return x * x + y * y; }
	float length() const { return sqrt(length_sq()); }
	float prod() const { return x * y; }
	float sum() const { return x + y; }
	float max() const { return fmax(x, y); }
	float min() const { return fmin(x, y); }
	int max_axis() const { return x > y ? 0 : 1; }
	int min_axis() const { return x > y ? 1 : 0; }

	float operator[](size_t idx) const {
		TWM_ASSERT(idx == 0 || idx == 1);
		return idx == 0 ? x : y;
	}
};

struct Rect {
	Vec2 top_left = {};
	Vec2 bottom_right = {};

	Rect() = default;
	Rect(const RECT& r) :
		top_left{static_cast<float>(r.left), static_cast<float>(r.top)},
		bottom_right{static_cast<float>(r.right), static_cast<float>(r.bottom)} {}

	bool operator==(const Rect& other) const {
		return top_left == other.top_left && bottom_right == other.bottom_right;
	}

	bool operator!=(const Rect& other) const {
		return top_left != other.top_left || bottom_right != other.bottom_right;
	}

	float distance_with_axis_preference(size_t axis, const Rect& other) const {
		auto off_axis = (axis + 1) % 2;
		auto c = center();
		auto oc = other.center();
		return abs(c[axis] - oc[axis]) + fmax(0.0f, abs(c[off_axis] - oc[off_axis]) - size()[off_axis] / 2);
	}

	Vec2 center() const { return (top_left + bottom_right) / 2.0f; }
	Vec2 size() const { return bottom_right - top_left; }
	Vec2 area() const { return size().prod(); }
};

} // namespace twm

// String formatting routines for Vec2 and Rect
namespace std {
template <> struct std::formatter<twm::Vec2> {
	constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }
	auto format(const twm::Vec2& v, std::format_context& ctx) {
		return std::format_to(ctx.out(), "[{}, {}]", v.x, v.y);
	}
};
template <> struct std::formatter<twm::Rect> {
	constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }
	auto format(const twm::Rect& r, std::format_context& ctx) {
		return std::format_to(ctx.out(), "[top_left={}, bottom_right={}]", r.top_left, r.bottom_right);
	}
};
} // namespace std
