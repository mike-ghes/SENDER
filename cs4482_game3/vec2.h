#ifndef __VEC2_H__
#define __VEC2_H__

//
// vec2 -- a super simple vector of 2 elements
//
struct vec2 {
	float x,y;
	
	// default vector is (0,0) unless specified by (x,y)
	vec2();
	vec2(float x, float y);

	// get raw pointer to vector data (useful for passing to OpenGL when needed)
	      float* ptr();
	const float* ptr() const;
};

////////////////////////////////////////////////////////

bool   operator==(const vec2& a, const vec2& b);
bool   operator!=(const vec2& a, const vec2& b);
vec2   operator+(const vec2& a, const vec2& b);     // vector-vector addition
vec2   operator-(const vec2& a, const vec2& b);     // vector-vector subtraction
float  operator*(const vec2& a, const vec2& b);     // vector-vector multiplication (dot product)
vec2   operator*(const vec2& a, float s);           // vector-scalar multiplication
vec2   operator*(float a, const vec2& b);           // scalar-vector multiplication (same)
vec2   operator/(const vec2& a, float s);           // vector-scalar division
vec2&  operator+=(vec2& a, const vec2& b);          // allows a += b
vec2&  operator-=(vec2& a, const vec2& b);          // allows a -= b
vec2&  operator*=(vec2& a, float s);                // allows a *= s
vec2&  operator/=(vec2& a, float s);                // allows a /= s

////////////////////////////////////////////////////////

inline vec2::vec2()
{
	x = y = 0;
}

inline vec2::vec2(float x, float y)
{
	this->x = x;
	this->y = y;
}

inline float* vec2::ptr()
{
	return &x;
}

inline const float* vec2::ptr() const
{
	return &x;
}

#endif // __vec2_H__
