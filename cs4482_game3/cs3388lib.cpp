/*******************************************************************************
  Course(s):
     CS 3388, University of Western Ontario

  Author(s):
     Andrew Delong <firstname.lastname@gmail.com>
     PNG loader by Alex Pankratov and Mark Adler (see end of file).

  Description:
     Provides functions to simplify a number of tasks needed for assignments.
     See cs3388lib.h

  History:
     Sep 17, 2011; - fixed y-axis flip problems in OpenGL 2.1 
                     code path for gl_drawbitmap 

     Sep 13, 2011; - gl_drawbitmap now works with OpenGL 2.1 

     Sep  9, 2011; - PNG loading and bitmap drawing
                   - GLSL shader loading
                   - random number generating
                   - clipped line and circle drawing

*******************************************************************************/

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#define _WIN32_WINNT 0x501
#define WINVER       0x501
#define NOMINMAX
#include <windows.h>
#include "cs3388lib.h"
#include "gl3w.h"
#include "glut.h"
#include <ctime>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
using namespace std;

#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp)   ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp)   ((int)(short)HIWORD(lp))
#endif

#undef LoadImage
#ifdef UNICODE
#define LoadImageWin32  LoadImageW
#else
#define LoadImageWin32  LoadImageA
#endif // !UNICODE

#ifdef _WIN64 // Stupid SetWindowLongPtr doesn't have the signature that the docs claim. Pfft.
#define SETWINDOWLONGPTR(hWnd, index, newLong) SetWindowLongPtr(hWnd, index, (LONG_PTR)(newLong))
#else
#define SETWINDOWLONGPTR(hWnd, index, newLong) (size_t)SetWindowLong(hWnd, index, (LONG)(size_t)(newLong))
#endif


#define BUFFER_OFFSET(bytes) ((const char*)(0) + bytes)

#ifdef _UNICODE
typedef wstring wstr;
static wstr char2wstr(const char* str) {
	wstring result;
	if (str) {
		int len = MultiByteToWideChar(CP_UTF8, 0, str, -1, 0, 0);
		result.resize(len);
		MultiByteToWideChar(CP_UTF8, 0, str, -1, &result[0], len);
	}
	return result;
}

static string wchar2str(const TCHAR* str) {
	string result;
	if (str) {
		int len = WideCharToMultiByte(CP_UTF8, 0, str, -1, 0, 0, 0, 0);
		result.resize(len-1);
		WideCharToMultiByte(CP_UTF8, 0, str, -1, &result[0], len, 0, 0);
	}
	return result;
}
#define sprintf_TEXT swprintf_s
#else
typedef string wstr;
static wstr char2wstr(const char* str) { return str; }
static string wchar2str(const char* str) { return str; }
#define sprintf_TEXT sprintf_s
#endif


#if _MSC_VER < 1400  // VC7.1 (2003) or below
// _s variants not defined until VC8 (2005)
static int vsprintf_s(char* dst, size_t, const char* format, va_list args)
{
	return vsprintf(dst, format, args);
}

static int sprintf_s(char* dst, size_t, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	int result = vsprintf(dst, format, args);
	va_end(args);
	return result;
}
#endif

HBITMAP LoadPng(const TCHAR * resName,
                const TCHAR * resType,
                HMODULE         resInst,
                BOOL   premultiplyAlpha); // LoadPng by Alex Pankratov

static void sShowDialog_(const char* title, const char* format, va_list args)
{
	int textLen = _vscprintf(format, args);
	char* message = new char[textLen+1];
	vsprintf_s(message, textLen+1, format, args);
	HDC dc = wglGetCurrentDC();
	HWND hWnd = WindowFromDC(dc);
	MessageBoxA(hWnd, message, title, MB_OK);
	delete [] message;
}

static void sShowDialog(const char* title, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	sShowDialog_(title, format, args);
	va_end(args);
}

void assert_dialog(const char* msg, const char* file, int line)
{
	const char* nameptr = strrchr(file, '\\');
	if (!nameptr) nameptr = strrchr(file, '/');
	if (!nameptr) nameptr = file;
	sShowDialog("ASSERT failed!", "%s\n\n%s line %d", msg, nameptr, line);
}

bitmap* bitmap_create(int width, int height)
{
	bitmap* bm = new bitmap;
	bm->wd = width;
	bm->ht = height;
	bm->pixels = new unsigned char[width*height*4];
	for (int i = 0; i < width*height; ++i) {
		bm->pixels[4*i+0] = 0;
		bm->pixels[4*i+1] = 0;
		bm->pixels[4*i+2] = 0;
		bm->pixels[4*i+3] = 255;
	}
	return bm;
}

unsigned char* bitmap_pixel(bitmap* bm, int x, int y)
{
	assert_msg(x >= 0 && y >= 0 && x < bm->wd && y < bm->ht,
	          "Requested pixel is outside image boundary");
	return bm->pixels + 4*(bm->wd*y + x);
}

bitmap* bitmap_load(const char* filename)
{
	bool isPNG = strstr(filename,".png") || strstr(filename,".PNG");
	bool isBMP = strstr(filename,".bmp") || strstr(filename,".BMP");
	assert_msg(isBMP || isPNG,"bitmap_load can only load .bmp or .png images");

	HANDLE bmp = 0;

	if (isPNG) {
		bmp = LoadPng(char2wstr(filename).c_str(), NULL, NULL, TRUE);
	} else {
		// Ask Windows to do the dirty work
		bmp = LoadImageWin32(0, char2wstr(filename).c_str(), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
	}

	if (!bmp) {
		char msg[512];
		sprintf_s(msg,512,"Failed to load image file %s (wrong name? file not in path? internal format unsupported?",filename);
		assert_msg(bmp,msg);
	}

	// Ask for the bitmap details and remember them in the registry for easy access
	HANDLE osHandle = bmp;
	DIBSECTION dib;

	if (GetObject(bmp, sizeof(dib), &dib) != sizeof(dib)) {
		DeleteObject(bmp);
		return 0;
	}

	bitmap* bm = 0;

	if (dib.dsBm.bmBitsPixel <= 8) {
		// For CreateImage and DrawImage functions, 8-bit images treated as grayscale. 
		// Any palettized image must therefore either be grayscale to begin with, OR 
		// be converted to BGR internally at load time.
		RGBQUAD palette[256];
		HDC tempDC = CreateCompatibleDC(0);
		HBITMAP oldBMP = (HBITMAP)SelectObject(tempDC, bmp);
		GetDIBColorTable(tempDC, 0, dib.dsBmih.biClrUsed, palette);
		int bpp = 1;
		for (unsigned i = 0; i < dib.dsBmih.biClrUsed; ++i) {
			if (palette[i].rgbBlue != palette[i].rgbGreen || palette[i].rgbBlue != palette[i].rgbRed) {
				bpp = 3; // palette is not greyscale
				break;
			}
		}
		SelectObject(tempDC, oldBMP);
		DeleteDC(tempDC);

		bm = bitmap_create(dib.dsBm.bmWidth, dib.dsBm.bmHeight);
		for (int y = 0; y < dib.dsBm.bmHeight; ++y) {
			unsigned char* src = (unsigned char*)dib.dsBm.bmBits + dib.dsBm.bmWidthBytes*y;
			unsigned char* dst = bitmap_pixel(bm, 0, dib.dsBmih.biHeight < 0 ? y : dib.dsBmih.biHeight-y-1); // potentially flip row order
			for (int x = 0; x < dib.dsBm.bmWidth; ++x) {
				unsigned char srcByte = src[x*dib.dsBmih.biBitCount/8];
				unsigned pindex = (srcByte >> (8-dib.dsBmih.biBitCount-(dib.dsBmih.biBitCount*x)%8)) & ((1 << dib.dsBmih.biBitCount)-1);
				memcpy(dst+x*4, palette+pindex, 3);
				dst[x*4+3] = 255; // no transparency
			}
		}

	} else {
		// The image is not palettized, so load it directly but flip row order if necessary
		bm = bitmap_create(dib.dsBm.bmWidth, dib.dsBm.bmHeight);
		int srcbpp = dib.dsBm.bmBitsPixel/8;
		for (int y = 0; y < dib.dsBm.bmHeight; ++y) {
			unsigned char* src = (unsigned char*)dib.dsBm.bmBits + dib.dsBm.bmWidthBytes*y;
			unsigned char* dst = bitmap_pixel(bm, 0, dib.dsBmih.biHeight < 0 ? y : dib.dsBmih.biHeight-y-1); // potentially flip row order
			for (int x = 0; x < dib.dsBm.bmWidth; ++x) {
				memcpy(dst + 4*x, src + srcbpp*x, srcbpp);
				if (srcbpp < 4)
					dst[4*x+3] = 255; // no transparency
			}
		}
	}

	DeleteObject(bmp);
	return bm;
}

void bitmap_save(bitmap* bm, const char* filename)
{
	bool isBMP = strstr(filename, ".bmp") == filename+strlen(filename)-4 
		      || strstr(filename, ".BMP") == filename+strlen(filename)-4;
	assert_msg(isBMP,"Only BMP images are supported; file name must end in .bmp");

	BITMAPFILEHEADER bmfh;
	bmfh.bfType = 'B' | ('M' << 8);
	bmfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + 0;
	bmfh.bfSize = bmfh.bfOffBits + bm->wd*bm->ht*4;
	bmfh.bfReserved1 = bmfh.bfReserved2 = 0;

	FILE* fh;
	fopen_s(&fh, filename, "wb");
	assert_msg(fh, "Could not open file for writing");
	fwrite(&bmfh, sizeof(BITMAPFILEHEADER), 1, fh);
	BITMAPINFOHEADER bmih; 
	bmih.biSize = sizeof(bmih);
	bmih.biWidth = bm->wd;
	bmih.biHeight = -bm->ht; // Standard for on-disk BMP is to have bottom-first row order
	bmih.biPlanes = 1;
	bmih.biBitCount = 32;
	bmih.biCompression = BI_RGB;
	bmih.biSizeImage = 0;
	bmih.biXPelsPerMeter = 0;
	bmih.biYPelsPerMeter = 0;
	bmih.biClrUsed = 0;
	bmih.biClrImportant = 0;
	fwrite(&bmih, sizeof(BITMAPINFOHEADER), 1, fh);
	fwrite(bm->pixels, bm->wd*bm->ht*4, 1, fh);
	fclose(fh);
}

void bitmap_delete(bitmap* bm)
{
	assert(bm != 0 && bm->pixels != 0);
	delete [] bm->pixels;
	delete bm;
}

struct QPCInitializer {
	QPCInitializer() 
	{
		QueryPerformanceFrequency(&frequency);
		QueryPerformanceCounter(&ticks0);
	}
	LARGE_INTEGER ticks0;
	LARGE_INTEGER frequency;
};

static QPCInitializer sQPC; // constructor is called before main(), to record the tick counter at program startup.

double GetMilliseconds()
{
	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);
	double microseconds = (double)(10000000i64*(now.QuadPart-sQPC.ticks0.QuadPart) / sQPC.frequency.QuadPart)/10.0;
	return microseconds / 1000;
}

