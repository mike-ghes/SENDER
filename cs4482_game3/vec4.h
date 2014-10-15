#ifndef __VEC4_H__
#define __VEC4_H__

//
// vec4 -- a super simple vector of 4 elements
//
struct vec4 {
	float x,y,z,w;
	
	// default vector is (0,0,0,0) unless specified by (x,y,z,w)
	vec4();
	vec4(float x, float y, float z, float w);

	// access items by index instead of by name, e.g. v[1] returns v.y
	      float& operator[](int i);
	const float& operator[](int i) const;

	// get raw pointer to vector data (useful for passing to OpenGL when needed)
	      float* ptr();
	const float* ptr() const;
};

////////////////////////////////////////////////////////

bool   operator==(const vec4& a, const vec4& b);
bool   operator!=(const vec4& a, const vec4& b);
vec4   operator+(const vec4& a, const vec4& b);     // vector-vector addition
vec4   operator-(const vec4& a, const vec4& b);     // vector-vector subtraction
vec4   operator-(const vec4& a);                    // vector negation
float  operator*(const vec4& a, const vec4& b);     // vector-vector multiplication (dot product)
vec4   operator*(const vec4& a, float s);           // vector-scalar multiplication
vec4   operator*(float a, const vec4& b);           // scalar-vector multiplication (same)
vec4   operator/(const vec4& a, float s);           // vector-scalar division
vec4&  operator+=(vec4& a, const vec4& b);          // allows a += b
vec4&  operator-=(vec4& a, const vec4& b);          // allows a -= b
vec4&  operator*=(vec4& a, float s);                // allows a *= s
vec4&  operator/=(vec4& a, float s);                // allows a /= s

// additional vector operations
vec4   cross(const vec4& a, const vec4& b);         // cross product assuming a and b are 3D vectors (w=0)
float  norm(const vec4& v);                         // the norm (length) of a vector
vec4   normalize(const vec4& v);                    // short for v/norm(v)

////////////////////////////////////////////////////////

inline vec4::vec4()
{
	x = y = z = w = 0;
}

inline vec4::vec4(float x, float y, float z, float w)
{
	this->x = x;
	this->y = y;
	this->z = z;
	this->w = w;
}

inline float& vec4::operator[](int i)
{
	return *(&x + i);
}

inline const float& vec4::operator[](int i) const
{
	return *(&x + i);
}

inline float* vec4::ptr()
{
	return &x;
}

inline const float* vec4::ptr() const
{
	return &x;
}

#endif // __VEC4_H__
