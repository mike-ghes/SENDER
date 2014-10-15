#include "mat4x4.h"
#include <cmath>

mat4x4::mat4x4()
{
	r0.c0 = 1; r0.c1 = 0; r0.c2 = 0; r0.c3 = 0;
	r1.c0 = 0; r1.c1 = 1; r1.c2 = 0; r1.c3 = 0;
	r2.c0 = 0; r2.c1 = 0; r2.c2 = 1; r2.c3 = 0;
	r3.c0 = 0; r3.c1 = 0; r3.c2 = 0; r3.c3 = 1;
}

////////////////////////////////////////////////////////

bool operator==(const mat4x4& A, const mat4x4& B)
{
	const float* a = &A[0][0];
	const float* b = &B[0][0];
	for (int i = 0; i < 16; ++i)
		if (a[i] != b[i])
			return false;
	return true;
}

bool operator!=(const mat4x4& A, const mat4x4& B)
{
	return !(A == B);
}

mat4x4 operator*(const mat4x4& A, const mat4x4& B)
{
	mat4x4 C;

	const float* b = &B[0][0];
	for (int row = 0; row < 4; ++row) {
		const float* a = &A[row][0];
		float*       c = &C[row][0];
		c[0] = a[0]*b[0] + a[1]*b[4] + a[2]*b[8]  + a[3]*b[12];
		c[1] = a[0]*b[1] + a[1]*b[5] + a[2]*b[9]  + a[3]*b[13];
		c[2] = a[0]*b[2] + a[1]*b[6] + a[2]*b[10] + a[3]*b[14];
		c[3] = a[0]*b[3] + a[1]*b[7] + a[2]*b[11] + a[3]*b[15];
	}	
	return C;
}

vec4 operator*(const mat4x4& A, const vec4& x)
{
	vec4 r;
	for (int row = 0; row < 4; ++row) {
		const float* a = A[row];
		r[row] = a[0]*x[0] + a[1]*x[1] + a[2]*x[2] + a[3]*x[3];
	}
	return r;
}

mat4x4 operator*(const mat4x4& A, float s)
{
	mat4x4 R = A;
	const float* a = &A[0][0];
	float*       r = &R[0][0];
	for (int i = 0; i < 16; ++i)
		r[i] = a[i]*s;
	return R;
}

mat4x4 operator*(float s, const mat4x4& A)
{
	return A*s;
}

mat4x4& operator*=(mat4x4& A, const mat4x4& B)
{
	A = A*B;
	return A;
}

mat4x4& operator*=(mat4x4& A, float s)
{
	float* a = &A[0][0];
	for (int i = 0; i < 16; ++i)
		a[i] *= s;
	return A;
}

mat4x4 transpose(const mat4x4& A)
{
	mat4x4 R;
	for (int i = 0; i < 4; ++i) {
		const float* a = A[i];
		R[0][i] = a[0];
		R[1][i] = a[1];
		R[2][i] = a[2];
		R[3][i] = a[3];
	}
	return R;
}