static bool sRandInitialized = false;

int random_int()
{
	if (!sRandInitialized) {
		srand((unsigned)time(0));
		sRandInitialized = true;
	}
	// randomize all 31 bits to make an unsigned random integer
	return ((rand() & 0x7fff) | (rand() << 16)) & ~(1 << 31);
}

float random_float()
{
	int x = random_int();  // int from 0..2^31
	double d = (double)x / ((double)~(1 << 31)+1); // double from 0..1
	return (float)d;
}

////////////////////////////////////////////////////

string readfile(string filename)
{
	ifstream infile(filename,std::ios::binary);
	std::stringstream ss;
	ss << infile.rdbuf();
	return ss.str();
}

GLuint gl_createshader(const char* shadercode, GLuint shadertype)
{
	GLuint shader = glCreateShader(shadertype);
	glShaderSource(shader,1,&shadercode,0);
	glCompileShader(shader);
	GLint success = 0;
	glGetShaderiv(shader,GL_COMPILE_STATUS,&success);
	if (!success) {
		GLchar infolog[1024];
		glGetShaderInfoLog(shader,1024,NULL,infolog);	
		cout << "glCompileShader(" << (shadertype == GL_VERTEX_SHADER ? "VERTEX" : "FRAGMENT" ) <<  ") FAILED: " << endl << infolog << endl;
		assert_msg(false,"gl_createshader failed inside glCompileShader;\n see console output for details");
	}
	return shader;
}

GLuint gl_createprogram(const char* vscode, const char* fscode)
{
	GLuint vs = gl_createshader(vscode,GL_VERTEX_SHADER);
	GLuint fs = gl_createshader(fscode,GL_FRAGMENT_SHADER);
	GLuint program = glCreateProgram();
	glAttachShader(program,vs);
	glAttachShader(program,fs);
	glLinkProgram(program);
	glDeleteShader(fs);
	glDeleteShader(vs);
	GLint success = 0;
	glGetProgramiv(program,GL_LINK_STATUS,&success);
	if (!success) {
		GLchar infolog[1024];
		glGetProgramInfoLog(program,1024,NULL,infolog);	
		cout << "glLinkProgram FAILED: " 
			 << endl << infolog << endl;
		assert_msg(false,"gl_createprogram failed inside glLinkProgram;\n see console output for details");
	}
	return program;
}

GLuint gl_loadprogram(const char* vsfile, const char* fsfile)
{
	string vscode = readfile(vsfile);
	string fscode = readfile(fsfile);
	return gl_createprogram(vscode.c_str(),fscode.c_str());
}

void fillrect(bitmap* bm, int x0, int y0, int x1, int y1,
	          unsigned char r, unsigned char g, unsigned char b)
{
	int bgra;
	((unsigned char*)&bgra)[0] = b;
	((unsigned char*)&bgra)[1] = g;
	((unsigned char*)&bgra)[2] = r;
	((unsigned char*)&bgra)[3] = 255;
	
	if (x0 > x1)
		swap(++x0,++x1);
	if (y0 > y1)
		swap(++y0,++y1);

	int row_inc = 4*bm->wd;
	unsigned char* row     = bm->pixels + row_inc*y0;
	unsigned char* row_end = bm->pixels + row_inc*y1;
	while (row < row_end) {
		int* pixel = (int*)row;
		for (int x = x0; x < x1; ++x)
			pixel[x] = bgra;
		row += row_inc;
	}
}

void drawrect(bitmap* bm, int x0, int y0, int x1, int y1,
	          unsigned char r, unsigned char g, unsigned char b)
{
	int bgra;
	((unsigned char*)&bgra)[0] = b;
	((unsigned char*)&bgra)[1] = g;
	((unsigned char*)&bgra)[2] = r;
	((unsigned char*)&bgra)[3] = 255;
	
	if (x0 > x1)
		swap(++x0,++x1);
	if (y0 > y1)
		swap(++y0,++y1);

	int row_inc = 4*bm->wd;
	unsigned char* row     = bm->pixels + row_inc*y0;
	unsigned char* row_end = bm->pixels + row_inc*y1;
	if (y0 < y1)
		for (int x = x0; x < x1; ++x)
			*((int*)row + x) = bgra;

	while (row < row_end) {
		int* pixel = (int*)row;
		pixel[x0] = bgra;
		pixel[x1-1] = bgra;
		row += row_inc;
	}

	if (y0 < y1) {
		row -= row_inc;
		for (int x = x0; x < x1; ++x)
			*((int*)row + x) = bgra;
	}
}

void drawline(bitmap* bm, int x0, int y0, int x1, int y1,
	          unsigned char r, unsigned char g, unsigned char b)
{
	// code below is based directly on the paper:
	//    Yevgeny P. Kuzmin. Bresenham's Line Generation Algorithm with 
	//    Built-in Clipping. Computer Graphics Forum, 14(5):275--280, 2005.
	//
	int wx0 = 0, wy0 = 0;
	int wx1 = bm->wd-1, wy1 = bm->ht-1;
	int  dsx,dsy,stx,sty,xd,yd,dx2,dy2,rem,term,e;
	int* d0;
	int* d1;
	if (x0 < x1) {
		if (x0 > wx1 || x1 < wx0)
			return;
		stx = 1;
	} else {
		if (x1 > wx1 || x0 < wx0)
			return;
		stx = -1; 
		x0 = -x0; x1 = -x1; 
		wx0 = -wx0; wx1 = -wx1;
		swap(wx0,wx1);
	}
	if (y0 < y1) {
		if (y0 > wy1 || y1 < wy0)
			return;
		sty = 1;
	} else {
		if (y1 > wy1 || y0 < wy0)
			return;
		sty = -1;
		y0 = -y0; y1 = -y1;
		wy0 = -wy0; wy1 = -wy1;
		swap(wy0,wy1);
	}
	dsx = x1-x0;
	dsy = y1-y0;
	if (dsx < dsy) {
		d0 = &yd; d1 = &xd;
		swap(x0,y0); swap(x1,y1); swap(dsx,dsy);
		swap(wx0,wy0); swap(wx1,wy1); swap(stx,sty);
	} else {
		d0 = &xd; d1 = &yd;
	}
	dx2 = 2*dsx; dy2 = 2*dsy;
	xd = x0; yd = y0;
	e = 2*dsy-dsx;
	term = x1;
	bool setexit = false;
	if (y0 < wy0) {
		int temp = dx2*(wy0-y0)-dsx;
		xd += temp/dy2; rem = temp%dy2;
		if (xd > wx1)
			return;
		if (xd+1 >= wx0) {
			yd = wy0;
			e -= rem+dsx;
			if (rem > 0) {
				xd++;
				e += dy2;
			}
			setexit = true;
		}
	}
	if (!setexit && x0 < wx0) {
		int temp = dy2*(wx0-x0);
		yd += temp/dx2; rem = temp%dx2;
		if (yd > wy1 || yd == wy1 && rem >= dsx)
			return;
		xd = wx0;
		e += rem;
		if (rem >= dsx) {
			yd++;
			e -= dx2;
		}
	}
	if (y1 > wy1) {
		int temp = dx2*(wy1-y0)+dsx;
		term = x0+temp/dy2; rem = temp%dy2;
		if (rem == 0)
			term--;
	}
	if (term > wx1)
		term = wx1;
	term++;
	if (sty == -1)
		yd = -yd;
	if (stx == -1) {
		xd = -xd;
		term = -term;
	}
	dx2 -= dy2;

	unsigned char* pixels = bm->pixels;
	int wd = bm->wd;

	auto plot = [&](int x,int y) {
		unsigned char* p = pixels + 4*(wd*y+x);
		p[0] = b;
		p[1] = g;
		p[2] = r;
		p[3] = 255;
	};

	while (xd != term) {
		// if you get a crash here, check your x0,y0,x1,y1 coordinates to see 
		// they are in a reasonable range -- it could cause integer overflow 
		// in this function
		plot(*d0,*d1);
		if (e >= 0) { 
			yd += sty; 
			e -= dx2; 
		} else
			e += dy2; 
		xd += stx;
	}
}

