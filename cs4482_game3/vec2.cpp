#include "vec2.h"
#include <cassert>

bool operator==(const vec2& a, const vec2& b)
{
	return a.x == b.x && a.y == b.y;
}

bool operator!=(const vec2& a, const vec2& b)
{
	return !(a == b);
}

vec2 operator+(const vec2& a, const vec2& b) 
{
	return vec2(a.x+b.x,a.y+b.y);
}

vec2 operator-(const vec2& a, const vec2& b) 
{
	return vec2(a.x-b.x,a.y-b.y);
}

float operator*(const vec2& a, const vec2& b) 
{
	return a.x*b.x + a.y*b.y;
}

vec2 operator*(const vec2& a, float b)
{
	return vec2(a.x*b,a.y*b);
}

vec2 operator*(float a, const vec2& b)
{
	return b * a;
}

vec2 operator/(const vec2& a, float b)
{
	assert(b != 0);
	return a * (1.0f/b);
}

vec2& operator+=(vec2& a, const vec2& b) 
{
	a.x += b.x;
	a.y += b.y;
	return a;
}

vec2& operator-=(vec2& a, const vec2& b) 
{
	a.x -= b.x;
	a.y -= b.y;
	return a;
}

vec2& operator*=(vec2& a, float b) 
{
	a.x *= b;
	a.y *= b;
	return a;
}

vec2& operator/=(vec2& a, float b) 
{
	assert(b != 0);
	a *= 1.0f/b;
	return a;
}
