/*******************************************************************************
  cs3388lib.h -- Utilities for Introduction to Computer Graphics

  DESCRIPTION:
     Provides simple functions to:
        - create and draw simple bitmaps (BGRA pixel format only)
        - draw lines, rectangles, and circles into bitmaps
        - read and write bitmap files (read PNG/BMP, write BMP)
        - draw bitmaps into an OpenGL framebuffer
        - simpler random number generation

*******************************************************************************/

#ifndef __CS3388LIB_H__
#define __CS3388LIB_H__

// bitmap
//   The minimal information needed to store/draw a bitmap.
//   The pixel format is assumed to be BGRA.
//
struct bitmap {
	int wd,ht;
	unsigned char* pixels;
};

// bitmap_load
//    Reads a PNG or BMP file from disk.
//    Use bitmap_delete if the bitmap is no longer needed.
//
// Example:
//    bitmap* bm = bitmap_load("uwologo.png"); // Load image from file
//    gl_drawbitmap(uwologo,0,0);              // Draw at top-left corner of window
//
bitmap* bitmap_load(const char* filename);

// bitmap_save
//    Saves a bitmap to disk with the given file name.
//    Can only write to BMP files (.bmp).
//  
void bitmap_save(bitmap* bm, const char* filename);

// bitmap_create
//    Allocates a bitmap with enough BGRA pixels for the size specified.
//
// Example:
//    bitmap* bm = bitmap_create(32,20); // Allocate bitmap memory
//    for (int i = 0; i < 32*20; ++i) {
//        bm->pixels[4*i+0] = 255;       // Make entire bitmap pure blue
//        bm->pixels[4*i+1] = 0;
//        bm->pixels[4*i+2] = 0;
//        bm->pixels[4*i+3] = 255;       // (and completely opaque, in case blending)
//    }
//
bitmap* bitmap_create(int width, int height);

// bitmap_pixel
//    Returns a pointer to the raw bytes that define the
//    image's colour data. Only needed if you want to 
//    inspect/modify specific pixels. 
//
// Example:
//    bitmap* bm = bitmap_create(9,9);
//    for (int x = 0; x < 9; ++x) {
//        unsigned char* p = bitmap_pixel(bm,x,4); // pointer to pixel (x,4)
//        p[0] = 0; p[1] = 0; p[2] = 255;          // set it to red
//    }
//
unsigned char* bitmap_pixel(bitmap* bm, int x, int y);

// bitmap_delete
//    Releases any pixel memory associated with the bitmap,
//    including the 'bitmap' struct itself.
//
void bitmap_delete(bitmap* bm);

////////////////////////////////////////////////////////////////

// drawrect, fillrect
//    Draws a rectangle shape into the bitmap, coloured (r,g,b).
//    The rectangle will extend from (x0,y0) to (x1,y1),
//    excluding the right-most column and bottom-most row. 
//    (This is standard for rectangle drawing functions.)
//
// Example:
//    bitmap* bm = bitmap_create(9,9);
//    drawrect(bm, 4,0, 5,9, 255,0,0); // set pixels (4,0)..(4,8) to red.
//
void drawrect(bitmap* bm, int x0, int y0, int x1, int y1, 
              unsigned char r, unsigned char g, unsigned char b);

void fillrect(bitmap* bm, int x0, int y0, int x1, int y1, 
              unsigned char r, unsigned char g, unsigned char b);

// drawline
//    Draws a line into the bitmap, coloured (r,g,b).
//    The line will interpolate from (x0,y0) to (x1,y1),
//    excluding the point (x1,y1) itself. 
//    (This is standard for line drawing functions.)
//
void drawline(bitmap* bm, int x0, int y0, int x1, int y1, 
              unsigned char r, unsigned char g, unsigned char b);

// drawcircle
//    Draws a circle into the bitmap, coloured (r,g,b).
//    The circle will be centered at (x,y) and have the given radius.
//    (Use radius 1 to draw a 1-pixel point, or use bitmap_pixel.)
//
void drawcircle(bitmap* bm, int x, int y, int radius, 
                unsigned char r, unsigned char g, unsigned char b);


////////////////////////////////////////////////////////////////

// gl_drawbitmap
//    Draws a bitmap into the OpenGL framebuffer.
//    The (left,top) coordinate is specified in window coordinates, 
//    with (0,0) being the top-left corner of the GLUT window.
//    If the (right,bottom) are specified, then the bitmap will 
//    be stretched to fill the rectangly (left,top,right-1,bottom-1).
//    (This is standard and similar to fillrect.)
//
// Example:
//    bitmap* bm = bitmap_load("uwologo.png");
//    int window_wd = glutGet(GLUT_WINDOW_WIDTH);
//    int window_ht = glutGet(GLUT_WINDOW_HEIGHT);
//    gl_drawbitmap(bm,0,window_wd,0,window_ht);   // stretch logo to fit entire window
// 
void   gl_drawbitmap(bitmap* bm, int left, int top);
void   gl_drawbitmap(bitmap* bm, int left, int right, int top, int bottom);

// gl_drawstring
//    Draws text in 'str' to the screen using colour (r,g,b)
//    The location (left,top) is in window coordinates, i.e.
//    in pixels from top-left corner of window.
void   gl_drawstring(const char* str, int left, int top, unsigned char r, unsigned char g, unsigned char b);

// gl_createprogram
//    Creates an OpenGL 2.1 or 3.0 GLSL shader program from the given
//    vertex program and fragment program source code.
//    Returns the GL identifier for the linked program, which can
//    be passed to glUseProgram etc.
//
unsigned gl_createprogram(const char* vscode, const char* fscode);

// gl_createprogram
//    Same as gl_createprogram but reads code from files on disk.
//
unsigned gl_loadprogram(const char* vsfile, const char* fsfile);

// gl_loadtexture
//    Loads a BMP or PNG bitmap, registers it with OpenGL, 
//    builds mipmaps, and returns the new OpenGL texture identifier
unsigned gl_loadtexture(const char* filename);


////////////////////////////////////////////////////////////////

// random_int
//    Returns a random, non-negative integer.
//
// Example:
//    int percent = random_int() % 101;  // Random integer from 0..100
//
int random_int();

// random_float
//    Returns a random float in the range [0,1).
//
// Example:
//    float percent = random_float() * 100;  // Random float in range [0,100)
//
float random_float();

////////////////////////////////////////////////////////////////

#ifdef assert
#undef assert
#endif

#define assert(expr)            { if (expr) { } else { assert_dialog(#expr, __FILE__, __LINE__); assert_break; } }
#define assert_msg(expr,msg)    { if (expr) { } else { assert_dialog(msg, __FILE__, __LINE__); assert_break; } }
#define assert_unreachable(msg) { assert_dialog(msg, __FILE__, __LINE__); assert_break; }
#ifdef _WIN64
#define assert_break __debugbreak();
#else
#define assert_break __asm { int 0x3 }
#endif

// do not use this function directly; rely on one of the macros above
void assert_dialog(const char* msg, const char* file, int line);

#undef near
#undef far

#endif // __CS3388LIB_H__