void drawcircle(bitmap* bm, int x0, int y0, int radius, unsigned char r, unsigned char g, unsigned char b)
{
	unsigned char* pixels = bm->pixels;
	int wd = bm->wd;
	int ht = bm->ht;

	if (x0+radius < 0 || x0-radius >= wd || y0+radius < 0 || y0-radius >= ht)
		return; // entire circle is outside bitmap

	auto plot = [&](int x, int y) {
		if (x >= 0 && y >= 0 && x < wd && y < ht) {
			unsigned char* p = pixels + 4*(wd*y+x);
			p[0] = b;
			p[1] = g;
			p[2] = r;
			p[3] = 255;
		}
	};

	// code below is based directly on C code from wikipedia
	int f = 1 - radius;
	int ddF_x = 1;
	int ddF_y = -2 * radius;
	int x = 0;
	int y = radius;

	plot(x0, y0 + radius);
	plot(x0, y0 - radius);
	plot(x0 + radius, y0);
	plot(x0 - radius, y0);
 
	while(x < y) {
		if(f >= 0) {
			y--;
			ddF_y += 2;
			f += ddF_y;
		}
		x++;
		ddF_x += 2;
		f += ddF_x;    
		plot(x0 + x, y0 + y);
		plot(x0 - x, y0 + y);
		plot(x0 + x, y0 - y);
		plot(x0 - x, y0 - y);
		plot(x0 + y, y0 + x);
		plot(x0 - y, y0 + x);
		plot(x0 + y, y0 - x);
		plot(x0 - y, y0 - x);
	}
}

////////////////////////////////////////////////////

void gl_drawbitmap(bitmap* bm, int left, int top)
{
	gl_drawbitmap(bm,left,left+bm->wd,top,top+bm->ht);
}

//
// copy-paste from mat4x4.h to avoid dependency on that 
// (file so that project setup is easier for students)
//
#undef local

namespace local {
class mat4x4 {
public:
	struct { float c0,c1,c2,c3; } r0,r1,r2,r3;

	// initializes to an identity matrix as default state
	mat4x4();

	// returns pointer to first element in a row
	      float* operator[](int row);
	const float* operator[](int row) const;

	      float* ptr();
	const float* ptr() const;
};

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

mat4x4::mat4x4()
{
	r0.c0 = 1; r0.c1 = 0; r0.c2 = 0; r0.c3 = 0;
	r1.c0 = 0; r1.c1 = 1; r1.c2 = 0; r1.c3 = 0;
	r2.c0 = 0; r2.c1 = 0; r2.c2 = 1; r2.c3 = 0;
	r3.c0 = 0; r3.c1 = 0; r3.c2 = 0; r3.c3 = 1;
}

////////////////////////////////////////////////////////

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

#undef far
#undef near

mat4x4 orthographic(float left, float right, float top, float bottom, float near, float far)
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

} // namespace local
using namespace local;

////////////////////////////////////////////////////////

GLAPI "C" void APIENTRY glBegin(GLenum mode);
GLAPI "C" void APIENTRY glEnd();
GLAPI "C" void APIENTRY glVertex2fv(const GLfloat* v);
GLAPI "C" void APIENTRY glTexCoord2fv(const GLfloat* v);
GLAPI "C" void APIENTRY glLoadMatrixf(const GLfloat* m);
GLAPI "C" void APIENTRY glMatrixMode(GLenum mode);

void gl_drawbitmap(bitmap* bm, int left, int right, int top, int bottom)
{
	static GLuint bmtex = 0;
	static GLuint bmtex_wd = 0;
	static GLuint bmtex_ht = 0;

	glActiveTexture(GL_TEXTURE0);

	if (!bmtex) {
		glGenTextures(1,&bmtex);
		glBindTexture(GL_TEXTURE_2D,bmtex);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
	} else
		glBindTexture(GL_TEXTURE_2D,bmtex);

	if (bmtex_wd != bm->wd || bmtex_ht != bm->ht) {
		glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,bm->wd,bm->ht,0,GL_BGRA,GL_UNSIGNED_BYTE,bm->pixels);
		bmtex_wd = bm->wd;
		bmtex_ht = bm->ht;
	} else
		glTexSubImage2D(GL_TEXTURE_2D,0,0,0,bm->wd,bm->ht,GL_BGRA,GL_UNSIGNED_BYTE,bm->pixels);
 
	//////////////////////////////////////////////////

	static GLuint quadprog = 0;
	if (!quadprog && gl3wIsSupported(2,1)) {
		static const char* quadvs130 =
			"#version 130\n"
			"uniform mat4 mvp_matrix;\n"
			"in      vec2 position;\n"
			"in      vec2 texcoord0;\n"
			"out     vec2 texcoord;\n"
			"void main()\n"
			"{\n"
			"	gl_Position = mvp_matrix * vec4(position.x,position.y,0,1);\n"
			"	texcoord = texcoord0;\n"
			"}\n";

		static const char* quadfs130 = 
			"#version 130\n"
			"uniform sampler2D texmap;\n"
			"in      vec2 texcoord;\n"
			"out     vec4 fragcolor;\n"
			"void main()\n"
			"{\n"
			"	fragcolor = texture(texmap,texcoord);\n"
			"}\n";

		static const char* quadvs120 =
			"#version 120\n"
			"uniform mat4 mvp_matrix;\n"
			"attribute vec2 position;\n"
			"attribute vec2 texcoord0;\n"
			"varying   vec2 texcoord;\n"
			"void main()\n"
			"{\n"
			"	gl_Position = mvp_matrix * vec4(position.x,position.y,0,1);\n"
			"	texcoord = gl_MultiTexCoord0.xy;//texcoord0;\n"
			"}\n";

		static const char* quadfs120 = 
			"uniform sampler2D texmap;\n"
			"varying vec2 texcoord;\n"
			"void main()\n"
			"{\n"
			"	gl_FragColor = texture2D(texmap,texcoord);\n"
			"}\n";

		if (gl3wIsSupported(3,0))
			quadprog = gl_createprogram(quadvs130,quadfs130);
		else {
			quadprog = gl_createprogram(quadvs120,quadfs120);
		}
	}
	
	//////////////////////////////////////////////////

	struct vertex2p2t {
		float position[2];
		float texcoord[2];
	};

	vertex2p2t quad[4] = {
		{ {0,0}, {0,0} },
		{ {0,1}, {0,1} },

		{ {1,0}, {1,0} },
		{ {1,1}, {1,1} }
	};

	GLushort quad_idx[4] = { 0,1,2,3 };

	static GLuint vao = 0, vbo = 0, vio = 0;

	if (!vao && gl3wIsSupported(3,0)) {
		glGenVertexArrays(1,&vao);
		glBindVertexArray(vao);

		glGenBuffers(1,&vbo);
		glBindBuffer(GL_ARRAY_BUFFER,vbo);
		glBufferData(GL_ARRAY_BUFFER,sizeof(vertex2p2t)*4,quad,GL_STATIC_DRAW);

		glGenBuffers(1,&vio);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,vio);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(GLushort)*4,quad_idx,GL_STATIC_DRAW);

		glVertexAttribPointer(glGetAttribLocation(quadprog,"position"),2,GL_FLOAT,GL_FALSE,sizeof(vertex2p2t),BUFFER_OFFSET(0));
		glEnableVertexAttribArray(glGetAttribLocation(quadprog, "position"));

		glVertexAttribPointer(glGetAttribLocation(quadprog,"texcoord0"),2,GL_FLOAT,GL_FALSE,sizeof(vertex2p2t),BUFFER_OFFSET(2*sizeof(GLfloat)));
		glEnableVertexAttribArray(glGetAttribLocation(quadprog,"texcoord0"));
	}

	/////////////////////////////////////////////////////////

	int viewport[4];
	glGetIntegerv(GL_VIEWPORT,viewport);

	int window_wd = glutGet(GLUT_WINDOW_WIDTH);
	int window_ht = glutGet(GLUT_WINDOW_HEIGHT);
	glViewport(0,0,window_wd,window_ht);

	mat4x4 projection_matrix = orthographic(0,(float)window_wd,0,(float)window_ht,-1,1);
	mat4x4 modelview_matrix;
	modelview_matrix[0][0] = (float)right-left;
	modelview_matrix[1][1] = (float)bottom-top;
	modelview_matrix[0][3] = (float)left;
	modelview_matrix[1][3] = (float)top;
	mat4x4 mvp_matrix = projection_matrix*modelview_matrix;

	if (quadprog) {
		// OpenGL 2.1 or 3.0 shader
		glUseProgram(quadprog);
		glUniformMatrix4fv(glGetUniformLocation(quadprog,"mvp_matrix"),1,GL_TRUE,mvp_matrix.ptr());
		glUniform1i(glGetUniformLocation(quadprog,"texmap"),0);
	} else {
		// pre-OpenGL 2.1 fixed-function 
		glLoadMatrixf(transpose(mvp_matrix).ptr());
		glEnable(GL_TEXTURE_2D);
	}

	if (vao) {
		// OpenGL 3.0 drawing vertex array object
		glBindVertexArray(vao);
		glDrawElements(GL_TRIANGLE_STRIP,4,GL_UNSIGNED_SHORT,0);
		glBindVertexArray(0);
	} else {
		// pre-OpenGL 3.0 drawing with fixed-function primitive
		glBegin(GL_TRIANGLE_STRIP);
		for (int i = 0; i < 4; ++i) {
			glTexCoord2fv(quad[quad_idx[i]].texcoord);
			glVertex2fv(quad[quad_idx[i]].position);
		}
		glEnd();
	}

	if (quadprog)
		glUseProgram(0);

	glBindTexture(GL_TEXTURE_2D,0);
	glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
}


