#ifndef __MAT4x4_H__
#define __MAT4x4_H__

#include "vec4.h"  // needed to define mat * vec operator

//
// mat4x4 -- a super simple 4x4 matrix class
//
class mat4x4 {
public:
	struct { float c0,c1,c2,c3; } r0,r1,r2,r3;

	// sets initial state to be identity matrix 
	mat4x4();

	// returns pointer to first element in a row; allows A[row][col] access
	      float* operator[](int row);
	const float* operator[](int row) const;

	// returns raw pointer to matrix data (useful for passing to OpenGL when needed)
	      float* ptr();
	const float* ptr() const;
};

////////////////////////////////////////////////////////

bool    operator==(const mat4x4& A, const mat4x4& B);
bool    operator!=(const mat4x4& A, const mat4x4& B);
mat4x4  operator*(const mat4x4& A, const mat4x4& B);   // matrix-matrix multiplication (A*B)
vec4    operator*(const mat4x4& A, const vec4& x);     // matrix-vector multiplication (A*x)
mat4x4  operator*(const mat4x4& A, float s);           // matrix-scalar multiplication (A*s)
mat4x4  operator*(float s, const mat4x4& A);           // matrix-scalar multiplication (s*A)
mat4x4& operator*=(mat4x4& A, const mat4x4& B);        // allows (A *= B)
mat4x4& operator*=(mat4x4& A, float s);                // allows (A *= s)

// Standard linear algebra operations
mat4x4 transpose(const mat4x4& A);
mat4x4 inverse(const mat4x4& A);

// Generate matrix corresponding to various affine transformations
mat4x4 translation(const vec4& v); // short for translation(v.x,v.y,v.z)
mat4x4 translation(float dx, float dy, float dz);
mat4x4 scaling(float sx, float sy, float sz);
mat4x4 rotation_x(float angle);    // rotate about x axis (pitch)
mat4x4 rotation_y(float angle);    // rotate about y axis (yaw)
mat4x4 rotation_z(float angle);    // rotate about z axis (roll)

// Generate matrix corresponding to various projective transformations
mat4x4 orthographic(float left, float right, float bottom, float top, float near, float far);
mat4x4 perspective(float left, float right, float bottom, float top, float near, float far);


////////////////////////////////////////////////////////

inline float* mat4x4::operator[](int row)
{
	return &r0.c0 + 4*row; // return address of first item in row
}

inline const float* mat4x4::operator[](int row) const
{
	return &r0.c0 + 4*row; // return address of first item in row
}

inline float* mat4x4::ptr()
{
	return &r0.c0; 
}

inline const float* mat4x4::ptr() const
{
	return &r0.c0;
}

#endif // __MAT4x4_H__