mat4x4 inverse(const mat4x4& A)
{
	mat4x4 R;
	const float* a = &A[0][0];
	float* r = &R[0][0];
	r[0]  =  a[5]*a[10]*a[15] - a[5]*a[14]*a[11] - a[6]*a[9]*a[15] + a[6]*a[13]*a[11] + a[7]*a[9]*a[14] - a[7]*a[13]*a[10];
	r[1]  = -a[1]*a[10]*a[15] + a[1]*a[14]*a[11] + a[2]*a[9]*a[15] - a[2]*a[13]*a[11] - a[3]*a[9]*a[14] + a[3]*a[13]*a[10];
	r[2]  =  a[1]*a[6]*a[15]  - a[1]*a[14]*a[7]  - a[2]*a[5]*a[15] + a[2]*a[13]*a[7]  + a[3]*a[5]*a[14] - a[3]*a[13]*a[6];
	r[3]  = -a[1]*a[6]*a[11]  + a[1]*a[10]*a[7]  + a[2]*a[5]*a[11] - a[2]*a[9]*a[7]   - a[3]*a[5]*a[10] + a[3]*a[9]*a[6];
	r[4]  = -a[4]*a[10]*a[15] + a[4]*a[14]*a[11] + a[6]*a[8]*a[15] - a[6]*a[12]*a[11] - a[7]*a[8]*a[14] + a[7]*a[12]*a[10];
	r[5]  =  a[0]*a[10]*a[15] - a[0]*a[14]*a[11] - a[2]*a[8]*a[15] + a[2]*a[12]*a[11] + a[3]*a[8]*a[14] - a[3]*a[12]*a[10];
	r[6]  = -a[0]*a[6]*a[15]  + a[0]*a[14]*a[7]  + a[2]*a[4]*a[15] - a[2]*a[12]*a[7]  - a[3]*a[4]*a[14] + a[3]*a[12]*a[6];
	r[7]  =  a[0]*a[6]*a[11]  - a[0]*a[10]*a[7]  - a[2]*a[4]*a[11] + a[2]*a[8]*a[7]   + a[3]*a[4]*a[10] - a[3]*a[8]*a[6];
	r[8]  =  a[4]*a[9]*a[15]  - a[4]*a[13]*a[11] - a[5]*a[8]*a[15] + a[5]*a[12]*a[11] + a[7]*a[8]*a[13] - a[7]*a[12]*a[9];
	r[9]  = -a[0]*a[9]*a[15]  + a[0]*a[13]*a[11] + a[1]*a[8]*a[15] - a[1]*a[12]*a[11] - a[3]*a[8]*a[13] + a[3]*a[12]*a[9];
	r[10] =  a[0]*a[5]*a[15]  - a[0]*a[13]*a[7]  - a[1]*a[4]*a[15] + a[1]*a[12]*a[7]  + a[3]*a[4]*a[13] - a[3]*a[12]*a[5];
	r[11] = -a[0]*a[5]*a[11]  + a[0]*a[9]*a[7]   + a[1]*a[4]*a[11] - a[1]*a[8]*a[7]   - a[3]*a[4]*a[9]  + a[3]*a[8]*a[5];
	r[12] = -a[4]*a[9]*a[14]  + a[4]*a[13]*a[10] + a[5]*a[8]*a[14] - a[5]*a[12]*a[10] - a[6]*a[8]*a[13] + a[6]*a[12]*a[9];
	r[13] =  a[0]*a[9]*a[14]  - a[0]*a[13]*a[10] - a[1]*a[8]*a[14] + a[1]*a[12]*a[10] + a[2]*a[8]*a[13] - a[2]*a[12]*a[9];
	r[14] = -a[0]*a[5]*a[14]  + a[0]*a[13]*a[6]  + a[1]*a[4]*a[14] - a[1]*a[12]*a[6]  - a[2]*a[4]*a[13] + a[2]*a[12]*a[5];
	r[15] =  a[0]*a[5]*a[10]  - a[0]*a[9]*a[6]   - a[1]*a[4]*a[10] + a[1]*a[8]*a[6]   + a[2]*a[4]*a[9]  - a[2]*a[8]*a[5];

	float det = a[0]*r[0] + a[4]*r[1] + a[8]*r[2] + a[12]*r[3];
	if (det != 0)  // is matrix invertible? better hope so!
		det = 1.0f / det;
	R *= det;
	return R;
}

mat4x4 translation(const vec4& v)
{
	return translation(v.x,v.y,v.z);
}

mat4x4 translation(float dx, float dy, float dz)
{
	mat4x4 T;
	T[0][3] = dx;
	T[1][3] = dy;
	T[2][3] = dz;
	return T;
}

mat4x4 rotation_x(float angle)
{
	mat4x4 R;
	R[1][1] =  cos(angle);
	R[1][2] = -sin(angle);
	R[2][1] =  sin(angle);
	R[2][2] =  cos(angle);
	return R;
}

mat4x4 rotation_y(float angle)
{
	mat4x4 R;
	R[0][0] =  cos(angle);
	R[0][2] = -sin(angle);
	R[2][0] =  sin(angle);
	R[2][2] =  cos(angle);
	return R;
}

mat4x4 rotation_z(float angle)
{
	mat4x4 R;
	R[0][0] =  cos(angle);
	R[0][1] = -sin(angle);
	R[1][0] =  sin(angle);
	R[1][1] =  cos(angle);
	return R;
}

mat4x4 scaling(float sx, float sy, float sz)
{
	mat4x4 S;
	S[0][0] = sx;
	S[1][1] = sy;
	S[2][2] = sz;
	return S;
}

mat4x4 orthographic(float left, float right, float bottom, float top, float near, float far)
{
	mat4x4 r;
	r[0][0] =  2.0f / (right-left);
	r[1][1] =  2.0f / (top-bottom);
	r[2][2] = -2.0f / (far-near);
	r[0][3] = -(right+left) / (right-left);
	r[1][3] = -(top+bottom) / (top-bottom);
	r[2][3] = -(far+near) / (far-near);
	return r;
}

mat4x4 perspective(float left, float right, float bottom, float top, float near, float far)
{
	mat4x4 r;
	r[0][0] = -2*near / (right-left);
	r[1][1] = -2*near / (top-bottom);
	r[0][2] = (right+left) / (right-left);
	r[1][2] = (top+bottom) / (top-bottom);
	r[2][2] = -(far+near) / (far-near);
	r[2][3] = 2*near*far / (far-near);
	r[3][2] = -1;
	r[3][3] = 0;
	return r;
}