GLAPI "C" void APIENTRY glRasterPos2f(GLfloat x, GLfloat y);
GLAPI "C" void APIENTRY glColor3f(GLfloat r, GLfloat g, GLfloat b);
GLAPI "C" void APIENTRY glPushMatrix();
GLAPI "C" void APIENTRY glLoadIdentity();
GLAPI "C" void APIENTRY glPopMatrix();
GLAPI "C" void APIENTRY glMatrixMode(GLenum mode);
#define GL_MODELVIEW                      0x1700
#define GL_PROJECTION                     0x1701
#define GL_MATRIX_MODE                    0x0BA0

void gl_drawstring(const char* str, int left, int top, unsigned char r, unsigned char g, unsigned char b)
{
	if (!str)
		return;
	gl3wInit();
	GLint mode;
	GLint depthtest;
	glGetIntegerv(GL_MATRIX_MODE,&mode);
	glGetIntegerv(GL_DEPTH_TEST,&depthtest);
	glDisable(GL_DEPTH_TEST);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glColor3f(r/255.0f,g/255.0f,b/255.0f);
	float viewport[4];
	glGetFloatv(GL_VIEWPORT,viewport);
	float char_wd =  8*glutGet(GLUT_WINDOW_WIDTH)/viewport[2];
	float char_ht = 13*glutGet(GLUT_WINDOW_HEIGHT)/viewport[3];
	int x = left, y = top;
	for (size_t i = 0; i < strlen(str); ++i) {
		if (str[i] == '\n') {
			y += char_ht+1;
			x = left;
		} else {
			glRasterPos2f(x*2.0f/glutGet(GLUT_WINDOW_WIDTH)-1,-(y+char_ht)*2.0f/glutGet(GLUT_WINDOW_HEIGHT)+1);
			glutBitmapCharacter(GLUT_BITMAP_8_BY_13,str[i]);
			x += char_wd;
		}
	}
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glMatrixMode((GLenum)mode);
	if (depthtest)
		glEnable(GL_DEPTH_TEST);
}




unsigned gl_loadtexture(const char* filename)
{
	// first try to load our pixels from disk
	bitmap* bm = bitmap_load(filename);
	assert(bm); // error loading file?

	// Ask OpenGL for a unique number that we can use to identify a new texture.
	// This identifier lets us tell OpenGL which texture we're talking about
	GLuint texid = 0;
	glGenTextures(1,&texid);  // OpenGL sets our 'texid' variable to some number

	// Now tell OpenGL that we're going to modify the (currently empty) texture.
	// This requires BINDING our texid to the TEXTURE_2D slot.
	// (there are other slots, like TEXTURE_1D, TEXTURE_3D, but we won't use them at all)
	glBindTexture(GL_TEXTURE_2D,texid);

	// Ask OpenGL to generate a mipmap pyramid so that, when a textured surface is
	// very far away, it can sample from the blurred textures and avoid ugly 'aliasing' 
	glTexParameteri(GL_TEXTURE_2D,GL_GENERATE_MIPMAP,GL_TRUE); // this is pre-3.0 since lab machines don't support glGenerateMipmap

	// First thing we do with an empty texture is upload some pixels.
	// Our bitmap is 32-bit per pixel (GL_RGBA8;GL_UNSIGNED_BYTE) in BGRA order (GL_BGRA).
	glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,bm->wd,bm->ht,0,GL_BGRA,GL_UNSIGNED_BYTE,bm->pixels);

	// Set up a reasonable default mode for the texture.
	// It's up to the student to reconfigure the texture if they want something else.
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT); // can also be GL_CLAMP
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT); // can also be GL_CLAMP
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
	
	bitmap_delete(bm); // we're done with the bitmap -- it's been uploaded to the video card
	return texid;
}







































































































///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
//
//  The code below was taken directly from the lpng library by Alex Pankratov
//  and the puff library by Mark Adler. 
//
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////


/* puff.h
  Copyright (C) 2002, 2003 Mark Adler, all rights reserved
  version 1.7, 3 Mar 2002

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the author be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  Mark Adler    madler@alumni.caltech.edu
 */

/*
 * puff.c
 * Copyright (C) 2002-2004 Mark Adler
 * For conditions of distribution and use, see copyright notice in puff.h
 * version 1.8, 9 Jan 2004
 *
 * puff.c is a simple inflate written to be an unambiguous way to specify the
 * deflate format.  It is not written for speed but rather simplicity.  As a
 * side benefit, this code might actually be useful when small code is more
 * important than speed, such as bootstrap applications.  For typical deflate
 * data, zlib's inflate() is about four times as fast as puff().  zlib's
 * inflate compiles to around 20K on my machine, whereas puff.c compiles to
 * around 4K on my machine (a PowerPC using GNU cc).  If the faster decode()
 * function here is used, then puff() is only twice as slow as zlib's
 * inflate().
 *
 * All dynamically allocated memory comes from the stack.  The stack required
 * is less than 2K bytes.  This code is compatible with 16-bit int's and
 * assumes that long's are at least 32 bits.  puff.c uses the short data type,
 * assumed to be 16 bits, for arrays in order to to conserve memory.  The code
 * works whether integers are stored big endian or little endian.
 *
 * In the comments below are "Format notes" that describe the inflate process
 * and document some of the less obvious aspects of the format.  This source
 * code is meant to supplement RFC 1951, which formally describes the deflate
 * format:
 *
 *    http://www.zlib.org/rfc-deflate.html
 */

/*
 * Change history:
 *
 * 1.0  10 Feb 2002     - First version
 * 1.1  17 Feb 2002     - Clarifications of some comments and notes
 *                      - Update puff() dest and source pointers on negative
 *                        errors to facilitate debugging deflators
 *                      - Remove longest from struct huffman -- not needed
 *                      - Simplify offs[] index in construct()
 *                      - Add input size and checking, using longjmp() to
 *                        maintain easy readability
 *                      - Use short data type for large arrays
 *                      - Use pointers instead of long to specify source and
 *                        destination sizes to avoid arbitrary 4 GB limits
 * 1.2  17 Mar 2002     - Add faster version of decode(), doubles speed (!),
 *                        but leave simple version for readabilty
 *                      - Make sure invalid distances detected if pointers
 *                        are 16 bits
 *                      - Fix fixed codes table error
 *                      - Provide a scanning mode for determining size of
 *                        uncompressed data
 * 1.3  20 Mar 2002     - Go back to lengths for puff() parameters [Jean-loup]
 *                      - Add a puff.h file for the interface
 *                      - Add braces in puff() for else do [Jean-loup]
 *                      - Use indexes instead of pointers for readability
 * 1.4  31 Mar 2002     - Simplify construct() code set check
 *                      - Fix some comments
 *                      - Add FIXLCODES #define
 * 1.5   6 Apr 2002     - Minor comment fixes
 * 1.6   7 Aug 2002     - Minor format changes
 * 1.7   3 Mar 2003     - Added test code for distribution
 *                      - Added zlib-like license
 * 1.8   9 Jan 2004     - Added some comments on no distance codes case
 */

#include <setjmp.h>             /* for setjmp(), longjmp(), and jmp_buf */

#define local static            /* for local function definitions */
#define NIL ((unsigned char *)0)        /* for no output option */

/*
 * Maximums for allocations and loops.  It is not useful to change these --
 * they are fixed by the deflate format.
 */
#define MAXBITS 15              /* maximum bits in a code */
#define MAXLCODES 286           /* maximum number of literal/length codes */
#define MAXDCODES 30            /* maximum number of distance codes */
#define MAXCODES (MAXLCODES+MAXDCODES)  /* maximum codes lengths to read */
#define FIXLCODES 288           /* number of fixed literal/length codes */

/* input and output state */
struct state {
    /* output state */
    unsigned char *out;         /* output buffer */
    unsigned long outlen;       /* available space at out */
    unsigned long outcnt;       /* bytes written to out so far */

    /* input state */
    unsigned char *in;          /* input buffer */
    unsigned long inlen;        /* available input at in */
    unsigned long incnt;        /* bytes read so far */
    int bitbuf;                 /* bit buffer */
    int bitcnt;                 /* number of bits in bit buffer */

    /* input limit error return state for bits() and decode() */
    jmp_buf env;
};

/*
 * Return need bits from the input stream.  This always leaves less than
 * eight bits in the buffer.  bits() works properly for need == 0.
 *
 * Format notes:
 *
 * - Bits are stored in bytes from the least significant bit to the most
 *   significant bit.  Therefore bits are dropped from the bottom of the bit
 *   buffer, using shift right, and new bytes are appended to the top of the
 *   bit buffer, using shift left.
 */
