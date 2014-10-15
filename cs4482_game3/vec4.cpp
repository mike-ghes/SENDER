#include "vec4.h"
#include <cmath>
#include <cassert>

bool operator==(const vec4& a, const vec4& b)
{
	return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
}

bool operator!=(const vec4& a, const vec4& b)
{
	return !(a == b);
}

vec4 operator+(const vec4& a, const vec4& b) 
{
	return vec4(a.x+b.x,a.y+b.y,a.z+b.z,a.w+b.w);
}

vec4 operator-(const vec4& a, const vec4& b) 
{
	return vec4(a.x-b.x,a.y-b.y,a.z-b.z,a.w-b.w);
}

vec4 operator-(const vec4& a) 
{
	return vec4(-a.x,-a.y,-a.z,-a.w);
}

float operator*(const vec4& a, const vec4& b) 
{
	return a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w;
}

vec4 operator*(const vec4& a, float b)
{
	return vec4(a.x*b,a.y*b,a.z*b,a.w*b);
}

vec4 operator*(float a, const vec4& b)
{
	return b * a;
}

vec4 operator/(const vec4& a, float b)
{
	assert(b != 0);
	return a * (1.0f/b);
}

vec4& operator+=(vec4& a, const vec4& b) 
{
	a.x += b.x;
	a.y += b.y;
	a.z += b.z;
	a.w += b.w;
	return a;
}

vec4& operator-=(vec4& a, const vec4& b) 
{
	a.x -= b.x;
	a.y -= b.y;
	a.z -= b.z;
	a.w -= b.w;
	return a;
}

vec4& operator*=(vec4& a, float b) 
{
	a.x *= b;
	a.y *= b;
	a.z *= b;
	a.w *= b;
	return a;
}

vec4& operator/=(vec4& a, float b) 
{
	assert(b != 0);
	a *= 1.0f/b;
	return a;
}

vec4 cross(const vec4& a, const vec4& b)
{
	assert(abs(a.w) <= 0.001f && abs(b.w) <= 0.001f); // make sure inputs have only 3 non-zero components
	return vec4(a.y*b.z - a.z*b.y,
	            a.z*b.x - a.x*b.z,
	            a.z*b.y - a.y*b.x, 0);
}

float norm(const vec4& v)
{
	return sqrt(v*v);
}

vec4 normalize(const vec4& v)
{
	return v/norm(v);
}

