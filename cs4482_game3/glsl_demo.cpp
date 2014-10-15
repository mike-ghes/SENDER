#include "gl3w.h"     // OpenGL 3.0 (including many 2.1 features)
#include "glut.h"
#include "cs3388lib.h"
#include "vec4.h"
#include "mat4x4.h"
#include "trimesh.h"

GLuint mesh_vbo = 0;  // id for our list of 3D vertices
GLuint program = 0;   // id for our GLSL program
float angle = 0;

void redraw()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);  // clear color, and reset the z-buffer
	glEnable(GL_DEPTH_TEST);                             // enable z-buffer test

	// Create our own projection and modelview matrices
	mat4x4 P = perspective(-1,1,-1,1,-1,-10);
	mat4x4 M = translation(0,0,-5)*rotation_z(angle)*translation(0,1,0)*rotation_y(10*angle);

	// Bind our vertex buffer so that glDraw* will pull data from it 
	// and send it down the rasterization pipeline
	glBindBuffer(GL_ARRAY_BUFFER,mesh_vbo);

	// Bind our GLSL program so that glDraw* will invoke it
	glUseProgram(program);

	// Bind UNIFORM program variables (constants that are the same for all vertices).
	// Copy our local "P" and "M" matrices into the GLSL program's "P" and "M" variables.
	// We use "Uniform" because P and M are the same for every vertex.
	// (Each GLSL variable is indexed by the order it appears in the program,
	//  and glGetUniformLocation looks up the index for a particular variable name.)
	glUniformMatrix4fv(glGetUniformLocation(program,"P"),1,GL_TRUE,P.ptr());
	glUniformMatrix4fv(glGetUniformLocation(program,"M"),1,GL_TRUE,M.ptr());

	// Bind ATTRIBUTE program variables (per-vertex attributes like position, normal, texcoord).
	// Tell the glDraw* functions *precisely* how our vertex data is formatted for each attribute.
	// In this simple program, we only have attribute 'p' (position), so we tell OpenGL that
	// each is 4 GL_FLOATs, and that position[1] is +sizeof(vec4) bytes beyond position[0]
	int ploc = glGetAttribLocation(program,"p");
	glVertexAttribPointer(ploc,4,GL_FLOAT,GL_FALSE,sizeof(vec4),0);
	glEnableVertexAttribArray(ploc);  // don't forget to enable the position 'attribute'!!

	// RASTERIZE! Instead of glBegin/glEnd, use glDrawArrays to draw primitives.
	glDrawArrays(GL_TRIANGLES,0,3);	

	// OPTIONAL: unbind the GLSL program and the vertex buffer, if we were to draw something
	//           else afterwards (such as text, bitmaps, etc).
	glUseProgram(0);
	glBindBuffer(GL_ARRAY_BUFFER,0);

	glFlush();
}

void init_program()
{
	// Specify a GLSL program that will be applied to each vertex independently
	// (a "vertex program" or "vertex shader")

	const char* vscode =            // vertex shader code
		"#version 120      \n"      // code is in GLSL version 1.20 (OpenGL 2.1)
		"uniform mat4 P;   \n"      // projection matrix
		"uniform mat4 M;   \n"      // modelview matrix
		"attribute vec4 p; \n"      // the (x,y,z,1) point that we should process
		"void main()       \n"
		"{\n"
		"	gl_Position = P*M*p;   \n" // just transform model-coordinates to clip-coordinates!
		"}\n";

	// Specify a GLSL program that will be applied to each pixel ('fragment') 
	// independently (a "fragment program" or "fragment shader")

	const char* fscode =            // fragment shader code
		"#version 120         \n"   // code is in GLSL version 1.20 (OpenGL 2.1)
		"void main()          \n"
		"{\n"
		"	gl_FragColor = vec4(1,0,0,1);   \n"  // just output red, all the time
		"}\n";

	// Compile each piece of code and link them into a shader program
	// i.e. "vertex shader" + "fragment shader" = GLSL program
	program = gl_createprogram(vscode,fscode);
}

void init_vertex_buffer()
{
	// Points of a 3D triangle
	triangles tri_box = create_box();

	// Just like for textures, we must ask OpenGL for a unique ID
	// that will identify some vert
	glGenBuffers(1,&mesh_vbo);
	glBindBuffer(GL_ARRAY_BUFFER,mesh_vbo);

	// Copy our vertex data into OpenGL's internal vertex buffer (currently mesh_vbo)
	glBufferData(GL_ARRAY_BUFFER,sizeof(positions),&positions[0],GL_STATIC_DRAW);
}

void update(int)
{
	glutTimerFunc(20,&update,0);
	angle += 0.02f;     // update rotation
	redraw();
}

void main(int argc, char** argv)
{
	glutInit(&argc,argv);
	glutCreateWindow("GLSL Demo");
	glutDisplayFunc(&redraw);
	glutTimerFunc(20,&update,0);
	glutReshapeWindow(500,500);
	gl3wInit();                   // must initialize OpenGL 3.0 or else crashy crashy!

	// Before we start drawing, register our GLSL program and our vertex list with OpenGL
	init_program();
	init_vertex_buffer();

	glutMainLoop();
}