local int bits(struct state *s, int need)
{
    long val;           /* bit accumulator (can use up to 20 bits) */

    /* load at least need bits into val */
    val = s->bitbuf;
    while (s->bitcnt < need) {
        if (s->incnt == s->inlen) longjmp(s->env, 1);   /* out of input */
        val |= (long)(s->in[s->incnt++]) << s->bitcnt;  /* load eight bits */
        s->bitcnt += 8;
    }

    /* drop need bits and update buffer, always zero to seven bits left */
    s->bitbuf = (int)(val >> need);
    s->bitcnt -= need;

    /* return need bits, zeroing the bits above that */
    return (int)(val & ((1L << need) - 1));
}

/*
 * Process a stored block.
 *
 * Format notes:
 *
 * - After the two-bit stored block type (00), the stored block length and
 *   stored bytes are byte-aligned for fast copying.  Therefore any leftover
 *   bits in the byte that has the last bit of the type, as many as seven, are
 *   discarded.  The value of the discarded bits are not defined and should not
 *   be checked against any expectation.
 *
 * - The second inverted copy of the stored block length does not have to be
 *   checked, but it's probably a good idea to do so anyway.
 *
 * - A stored block can have zero length.  This is sometimes used to byte-align
 *   subsets of the compressed data for random access or partial recovery.
 */
local int stored(struct state *s)
{
    unsigned len;       /* length of stored block */

    /* discard leftover bits from current byte (assumes s->bitcnt < 8) */
    s->bitbuf = 0;
    s->bitcnt = 0;

    /* get length and check against its one's complement */
    if (s->incnt + 4 > s->inlen) return 2;      /* not enough input */
    len = s->in[s->incnt++];
    len |= s->in[s->incnt++] << 8;
    if (s->in[s->incnt++] != (~len & 0xff) ||
        s->in[s->incnt++] != ((~len >> 8) & 0xff))
        return -2;                              /* didn't match complement! */

    /* copy len bytes from in to out */
    if (s->incnt + len > s->inlen) return 2;    /* not enough input */
    if (s->out != NIL) {
        if (s->outcnt + len > s->outlen)
            return 1;                           /* not enough output space */
        while (len--)
            s->out[s->outcnt++] = s->in[s->incnt++];
    }
    else {                                      /* just scanning */
        s->outcnt += len;
        s->incnt += len;
    }

    /* done with a valid stored block */
    return 0;
}

/*
 * Huffman code decoding tables.  count[1..MAXBITS] is the number of symbols of
 * each length, which for a canonical code are stepped through in order.
 * symbol[] are the symbol values in canonical order, where the number of
 * entries is the sum of the counts in count[].  The decoding process can be
 * seen in the function decode() below.
 */
struct huffman {
    short *count;       /* number of symbols of each length */
    short *symbol;      /* canonically ordered symbols */
};

/*
 * Decode a code from the stream s using huffman table h.  Return the symbol or
 * a negative value if there is an error.  If all of the lengths are zero, i.e.
 * an empty code, or if the code is incomplete and an invalid code is received,
 * then -9 is returned after reading MAXBITS bits.
 *
 * Format notes:
 *
 * - The codes as stored in the compressed data are bit-reversed relative to
 *   a simple integer ordering of codes of the same lengths.  Hence below the
 *   bits are pulled from the compressed data one at a time and used to
 *   build the code value reversed from what is in the stream in order to
 *   permit simple integer comparisons for decoding.  A table-based decoding
 *   scheme (as used in zlib) does not need to do this reversal.
 *
 * - The first code for the shortest length is all zeros.  Subsequent codes of
 *   the same length are simply integer increments of the previous code.  When
 *   moving up a length, a zero bit is appended to the code.  For a complete
 *   code, the last code of the longest length will be all ones.
 *
 * - Incomplete codes are handled by this decoder, since they are permitted
 *   in the deflate format.  See the format notes for fixed() and dynamic().
 */
#ifdef SLOW
local int decode(struct state *s, struct huffman *h)
{
    int len;            /* current number of bits in code */
    int code;           /* len bits being decoded */
    int first;          /* first code of length len */
    int count;          /* number of codes of length len */
    int index;          /* index of first code of length len in symbol table */

    code = first = index = 0;
    for (len = 1; len <= MAXBITS; len++) {
        code |= bits(s, 1);             /* get next bit */
        count = h->count[len];
        if (code < first + count)       /* if length len, return symbol */
            return h->symbol[index + (code - first)];
        index += count;                 /* else update for next length */
        first += count;
        first <<= 1;
        code <<= 1;
    }
    return -9;                          /* ran out of codes */
}

/*
 * A faster version of decode() for real applications of this code.   It's not
 * as readable, but it makes puff() twice as fast.  And it only makes the code
 * a few percent larger.
 */
#else /* !SLOW */
local int decode(struct state *s, struct huffman *h)
{
    int len;            /* current number of bits in code */
    int code;           /* len bits being decoded */
    int first;          /* first code of length len */
    int count;          /* number of codes of length len */
    int index;          /* index of first code of length len in symbol table */
    int bitbuf;         /* bits from stream */
    int left;           /* bits left in next or left to process */
    short *next;        /* next number of codes */

    bitbuf = s->bitbuf;
    left = s->bitcnt;
    code = first = index = 0;
    len = 1;
    next = h->count + 1;
    while (1) {
        while (left--) {
            code |= bitbuf & 1;
            bitbuf >>= 1;
            count = *next++;
            if (code < first + count) { /* if length len, return symbol */
                s->bitbuf = bitbuf;
                s->bitcnt = (s->bitcnt - len) & 7;
                return h->symbol[index + (code - first)];
            }
            index += count;             /* else update for next length */
            first += count;
            first <<= 1;
            code <<= 1;
            len++;
        }
        left = (MAXBITS+1) - len;
        if (left == 0) break;
        if (s->incnt == s->inlen) longjmp(s->env, 1);   /* out of input */
        bitbuf = s->in[s->incnt++];
        if (left > 8) left = 8;
    }
    return -9;                          /* ran out of codes */
}
#endif /* SLOW */

/*
 * Given the list of code lengths length[0..n-1] representing a canonical
 * Huffman code for n symbols, construct the tables required to decode those
 * codes.  Those tables are the number of codes of each length, and the symbols
 * sorted by length, retaining their original order within each length.  The
 * return value is zero for a complete code set, negative for an over-
 * subscribed code set, and positive for an incomplete code set.  The tables
 * can be used if the return value is zero or positive, but they cannot be used
 * if the return value is negative.  If the return value is zero, it is not
 * possible for decode() using that table to return an error--any stream of
 * enough bits will resolve to a symbol.  If the return value is positive, then
 * it is possible for decode() using that table to return an error for received
 * codes past the end of the incomplete lengths.
 *
 * Not used by decode(), but used for error checking, h->count[0] is the number
 * of the n symbols not in the code.  So n - h->count[0] is the number of
 * codes.  This is useful for checking for incomplete codes that have more than
 * one symbol, which is an error in a dynamic block.
 *
 * Assumption: for all i in 0..n-1, 0 <= length[i] <= MAXBITS
 * This is assured by the construction of the length arrays in dynamic() and
 * fixed() and is not verified by construct().
 *
 * Format notes:
 *
 * - Permitted and expected examples of incomplete codes are one of the fixed
 *   codes and any code with a single symbol which in deflate is coded as one
 *   bit instead of zero bits.  See the format notes for fixed() and dynamic().
 *
 * - Within a given code length, the symbols are kept in ascending order for
 *   the code bits definition.
 */
local int construct(struct huffman *h, short *length, int n)
{
    int symbol;         /* current symbol when stepping through length[] */
    int len;            /* current length when stepping through h->count[] */
    int left;           /* number of possible codes left of current length */
    short offs[MAXBITS+1];      /* offsets in symbol table for each length */

    /* count number of codes of each length */
    for (len = 0; len <= MAXBITS; len++)
        h->count[len] = 0;
    for (symbol = 0; symbol < n; symbol++)
        (h->count[length[symbol]])++;   /* assumes lengths are within bounds */
    if (h->count[0] == n)               /* no codes! */
        return 0;                       /* complete, but decode() will fail */

    /* check for an over-subscribed or incomplete set of lengths */
    left = 1;                           /* one possible code of zero length */
    for (len = 1; len <= MAXBITS; len++) {
        left <<= 1;                     /* one more bit, double codes left */
        left -= h->count[len];          /* deduct count from possible codes */
        if (left < 0) return left;      /* over-subscribed--return negative */
    }                                   /* left > 0 means incomplete */

    /* generate offsets into symbol table for each length for sorting */
    offs[1] = 0;
    for (len = 1; len < MAXBITS; len++)
        offs[len + 1] = offs[len] + h->count[len];

    /*
     * put symbols in table sorted by length, by symbol order within each
     * length
     */
    for (symbol = 0; symbol < n; symbol++)
        if (length[symbol] != 0)
            h->symbol[offs[length[symbol]]++] = symbol;

    /* return zero for complete set, positive for incomplete set */
    return left;
}

