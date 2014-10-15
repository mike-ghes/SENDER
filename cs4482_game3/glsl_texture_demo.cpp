// Much of the code in this demo is similar to glsl_demo, so some
// old comments have been stripped so as not to repeat them here.

#include "gl3w.h"     // OpenGL 3.0
#include "glut.h"
#include "cs3388lib.h"
#include "vec2.h"
#include "vec4.h"
#include "mat4x4.h"

#define OFFSET(type,member) (void*)(offsetof(type,member))  // what is the byte offset of type::member?

GLuint mesh_vbo = 0;  // id for our list of 3D vertices
GLuint program = 0;   // id for our GLSL program
GLuint texid = 0;     // id for texture
float angle = 0;
bool animate = true;

// For this demo, we want to interpolate more than just position,
// so we need each 'vertex' to contain more attributes.
// Later on, we will tell OpenGL exactly how we've structured the vertex data.
struct vertex {
	vec4 p;  // position
	vec4 c;  // per-vertex colour
	vec2 t;  // per-vertex texcoord

	// constructor to allow vertex v(vec4(...),vec4(...),vec2(...))
	//          instead of  vertex v = { vec4(...), vec4(...), vec2(...) }
	vertex(vec4 p, vec4 c, vec2 t): p(p), c(c), t(t) { }
};

void redraw()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);  // clear color, and reset the z-buffer
	glEnable(GL_DEPTH_TEST);                             // enable z-buffer test

	mat4x4 P = perspective(-1,1,-1,1,-1,-10);
	mat4x4 M = translation(0,0,-3)*rotation_z(angle)*rotation_y(2*angle);

	// Bind our vertices, texture, and GLSL program
	glBindBuffer(GL_ARRAY_BUFFER,mesh_vbo);
	glBindTexture(GL_TEXTURE_2D,texid);
	glUseProgram(program);

	// Initialize the GLSL uniform variables
	glUniformMatrix4fv(glGetUniformLocation(program,"P"),1,GL_TRUE,P.ptr());
	glUniformMatrix4fv(glGetUniformLocation(program,"M"),1,GL_TRUE,M.ptr());
	// Since our fragment shader also has a uniform (a 'sampler2D' texture index name 'texmap'),
	// we should tell our fragment shader that 'texmap' was bound to "texture unit 0" (the default)
	glUniform1i(glGetUniformLocation(program,"texmap"),0);

	int ploc = glGetAttribLocation(program,"p");  // index of position attribute in GLSL program
	int cloc = glGetAttribLocation(program,"c");  // index of colour attribute
	int tloc = glGetAttribLocation(program,"t");  // index of texcoord attribute

	// Each vertex contains a position, colour, and texcoord, so we must
	// tell OpenGL exactly how the memory of our vertex buffer is layed out.
	// sizeof(vertex) = 16 + 16 + 8 = 40 bytes
	//   position: "4 floats, 40 bytes apart, with offset 0 bytes"
	//   colour:   "4 floats, 40 bytes apart, with offset 16 bytes"
	//   colour:   "2 floats, 40 bytes apart, with offset 32 bytes"
	glVertexAttribPointer(ploc,4,GL_FLOAT,GL_FALSE,sizeof(vertex),OFFSET(vertex,p));
	glVertexAttribPointer(cloc,4,GL_FLOAT,GL_FALSE,sizeof(vertex),OFFSET(vertex,c));
	glVertexAttribPointer(tloc,2,GL_FLOAT,GL_FALSE,sizeof(vertex),OFFSET(vertex,t));
	glEnableVertexAttribArray(ploc); // don't forget to enable each one!
	glEnableVertexAttribArray(cloc);
	glEnableVertexAttribArray(tloc);

	// RASTERIZE!
	glDrawArrays(GL_TRIANGLE_STRIP,0,4);	
	
	// OPTIONAL
	glUseProgram(0);
	glBindBuffer(GL_ARRAY_BUFFER,0);

	glFlush();
}


void init_program()
{
	const char* vscode =            // vertex shader code
		"#version 120      \n"

		"uniform mat4 P;   \n"
		"uniform mat4 M;   \n"

		"attribute vec4 p; \n"      // the (x,y,z,1) point that we should process
		"attribute vec4 c; \n"      // the (r,g,b,a) colour for point 'p'
		"attribute vec2 t; \n"      // the (u,v) texcoord for point 'p'

		"varying   vec4 colour;    \n" // must explicitly ask for colour and texcoord to be 
		"varying   vec2 texcoord;  \n" // interpolated between points and passed to fragment

		"void main()               \n"
		"{                         \n"
		"	gl_Position = P*M*p;   \n" // transform model-coordinates to clip-coordinates
		"	colour = c;            \n" // 
		"	texcoord = t;          \n" // 
		"}                         \n";

	const char* fscode =            // fragment shader code
		"#version 120              \n" 

		"uniform sampler2D texmap; \n" // get ready to sample a texture that we call 'texmap'

		"varying vec4 colour;      \n" // the *interpolated* colour (r,g,b,a) at this fragment
		"varying vec2 texcoord;    \n" // the *interpolated* texcoord (u,v) at this fragment

		"void main()               \n"
		"{\n"
		"	vec4 sample = texture2D(texmap,texcoord); \n" // sample the texture at (u,v)
		"	gl_FragColor = colour*sample;             \n" // modulate the colours, i.e. (r1*r2,g1*g2,b1*b2,a1*a1)
		"}\n";

	program = gl_createprogram(vscode,fscode);
}

void init_vertex_buffer()
{
	// Instead of an array of vec4s, we now have an array of 'vertex'
	// structures where each contains the attributes for that vertex,
	// in this case (pos,colour,texcoord).
	vertex vertices[4] = {
		vertex(vec4(-1,-1,0,1),vec4(1,1,1,1),vec2(0,0)),
		vertex(vec4( 1,-1,0,1),vec4(1,0,0,1),vec2(4,0)),
		vertex(vec4(-1, 1,0,1),vec4(0,1,0,1),vec2(0,4)),
		vertex(vec4( 1, 1,0,1),vec4(0,0,1,1),vec2(4,4))   // four vertices of a quad
	};

	glGenBuffers(1,&mesh_vbo);
	glBindBuffer(GL_ARRAY_BUFFER,mesh_vbo);
	glBufferData(GL_ARRAY_BUFFER,sizeof(vertices),&vertices[0],GL_STATIC_DRAW);
}

void update(int)
{
	glutTimerFunc(20,&update,0);
	if (animate)
		angle += 0.01f;
	redraw();
}

void key_down(unsigned char key, int x, int y)
{
	if (key == ' ')
		animate = !animate; // toggle animation
}

void main(int argc, char** argv)
{
	glutInit(&argc,argv);
	glutCreateWindow("GLSL Texture Mapping Demo");
	glutDisplayFunc(&redraw);
	glutTimerFunc(20,&update,0);
	glutKeyboardFunc(&key_down);
	glutReshapeWindow(600,600);
	gl3wInit();                   // must initialize OpenGL 3.0 or else crashy crashy!

	init_program();
	init_vertex_buffer();
	texid = gl_loadtexture("checker.png");

	//texid = gl_loadtexture("checker_small.png");
	//glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
	//glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);

	glutMainLoop();
}