/*
 * Decode literal/length and distance codes until an end-of-block code.
 *
 * Format notes:
 *
 * - Compressed data that is after the block type if fixed or after the code
 *   description if dynamic is a combination of literals and length/distance
 *   pairs terminated by and end-of-block code.  Literals are simply Huffman
 *   coded bytes.  A length/distance pair is a coded length followed by a
 *   coded distance to represent a string that occurs earlier in the
 *   uncompressed data that occurs again at the current location.
 *
 * - Literals, lengths, and the end-of-block code are combined into a single
 *   code of up to 286 symbols.  They are 256 literals (0..255), 29 length
 *   symbols (257..285), and the end-of-block symbol (256).
 *
 * - There are 256 possible lengths (3..258), and so 29 symbols are not enough
 *   to represent all of those.  Lengths 3..10 and 258 are in fact represented
 *   by just a length symbol.  Lengths 11..257 are represented as a symbol and
 *   some number of extra bits that are added as an integer to the base length
 *   of the length symbol.  The number of extra bits is determined by the base
 *   length symbol.  These are in the static arrays below, lens[] for the base
 *   lengths and lext[] for the corresponding number of extra bits.
 *
 * - The reason that 258 gets its own symbol is that the longest length is used
 *   often in highly redundant files.  Note that 258 can also be coded as the
 *   base value 227 plus the maximum extra value of 31.  While a good deflate
 *   should never do this, it is not an error, and should be decoded properly.
 *
 * - If a length is decoded, including its extra bits if any, then it is
 *   followed a distance code.  There are up to 30 distance symbols.  Again
 *   there are many more possible distances (1..32768), so extra bits are added
 *   to a base value represented by the symbol.  The distances 1..4 get their
 *   own symbol, but the rest require extra bits.  The base distances and
 *   corresponding number of extra bits are below in the static arrays dist[]
 *   and dext[].
 *
 * - Literal bytes are simply written to the output.  A length/distance pair is
 *   an instruction to copy previously uncompressed bytes to the output.  The
 *   copy is from distance bytes back in the output stream, copying for length
 *   bytes.
 *
 * - Distances pointing before the beginning of the output data are not
 *   permitted.
 *
 * - Overlapped copies, where the length is greater than the distance, are
 *   allowed and common.  For example, a distance of one and a length of 258
 *   simply copies the last byte 258 times.  A distance of four and a length of
 *   twelve copies the last four bytes three times.  A simple forward copy
 *   ignoring whether the length is greater than the distance or not implements
 *   this correctly.  You should not use memcpy() since its behavior is not
 *   defined for overlapped arrays.  You should not use memmove() or bcopy()
 *   since though their behavior -is- defined for overlapping arrays, it is
 *   defined to do the wrong thing in this case.
 */
local int codes(struct state *s,
                struct huffman *lencode,
                struct huffman *distcode)
{
    int symbol;         /* decoded symbol */
    int len;            /* length for copy */
    unsigned dist;      /* distance for copy */
    static const short lens[29] = { /* Size base for length codes 257..285 */
        3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
        35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
    static const short lext[29] = { /* Extra bits for length codes 257..285 */
        0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
        3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
    static const short dists[30] = { /* Offset base for distance codes 0..29 */
        1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
        257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
        8193, 12289, 16385, 24577};
    static const short dext[30] = { /* Extra bits for distance codes 0..29 */
        0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
        7, 7, 8, 8, 9, 9, 10, 10, 11, 11,
        12, 12, 13, 13};

    /* decode literals and length/distance pairs */
    do {
        symbol = decode(s, lencode);
        if (symbol < 0) return symbol;  /* invalid symbol */
        if (symbol < 256) {             /* literal: symbol is the byte */
            /* write out the literal */
            if (s->out != NIL) {
                if (s->outcnt == s->outlen) return 1;
                s->out[s->outcnt] = symbol;
            }
            s->outcnt++;
        }
        else if (symbol > 256) {        /* length */
            /* get and compute length */
            symbol -= 257;
            if (symbol >= 29) return -9;        /* invalid fixed code */
            len = lens[symbol] + bits(s, lext[symbol]);

            /* get and check distance */
            symbol = decode(s, distcode);
            if (symbol < 0) return symbol;      /* invalid symbol */
            dist = dists[symbol] + bits(s, dext[symbol]);
            if (dist > s->outcnt)
                return -10;     /* distance too far back */

            /* copy length bytes from distance bytes back */
            if (s->out != NIL) {
                if (s->outcnt + len > s->outlen) return 1;
                while (len--) {
                    s->out[s->outcnt] = s->out[s->outcnt - dist];
                    s->outcnt++;
                }
            }
            else
                s->outcnt += len;
        }
    } while (symbol != 256);            /* end of block symbol */

    /* done with a valid fixed or dynamic block */
    return 0;
}

/*
 * Process a fixed codes block.
 *
 * Format notes:
 *
 * - This block type can be useful for compressing small amounts of data for
 *   which the size of the code descriptions in a dynamic block exceeds the
 *   benefit of custom codes for that block.  For fixed codes, no bits are
 *   spent on code descriptions.  Instead the code lengths for literal/length
 *   codes and distance codes are fixed.  The specific lengths for each symbol
 *   can be seen in the "for" loops below.
 *
 * - The literal/length code is complete, but has two symbols that are invalid
 *   and should result in an error if received.  This cannot be implemented
 *   simply as an incomplete code since those two symbols are in the "middle"
 *   of the code.  They are eight bits long and the longest literal/length\
 *   code is nine bits.  Therefore the code must be constructed with those
 *   symbols, and the invalid symbols must be detected after decoding.
 *
 * - The fixed distance codes also have two invalid symbols that should result
 *   in an error if received.  Since all of the distance codes are the same
 *   length, this can be implemented as an incomplete code.  Then the invalid
 *   codes are detected while decoding.
 */
local int fixed(struct state *s)
{
    static int virgin = 1;
    static short lencnt[MAXBITS+1], lensym[FIXLCODES];
    static short distcnt[MAXBITS+1], distsym[MAXDCODES];
    static struct huffman lencode = {lencnt, lensym};
    static struct huffman distcode = {distcnt, distsym};

    /* build fixed huffman tables if first call (may not be thread safe) */
    if (virgin) {
        int symbol;
        short lengths[FIXLCODES];

        /* literal/length table */
        for (symbol = 0; symbol < 144; symbol++)
            lengths[symbol] = 8;
        for (; symbol < 256; symbol++)
            lengths[symbol] = 9;
        for (; symbol < 280; symbol++)
            lengths[symbol] = 7;
        for (; symbol < FIXLCODES; symbol++)
            lengths[symbol] = 8;
        construct(&lencode, lengths, FIXLCODES);

        /* distance table */
        for (symbol = 0; symbol < MAXDCODES; symbol++)
            lengths[symbol] = 5;
        construct(&distcode, lengths, MAXDCODES);

        /* do this just once */
        virgin = 0;
    }

    /* decode data until end-of-block code */
    return codes(s, &lencode, &distcode);
}

/*
 * Process a dynamic codes block.
 *
 * Format notes:
 *
 * - A dynamic block starts with a description of the literal/length and
 *   distance codes for that block.  New dynamic blocks allow the compressor to
 *   rapidly adapt to changing data with new codes optimized for that data.
 *
 * - The codes used by the deflate format are "canonical", which means that
 *   the actual bits of the codes are generated in an unambiguous way simply
 *   from the number of bits in each code.  Therefore the code descriptions
 *   are simply a list of code lengths for each symbol.
 *
 * - The code lengths are stored in order for the symbols, so lengths are
 *   provided for each of the literal/length symbols, and for each of the
 *   distance symbols.
 *
 * - If a symbol is not used in the block, this is represented by a zero as
 *   as the code length.  This does not mean a zero-length code, but rather
 *   that no code should be created for this symbol.  There is no way in the
 *   deflate format to represent a zero-length code.
 *
 * - The maximum number of bits in a code is 15, so the possible lengths for
 *   any code are 1..15.
 *
 * - The fact that a length of zero is not permitted for a code has an
 *   interesting consequence.  Normally if only one symbol is used for a given
 *   code, then in fact that code could be represented with zero bits.  However
 *   in deflate, that code has to be at least one bit.  So for example, if
 *   only a single distance base symbol appears in a block, then it will be
 *   represented by a single code of length one, in particular one 0 bit.  This
 *   is an incomplete code, since if a 1 bit is received, it has no meaning,
 *   and should result in an error.  So incomplete distance codes of one symbol
 *   should be permitted, and the receipt of invalid codes should be handled.
 *
 * - It is also possible to have a single literal/length code, but that code
 *   must be the end-of-block code, since every dynamic block has one.  This
 *   is not the most efficient way to create an empty block (an empty fixed
 *   block is fewer bits), but it is allowed by the format.  So incomplete
 *   literal/length codes of one symbol should also be permitted.
 *
 * - If there are only literal codes and no lengths, then there are no distance
 *   codes.  This is represented by one distance code with zero bits.
 *
 * - The list of up to 286 length/literal lengths and up to 30 distance lengths
 *   are themselves compressed using Huffman codes and run-length encoding.  In
 *   the list of code lengths, a 0 symbol means no code, a 1..15 symbol means
 *   that length, and the symbols 16, 17, and 18 are run-length instructions.
 *   Each of 16, 17, and 18 are follwed by extra bits to define the length of
 *   the run.  16 copies the last length 3 to 6 times.  17 represents 3 to 10
 *   zero lengths, and 18 represents 11 to 138 zero lengths.  Unused symbols
 *   are common, hence the special coding for zero lengths.
 *
 * - The symbols for 0..18 are Huffman coded, and so that code must be
 *   described first.  This is simply a sequence of up to 19 three-bit values
 *   representing no code (0) or the code length for that symbol (1..7).
 *
 * - A dynamic block starts with three fixed-size counts from which is computed
 *   the number of literal/length code lengths, the number of distance code
 *   lengths, and the number of code length code lengths (ok, you come up with
 *   a better name!) in the code descriptions.  For the literal/length and
 *   distance codes, lengths after those provided are considered zero, i.e. no
 *   code.  The code length code lengths are received in a permuted order (see
 *   the order[] array below) to make a short code length code length list more
 *   likely.  As it turns out, very short and very long codes are less likely
 *   to be seen in a dynamic code description, hence what may appear initially
 *   to be a peculiar ordering.
 *
 * - Given the number of literal/length code lengths (nlen) and distance code
 *   lengths (ndist), then they are treated as one long list of nlen + ndist
 *   code lengths.  Therefore run-length coding can and often does cross the
 *   boundary between the two sets of lengths.
 *
 * - So to summarize, the code description at the start of a dynamic block is
 *   three counts for the number of code lengths for the literal/length codes,
 *   the distance codes, and the code length codes.  This is followed by the
 *   code length code lengths, three bits each.  This is used to construct the
 *   code length code which is used to read the remainder of the lengths.  Then
 *   the literal/length code lengths and distance lengths are read as a single
 *   set of lengths using the code length codes.  Codes are constructed from
 *   the resulting two sets of lengths, and then finally you can start
 *   decoding actual compressed data in the block.
 *
 * - For reference, a "typical" size for the code description in a dynamic
 *   block is around 80 bytes.
 */
local int dynamic(struct state *s)
{
    int nlen, ndist, ncode;             /* number of lengths in descriptor */
    int index;                          /* index of lengths[] */
    int err;                            /* construct() return value */
    short lengths[MAXCODES];            /* descriptor code lengths */
    short lencnt[MAXBITS+1], lensym[MAXLCODES];         /* lencode memory */
    short distcnt[MAXBITS+1], distsym[MAXDCODES];       /* distcode memory */
    struct huffman lencode = {lencnt, lensym};          /* length code */
    struct huffman distcode = {distcnt, distsym};       /* distance code */
    static const short order[19] =      /* permutation of code length codes */
        {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

    /* get number of lengths in each table, check lengths */
    nlen = bits(s, 5) + 257;
    ndist = bits(s, 5) + 1;
    ncode = bits(s, 4) + 4;
    if (nlen > MAXLCODES || ndist > MAXDCODES)
        return -3;                      /* bad counts */

    /* read code length code lengths (really), missing lengths are zero */
    for (index = 0; index < ncode; index++)
        lengths[order[index]] = bits(s, 3);
    for (; index < 19; index++)
        lengths[order[index]] = 0;

    /* build huffman table for code lengths codes (use lencode temporarily) */
    err = construct(&lencode, lengths, 19);
    if (err != 0) return -4;            /* require complete code set here */

    /* read length/literal and distance code length tables */
    index = 0;
    while (index < nlen + ndist) {
        int symbol;             /* decoded value */
        int len;                /* last length to repeat */

        symbol = decode(s, &lencode);
        if (symbol < 16)                /* length in 0..15 */
            lengths[index++] = symbol;
        else {                          /* repeat instruction */
            len = 0;                    /* assume repeating zeros */
            if (symbol == 16) {         /* repeat last length 3..6 times */
                if (index == 0) return -5;      /* no last length! */
                len = lengths[index - 1];       /* last length */
                symbol = 3 + bits(s, 2);
            }
            else if (symbol == 17)      /* repeat zero 3..10 times */
                symbol = 3 + bits(s, 3);
            else                        /* == 18, repeat zero 11..138 times */
                symbol = 11 + bits(s, 7);
            if (index + symbol > nlen + ndist)
                return -6;              /* too many lengths! */
            while (symbol--)            /* repeat last or zero symbol times */
                lengths[index++] = len;
        }
    }

    /* build huffman table for literal/length codes */
    err = construct(&lencode, lengths, nlen);
    if (err < 0 || (err > 0 && nlen - lencode.count[0] != 1))
        return -7;      /* only allow incomplete codes if just one code */

    /* build huffman table for distance codes */
    err = construct(&distcode, lengths + nlen, ndist);
    if (err < 0 || (err > 0 && ndist - distcode.count[0] != 1))
        return -8;      /* only allow incomplete codes if just one code */

    /* decode data until end-of-block code */
    return codes(s, &lencode, &distcode);
}

/*
 * Inflate source to dest.  On return, destlen and sourcelen are updated to the
 * size of the uncompressed data and the size of the deflate data respectively.
 * On success, the return value of puff() is zero.  If there is an error in the
 * source data, i.e. it is not in the deflate format, then a negative value is
 * returned.  If there is not enough input available or there is not enough
 * output space, then a positive error is returned.  In that case, destlen and
 * sourcelen are not updated to facilitate retrying from the beginning with the
 * provision of more input data or more output space.  In the case of invalid
 * inflate data (a negative error), the dest and source pointers are updated to
 * facilitate the debugging of deflators.
 *
 * puff() also has a mode to determine the size of the uncompressed output with
 * no output written.  For this dest must be (unsigned char *)0.  In this case,
 * the input value of *destlen is ignored, and on return *destlen is set to the
 * size of the uncompressed output.
 *
 * The return codes are:
 *
 *   2:  available inflate data did not terminate
 *   1:  output space exhausted before completing inflate
 *   0:  successful inflate
 *  -1:  invalid block type (type == 3)
 *  -2:  stored block length did not match one's complement
 *  -3:  dynamic block code description: too many length or distance codes
 *  -4:  dynamic block code description: code lengths codes incomplete
 *  -5:  dynamic block code description: repeat lengths with no first length
 *  -6:  dynamic block code description: repeat more than specified lengths
 *  -7:  dynamic block code description: invalid literal/length code lengths
 *  -8:  dynamic block code description: invalid distance code lengths
 *  -9:  invalid literal/length or distance code in fixed or dynamic block
 * -10:  distance is too far back in fixed or dynamic block
 *
 * Format notes:
 *
 * - Three bits are read for each block to determine the kind of block and
 *   whether or not it is the last block.  Then the block is decoded and the
 *   process repeated if it was not the last block.
 *
 * - The leftover bits in the last byte of the deflate data after the last
 *   block (if it was a fixed or dynamic block) are undefined and have no
 *   expected values to check.
 */
int puff(unsigned char *dest,           /* pointer to destination pointer */
         unsigned long *destlen,        /* amount of output space */
         unsigned char *source,         /* pointer to source data pointer */
         unsigned long *sourcelen)      /* amount of input available */
{
    struct state s;             /* input/output state */
    int last, type;             /* block information */
    int err;                    /* return value */

    /* initialize output state */
    s.out = dest;
    s.outlen = *destlen;                /* ignored if dest is NIL */
    s.outcnt = 0;

    /* initialize input state */
    s.in = source;
    s.inlen = *sourcelen;
    s.incnt = 0;
    s.bitbuf = 0;
    s.bitcnt = 0;

    /* return if bits() or decode() tries to read past available input */
    if (setjmp(s.env) != 0)             /* if came back here via longjmp() */
        err = 2;                        /* then skip do-loop, return error */
    else {
        /* process blocks until last block or error */
        do {
            last = bits(&s, 1);         /* one if last block */
            type = bits(&s, 2);         /* block type 0..3 */
            err = type == 0 ? stored(&s) :
                  (type == 1 ? fixed(&s) :
                   (type == 2 ? dynamic(&s) :
                    -1));               /* type == 3, invalid */
            if (err != 0) break;        /* return with error */
        } while (!last);
    }

    /* update the lengths and return */
    if (err <= 0) {
        *destlen = s.outcnt;
        *sourcelen = s.incnt;
    }
    return err;
}





































/* 
 *  lpng.h, version 16-01-2009
 *
 *  Copyright (c) 2009, Alex Pankratov, ap-at-swapped-dot-cc
 *
 *  Permission is hereby granted, free of charge, to any person
 *  obtaining a copy of this software and associated documentation
 *  files (the "Software"), to deal in the Software without
 *  restriction, including without limitation the rights to use,
 *  copy, modify, merge, publish, distribute, sublicense, and/or 
 *  sell copies of the Software, and to permit persons to whom the
 *  Software is furnished to do so, subject to the following
 *  conditions:
 *  
 *  The above copyright notice and this permission notice shall be
 *  included in all copies or substantial portions of the Software.
 *  
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 *  OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *  HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 *  OTHER DEALINGS IN THE SOFTWARE.
 */


/* 
 *  lpng.c, version 16-01-2009
 *
 *  Copyright (c) 2009, Alex Pankratov, ap-at-swapped-dot-cc
 *
 *  Permission is hereby granted, free of charge, to any person
 *  obtaining a copy of this software and associated documentation
 *  files (the "Software"), to deal in the Software without
 *  restriction, including without limitation the rights to use,
 *  copy, modify, merge, publish, distribute, sublicense, and/or 
 *  sell copies of the Software, and to permit persons to whom the
 *  Software is furnished to do so, subject to the following
 *  conditions:
 *  
 *  The above copyright notice and this permission notice shall be
 *  included in all copies or substantial portions of the Software.
 *  
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 *  OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *  HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 *  OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 *	The following constant limits the maximum size of a PNG 
 *	image. Larger images are not loaded. 
 *
 *	The limit is arbitrary and can be increased as needed.
 *	It exists solely to prevent the accidental loading of 
 *	very large images.
 */
#define MAX_IDAT_SIZE (16*1024*1024)

/*
 *
 */
#pragma warning (disable: 4996)

typedef unsigned char  uchar;
typedef unsigned long  ulong;
typedef struct _png    png_t;
typedef struct _buf    buf_t;

struct _png
{
	ulong w;
	ulong h;
	uchar bpp; /* 3 - truecolor, 4 - truecolor + alpha */
	uchar pix[1];
};

struct _buf
{
	uchar * ptr;
	ulong   len;
};

typedef int (* read_cb)(uchar * buf, ulong len, void * arg);

//
static __inline ulong get_ulong(uchar * v)
{
	ulong r = v[0];
	r = (r << 8) | v[1];
	r = (r << 8) | v[2];
	return (r << 8) | v[3];
}

static __inline uchar paeth(uchar a, uchar b, uchar c)
{
	int p = a + b - c;
        int pa = p > a ? p-a : a-p;
	int pb = p > b ? p-b : b-p;
	int pc = p > c ? p-c : c-p;
        return (pa <= pb && pa <= pc) ? a : (pb <= pc) ? b : c;
}

static const uchar png_sig[] = { 137, 80, 78, 71, 13, 10, 26, 10 };

/*
 *
 */
static png_t * LoadPngEx(read_cb read, void * read_arg)
{
	png_t * png = NULL;

	uchar buf[64];
	ulong w, h, i, j;
	uchar bpp;
	ulong len;
	uchar * tmp, * dat = 0;
	ulong dat_max, dat_len;
	ulong png_max, png_len;
	ulong out_len;
	int   r;
	uchar * line, * prev;

	/* sig and IHDR */
	if (! read(buf, 8 + 8 + 13 + 4, read_arg))
		return NULL;

	if (memcmp(buf, png_sig, 8) != 0)
		return NULL;

	len = get_ulong(buf+8);
	if (len != 13)
		return NULL;
	
	if (memcmp(buf+12, "IHDR", 4) != 0)
		return NULL;

	w = get_ulong(buf+16);
	h = get_ulong(buf+20);

	if (buf[24] != 8)
		return NULL;
	
	/* truecolor pngs only please */
	if ((buf[25] & 0x03) != 0x02)
		return NULL;

	/* check for the alpha channel */
	bpp = (buf[25] & 0x04) ? 4 : 3;

	if (buf[26] != 0 || buf[27] != 0 || buf[28] != 0)
		return NULL;

	/* IDAT */
	dat_max = 0;
	for (;;)
	{
		if (! read(buf, 8, read_arg))
			goto err;

		len = get_ulong(buf);
		if (memcmp(buf+4, "IDAT", 4) != 0)
		{
			if (! read(0, len+4, read_arg))
				goto err;

			if (memcmp(buf+4, "IEND", 4) == 0)
				break;
		
			continue;
		}

		/*
		 *	see comment at the top of this file
		 */
		if (len > MAX_IDAT_SIZE)
			goto err;

		if (len+4 > dat_max)
		{
			dat_max = len+4;
			tmp = (uchar*)realloc(dat, dat_max);
			if (! tmp)
				goto err;
			dat = tmp;
		}

		if (! read(dat, len+4, read_arg))
			goto err;

		if ((dat[0] & 0x0f) != 0x08 ||  /* compression method (rfc 1950) */
		    (dat[0] & 0xf0) > 0x70)     /* window size */
			goto err;

		if ((dat[1] & 0x20) != 0)       /* preset dictionary present */
			goto err;

		if (! png)
		{
			png_max = (w * bpp + 1) * h;
			png_len = 0;
			png = (png_t*)malloc(sizeof(*png) - 1 + png_max);
			if (! png)
				goto err;
			png->w = w;
			png->h = h;
			png->bpp = bpp;
		}

		out_len = png_max - png_len;
		dat_len = len - 2;
		r = puff(png->pix + png_len, &out_len, dat+2, &dat_len);
		if (r != 0)
			goto err;
		if (2+dat_len+4 != len)
			goto err;
		png_len += out_len;
	}
	free(dat);

	/* unfilter */
	len = w * bpp + 1;
	line = png->pix;
	prev = 0;
	for (i=0; i<h; i++, prev = line, line += len)
	{
		switch (line[0])
		{
		case 0: /* none */
			break;
		case 1: /* sub */
			for (j=1+bpp; j<len; j++)
				line[j] += line[j-bpp];
			break;
		case 2: /* up */
			if (! prev)
				break;
			for (j=1; j<len; j++)
				line[j] += prev[j];
			break;
		case 3: /* avg */
			if (! prev)
				goto err; /* $todo */
			for (j=1; j<=bpp; j++)
				line[j] += prev[j]/2;
			for (   ; j<len; j++)
				line[j] += (line[j-bpp] + prev[j])/2;
			break;
		case 4: /* paeth */
			if (! prev)
				goto err; /* $todo */
			for (j=1; j<=bpp; j++)
				line[j] += prev[j];
			for (   ; j<len; j++)
				line[j] += paeth(line[j-bpp], prev[j], prev[j-bpp]);			
			break;
		default:
			goto err;
		}
	}
	
	return png;
err:
	free(png);
	free(dat);
	return NULL;
}

/*
 *
 */
static int file_reader(uchar * buf, ulong len, void * arg)
{
	FILE * fh = (FILE*)arg;
	return buf ? 
		fread(buf, 1, len, fh) == len :
		fseek(fh, len, SEEK_CUR) == 0;
}

static png_t * LoadPngFile(const TCHAR * name)
{
	png_t * png = NULL;
	FILE  * fh;
#ifdef _UNICODE
	fh = _wfopen(name, L"rb");
#else
	fh = fopen(name,"rb");
#endif
	if (fh)
	{
		png = LoadPngEx(file_reader, fh);
		fclose(fh); 
	}

	return png;
}

/*
 *
 */
static int data_reader(uchar * buf, ulong len, void * arg)
{
	buf_t * m = (buf_t*)arg;
	
	if (len > m->len)
		return 0;

	if (buf)
		memcpy(buf, m->ptr, len);

	m->len -= len;
	m->ptr += len;
	return 1;
}

static png_t * LoadPngResource(const TCHAR* name, const TCHAR * type, HMODULE module)
{
	HRSRC   hRes;
	HGLOBAL hResData;
	buf_t   buf;

	hRes = FindResource(module, name, type);
	if (! hRes)
		return NULL;

	if (! SizeofResource(0, hRes))
		return NULL;

	if (! (hResData = LoadResource(0, hRes)))
		return NULL;

	if (! (buf.ptr = (uchar*)LockResource(hResData)))
		return NULL;

	buf.len = SizeofResource(0, hRes);

	return LoadPngEx(data_reader, &buf);
}

/*
 *
 */
static HBITMAP PngToDib(png_t * png, BOOL premultiply)
{
	BITMAPINFO bmi = { sizeof(bmi) };
	HBITMAP dib;
	uchar * dst;
	uchar * src, * tmp;
	uchar  alpha;
	int    bpl;
	ulong  x, y;

	if (png->bpp != 3 && png->bpp != 4)
		return NULL;

	//
	bmi.bmiHeader.biWidth = png->w;
	bmi.bmiHeader.biHeight = png->h;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;
	bmi.bmiHeader.biSizeImage = 0;
	bmi.bmiHeader.biXPelsPerMeter = 0;
	bmi.bmiHeader.biYPelsPerMeter = 0;
	bmi.bmiHeader.biClrUsed = 0;
	bmi.bmiHeader.biClrImportant = 0;
	
	dib = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, (void**)&dst, NULL, 0);
	if (! dib)
		return NULL;

	bpl = png->bpp * png->w + 1;              // bytes per line
	src = png->pix + (png->h - 1) * bpl + 1;  // start of the last line

	for (y = 0; y < png->h; y++, src -= bpl)
	{
		tmp = src;

		for (x = 0; x < png->w; x++)
		{
			alpha = (png->bpp == 4) ? tmp[3] : 0xff; // $fixme - optimize

			/*
			 *	R, G and B need to be 'pre-multiplied' to alpha as 
			 *	this is the format that AlphaBlend() expects from
			 *	an alpha-transparent DIB
			 */
			if (premultiply)
			{
				dst[0] = tmp[2] * alpha / 255;
				dst[1] = tmp[1] * alpha / 255;
				dst[2] = tmp[0] * alpha / 255;
			}
			else
			{
				dst[0] = tmp[2];
				dst[1] = tmp[1];
				dst[2] = tmp[0];
			}
			dst[3] = alpha;

			dst += 4;
			tmp += png->bpp;
		}
	}

	return dib;
}

/*
 *
 */
HBITMAP LoadPng(const TCHAR * res_name, 
		const TCHAR * res_type, 
		HMODULE         res_inst,
		BOOL            premultiply)
{
	HBITMAP  bmp;
	png_t  * png;

	if (res_type)
		png = LoadPngResource(res_name, res_type, res_inst);
	else
		png = LoadPngFile(res_name);

	if (! png)
		return NULL;

	bmp = PngToDib(png, premultiply);
	
	free(png);
	if (! bmp)
		return NULL;

	return bmp;
}
