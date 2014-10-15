#include "gl3w.h"     // for OpenGL 3.0 and compatible 2.1
#include "glut.h"
#include "trimesh.h"
#include "vec4.h"
#include "mat4x4.h"
#include "cs3388lib.h"
#include <vector>

#include <stdlib.h>
#include "fmod.h"
#include "fmod_errors.h"
#include <iostream>

#ifdef WIN32
	#include <windows.h>
	// automatically link to fmod library
	#pragma comment(lib,"fmod.lib")
#else
	#include <wincompat.h>
#endif

using namespace std;

#define OFFSET(type,member) (void*)(offsetof(type,member))  // what is the byte offset of type::member?

#define PI					3.14159265359f
#define MAX_HEIGHT			10
#define INVERSE256			0.00390625
#define PLAYER_HEIGHT		1
#define LOD_DISTANCE		24
#define VIEW_DISTANCE		48
#define POSTAGE_RANGE		2.0
#define TREE_RADIUS			1.7
#define TREE_TIGHTRADIUS	0.75
#define NUM_PAGES			8
#define NUM_TRIMESHES		5
#define TREE_OFFSET			-1

struct object {
	vec4		pos;  // position
	vec4		rot;  // rotation
	vec4		sca;  // scaling
	vec4		clr;  // diffuse colour
	triangles*	tri;  // triangles
	float		collisionRadius;
	bool		postable;

	// default constructor to initialize an object; see defaults below...
	object()
		: pos(0,0,0,1)  // default position (0,0,0)
		, rot(0,0,0,1)  // default rotation (0,0,0)
		, sca(1,1,1,0)  // default scale (1,1,1)
		, clr(0,0,0,1)  // default colour (white)
		, tri(0)        // default geometry (none)
		, collisionRadius(-1)
		, postable(false) 
	{
	}
};

GLuint mesh_vbo[NUM_TRIMESHES];  // id for our list of 3D vertices
GLuint program = 0;   // id for our GLSL program

// build some triangle lists ONE TIME ONLY; objects can point 
// to these to share the geometry inside, using different materials
bitmap* hm				= bitmap_load("valley_heightmap.png");
bitmap* tm				= bitmap_load("valley_treemap.png");

triangles tri_hm		= create_heightmap(hm);
triangles tri_tree		= load_obj("tree6_1.obj");
triangles tri_treeLOD	= load_obj("tree6_2.obj");
triangles tri_box		= create_box();
triangles tri_sphere	= create_sphere(6);

object* player			= 0;						// one object is the 'player' from which the eye is drawn
object* obj_hm			= 0;
object* obj_page[NUM_PAGES];
vector<object*> objects;							// list of objects to draw

float time				= 0;
float madness			= 0;
int currPage = 0;
vector<bool> keystate(256);
bool warped;
vec4 boxdim(32,32,32,0);

FSOUND_STREAM* g_mp3_stream = NULL;


// Generic helpers
float clamp(float x, float a, float  b){
	return x < a ? a : (x > b ? b : x);
}

float interpolate(float a, float b, float coeff){
	return (1-coeff)*a + (coeff)*b;
}

vec4 interpolate(vec4 a, vec4 b, float coeff){
	return (1-coeff)*a + (coeff)*b;
}

float flatDistance(vec4 a, vec4 b){
	return sqrt( (a.x-b.x)*(a.x-b.x) + (a.z-b.z)*(a.z-b.z) );
}

// Bitmap lookups
float height(int x, int z) {
	int p = 4 * ((z+32)*hm->wd+(x+32));
	float h = hm->pixels[p] * INVERSE256;
	return MAX_HEIGHT * h;
}

bool tree(int x, int z){
	int p = 4 * ((z+32)*hm->wd+(x+32));
	return tm->pixels[p];
}

float interpolatedHeight(float x, float z){
	int xRoundDown	= (int)(x);			// x position, to int, rounded down
	int zRoundDown	= (int)(z);			// z position, to int, rounded down
	int xRoundUp	= (int)(x+0.5);		// x position, to int, rounded up 
	int zRoundUp	= (int)(z+0.5);		// z position, to int, rounded up

	float ypos[4] = {	obj_hm->sca.y * height(xRoundDown, zRoundDown),
						obj_hm->sca.y * height(xRoundDown, zRoundUp),
						obj_hm->sca.y * height(xRoundUp, zRoundUp),
						obj_hm->sca.y * height(xRoundUp, zRoundDown)	};

	float xCoeff = player->pos.x - xRoundDown;
	float zCoeff = player->pos.z - zRoundDown;

	float x1 = interpolate(ypos[0], ypos[3], xCoeff);
	float x2 = interpolate(ypos[1], ypos[2], xCoeff);

	float z1 = interpolate(ypos[0], ypos[1], zCoeff);
	float z2 = interpolate(ypos[3], ypos[2], zCoeff);

	return interpolate(interpolate(x1, x2, zCoeff), interpolate(z1, z2, xCoeff), 0.5);
}


// Build the Shader
void init_program()
{
	// Specify a GLSL program that will be applied to each vertex independently
	// (a "vertex program" or "vertex shader")
	
	const char* vscode =						// vertex shader code
		"#version 120					\n"     // code is in GLSL version 1.20 (OpenGL 2.1)		
		"uniform mat4 P;				\n"     // projection matrix
		"uniform mat4 M;				\n"     // modelview matrix

		"attribute vec4 c;				\n" 
		"attribute vec4 p;				\n"     // the (x,y,z,1) point that we should process

		"varying vec4 p_col;			\n"		// Point Colour

		"void main()					\n"
		"{								\n"
		"	gl_Position = P*M*p;		\n"		// just transform model-coordinates to clip-coordinates
		"	p_col = c;					\n"												
												// Everything is done at the fragment level
		"}								\n";
	
	// Specify a GLSL program that will be applied to each pixel ('fragment') 
	// independently (a "fragment program" or "fragment shader")
	const char* fscode =						// fragment shader code
		"#version 120					\n"		// code is in GLSL version 1.20 (OpenGL 2.1)
		"uniform float time;"
		"uniform vec2 resolution;"
		"uniform float madness;"

		"varying vec4 p_col;			\n"		// Point Colour

		"void main()					\n"
		"{								\n"

		"	vec2 q = gl_FragCoord.xy / resolution.xy;"
		"	vec2 uv = 0.5 + (q-0.5)*(0.95 + 0.05*sin(0.2*time));"

			// Fog Colour
			"	vec4 fogColour	= vec4(0.7,0.7,0.7,1);"
			// Increase contrast on the depth
			"	float d0		= pow(gl_FragCoord.z, 64); \n"
			"	vec4 oricol		= clamp(	d0*fogColour	+gl_FragCoord.z*0.5*(1-madness/4)*fogColour		+p_col,			0.0,1.0);\n"
		    "	vec4 col = oricol;"
			// Contrast
			"	col = clamp(col*0.5 + 0.5*col*col*1.2 ,0.0,1.0);													\n"
			// radial vignette
			"	col = col * (0.5 + 0.5*16.0*uv.x*uv.y*(1.0-uv.x)*(1.0-uv.y));										\n"
			// Green Shift
			"	col = col * (vec3(0.9, 1.0, 0.8));																	\n"
		    // Scan lines
			"	col = col * (0.95+0.05 * sin(10.0*time+uv.y*1000.0));												\n"
			// Slight flicker
			"	col = col * ( 0.97 + 0.03 * sin(110.0*time) );														\n"
			 // static strength
			"	float noiseCoeff = 0.1*madness + 0.03 * sin(23.42357*time);											\n"
			// pseudo-random number generator
			"	vec4 noiseCol = fract(sin(dot(uv.xy , vec2(12.9898,78.233) ) )* 43758.5453) * vec4(1,1,1,1);		\n"
			// Put it all together
			"	gl_FragColor = (1-noiseCoeff) * col + (noiseCoeff) * noiseCol;										\n"	
		"}																											\n";

	// Compile each piece of code and link them into a shader program
	// i.e. "vertex shader" + "fragment shader" = GLSL program
	program = gl_createprogram(vscode,fscode);
}

// Send meshes down the drinking straw
void init_vertex_buffer()
{
	// Just like for textures, we must ask OpenGL for a unique ID
	// that will identify some vert
	glGenBuffers(NUM_TRIMESHES,&mesh_vbo[0]);														/* number of buffers needed */

	glBindBuffer(GL_ARRAY_BUFFER,	mesh_vbo[0]);
	glBufferData(GL_ARRAY_BUFFER,	tri_box.size()*sizeof(vertex),		&tri_box[0],		GL_STATIC_DRAW);
	
	glBindBuffer(GL_ARRAY_BUFFER,	mesh_vbo[1]);
	glBufferData(GL_ARRAY_BUFFER,	tri_sphere.size()*sizeof(vertex),	&tri_sphere[0],		GL_STATIC_DRAW);
	
	glBindBuffer(GL_ARRAY_BUFFER,	mesh_vbo[2]);
	glBufferData(GL_ARRAY_BUFFER,	tri_hm.size()*sizeof(vertex),		&tri_hm[0],			GL_STATIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER,	mesh_vbo[3]);
	glBufferData(GL_ARRAY_BUFFER,	tri_tree.size()*sizeof(vertex),		&tri_tree[0],		GL_STATIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER,	mesh_vbo[4]);
	glBufferData(GL_ARRAY_BUFFER,	tri_treeLOD.size()*sizeof(vertex),	&tri_treeLOD[0],	GL_STATIC_DRAW);
}

// Initialize objects
void init_objects()
{
	// create height map
	obj_hm = new object;
	obj_hm->pos = vec4(0,0,0,1);
	obj_hm->tri = &tri_hm;
	obj_hm->sca.y = 0.5;
	objects.push_back(obj_hm);

	for(int x = -32; x < 32; x++){
		for(int z = -32; z < 32; z++){
			if(tree(x,z)){
				object* obj_tree = new object;
				obj_tree->pos = vec4(x, height(x,z)*obj_hm->sca.y + TREE_OFFSET, z, 1);
				obj_tree->tri = &tri_tree;
				obj_tree->rot.y = rand();
				obj_tree->sca = vec4(0.5,0.5,0.5,1);
				obj_tree->collisionRadius = TREE_RADIUS;
				obj_tree->postable = true;
				objects.push_back(obj_tree);
			}
		}
	}

	for(int i = 0; i < NUM_PAGES; i++){
		obj_page[i] = new object;
		obj_page[i]->pos = vec4(0,64,0,1);
		obj_page[i]->clr = vec4(1,1,1,1);
		obj_page[i]->tri = &tri_box;
		obj_page[i]->sca = vec4(0.3, 0.4,0, 1);
		objects.push_back(obj_page[i]);
	}
	
	// positions for turning big coloured boxes into the walls of our Cornell box
	vec4 wall_pos[5] = {
		vec4(-2*boxdim.x,boxdim.y,0,1),  // shift a box so its right face becomes our left  wall
		vec4( 2*boxdim.x -1,boxdim.y,0,1),  // shift a box so its left  face becomes our right wall
		vec4( 0,boxdim.y, 2*boxdim.z -1,1), // shift a box so its far   face becomes our near  wall
		vec4( 0,boxdim.y,-2*boxdim.z,1), // shift a box so its near  face becomes our far   wall
		vec4( 0,2*boxdim.y,0,1)         // shift a box so its top   face becomes our floor
	};


	// create the five walls of different colour (which are actually boxes!)
	for (int i = 0; i < 5; ++i) {
		object* wall = new object;
		wall->pos = wall_pos[i];
		wall->sca = boxdim;
		wall->tri = &tri_box;
		objects.push_back(wall);
	}
	
	// insert player into world as an object with some position/rotation; we remember the 
	// object's pointer so that we can manipulate its position and draw from its viewpoint
	player = new object;
	player->pos = vec4(0, height(0,0) ,0,1);
	objects.push_back(player);
}

// Given an object, return the transformation matrix for it's position, rotation, and scale
mat4x4 xform(const object* obj)
{
	mat4x4 S  = scaling(obj->sca.x,obj->sca.y,obj->sca.z);
	mat4x4 Rx = rotation_x(obj->rot.x);
	mat4x4 Ry = rotation_y(obj->rot.y);
	mat4x4 Rz = rotation_z(obj->rot.z);
	mat4x4 T  = translation(obj->pos);
	return T*Rx*Ry*Rz*S;  // scale first, then rotate z,y,x, then translate
}

void draw_scene(const mat4x4& P, const object* camera)
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);  // clear color, and reset the z-buffer

	// prepare to configure modelview matrix
	mat4x4 Meye_inv = inverse(xform(camera));

	for (size_t i = 0; i < objects.size(); ++i) {
		object* obj = objects[i];
		
		// Nothing there, skip
		if (!(obj->tri) )
			continue;

		int size = (obj->tri)->size();
		mat4x4 M = Meye_inv*xform(obj);

		if (obj->tri == &tri_box) {
			glBindBuffer(GL_ARRAY_BUFFER,mesh_vbo[0]);
		}
		else if (obj->tri == &tri_sphere) {
			glBindBuffer(GL_ARRAY_BUFFER,mesh_vbo[1]);
		}
		else if (obj->tri == &tri_hm) {
			glBindBuffer(GL_ARRAY_BUFFER,mesh_vbo[2]);
		}
		else if (obj->tri == &tri_tree) {
			if(flatDistance(obj->pos, player->pos) > VIEW_DISTANCE){
				continue;
			}
			else if(flatDistance(obj->pos, player->pos) > LOD_DISTANCE){
				glBindBuffer(GL_ARRAY_BUFFER,mesh_vbo[4]);
				size = tri_treeLOD.size();			// Using smaller model so need to be careful of out of bounds.
			}
			else{
				glBindBuffer(GL_ARRAY_BUFFER,mesh_vbo[3]);
			}
		}


		glUseProgram(program);
		glUniformMatrix4fv(glGetUniformLocation(program,"P"),1,GL_TRUE,P.ptr());
		glUniformMatrix4fv(glGetUniformLocation(program,"M"),1,GL_TRUE,M.ptr());

		glUniform1f(glGetUniformLocation(program,"time"), time);
		glUniform1f(glGetUniformLocation(program,"madness"), madness);
		glUniform2f(glGetUniformLocation(program,"resolution"),glutGet(GLUT_WINDOW_WIDTH), glutGet(GLUT_WINDOW_HEIGHT));

		// Send point array to vertex shader
		GLuint ploc = glGetAttribLocation(program,"p");
		glVertexAttribPointer(ploc,4,GL_FLOAT,GL_FALSE,sizeof(vertex),OFFSET(vertex,p));
		glEnableVertexAttribArray(ploc);

		// Send object colour to vertex shader
		GLuint cloc = glGetAttribLocation(program,"c");
		glVertexAttrib4f(cloc, obj->clr.x, obj->clr.y, obj->clr.z, obj->clr.w);

		glDrawArrays(GL_TRIANGLES,0, size );			// Rasterize

		// Optional
		glUseProgram(0);
		glBindBuffer(GL_ARRAY_BUFFER,0);
	}
}

void redraw()
{
	//increment the global timer (used in shader for generating random numbers, should not be treated as actual timer)
	time++;

	// compute aspect ratio of current GLUT window
	float window_wd = glutGet(GLUT_WINDOW_WIDTH);
	float window_ht = glutGet(GLUT_WINDOW_HEIGHT);
	float aspect = window_wd / window_ht;

	// draw scene from 'eye' object's current vantage point
	//set_viewport(0,0,window_wd,window_ht);
	glViewport(0,0,window_wd,window_ht);
	glScissor(0,0,window_wd,window_ht);

	mat4x4 P0 = perspective(-.1f,.1f,-.1f/aspect,.1f/aspect,-.1f,-100);
	draw_scene(P0,player);

	// since drawing may take a while, we draw to an off-screen buffer and then
	// copy it to the screen (swap buffers) only once drawing is finished.
	glutSwapBuffers();
	glFinish();
}

void key_down(unsigned char key, int x, int y)
{
	keystate[key] = true;
}

void key_up(unsigned char key, int x, int y)
{
	keystate[key] = false;
}

void mouseMovement(int x, int y) {
	int GW = glutGet(GLUT_WINDOW_WIDTH);
	int GH = glutGet(GLUT_WINDOW_HEIGHT);
	
	if(!warped)
    {
        float diffx = x - GW/2;
        float diffy = y - GH/2;


		//QUATERNIONS, y u no work?

		/*
		float phi[2] = { cos(player->rot.x/2), sin(player->rot.x/2) };
		float tht[2] = { cos(player->rot.y/2), sin(player->rot.y/2) };
		float psi[2] = { cos(player->rot.z/2), sin(player->rot.z/2) };

		// to quaternions
		vec4 q = vec4(	phi[0] * tht[0] * psi[0] + phi[1] * tht[1] * psi[1],
						phi[1] * tht[0] * psi[0] - phi[0] * tht[1] * psi[1],
						phi[0] * tht[1] * psi[0] + phi[1] * tht[0] * psi[1],
						phi[0] * tht[0] * psi[1] - phi[1] * tht[1] * psi[0] );

		//Quaternion rotations

		// Back to euler
		player->rot = vec4( atan2( (2*(q.x * q.y + q.z * q.w)), (1 - 2*(q.x * q.x + q.y * q.y)) ),
							asin(  (2*(q.x * q.z - q.w * q.y)) ),
							atan2( (2*(q.x * q.w + q.y * q.z)), (1 - 2*(q.z * q.z + q.w * q.w)) ),
							1
							);
							*/


		//Suffers from gymbal locking but still works better than current quaternion implementation.
		
		player->rot.y += 0.005f*diffx;
        
		// Allows for vertical movement relative to the x axis.
		// Closer to the x-axis, this condenses to 0.
		// Some rotation in the z-axis(?) should take over as this happens.
			player->rot.x -= 0.005f*diffy*(cos(player->rot.y) //- 0.005*diffx*sin(player->rot.x)
				);

			player->rot.z -= 0.005f*diffy*(sin(player->rot.y) //+ 0.005*diffx*cos(player->rot.z)
				);
			
			
			player->rot.x = clamp(player->rot.x, -PI/4, PI/4);
			player->rot.z = clamp(player->rot.z, -PI/4, PI/4);

		//Seems to keep the camera level.
			player->rot.z = sin(player->rot.y)*sin(player->rot.x);

		warped = true;
        glutWarpPointer(GW/2, GH/2);
    }
    else
        warped = false;
}

bool canPost(float dist, object* obj){
	return dist < POSTAGE_RANGE && obj->postable;
}

void endGame(){
	
	
}

void update(int)
{
	glutTimerFunc(20,&update,0);
	vec4 targetPos = player->pos;		// Move buffer in case player tries to move into a wall.

	// to move the eye forward along current viewing angle
	if (keystate['w']){
		targetPos += 0.2f*rotation_y(player->rot.y)*vec4(0,0,-1,0);
	}
	if (keystate['s']){
		targetPos -= 0.08*rotation_y(player->rot.y)*vec4(0,0,-1,0);
	}
	// Strafing
	if (keystate['a']){
		targetPos.x -= float(cos(player->rot.y)) * 0.2;
		targetPos.z -= float(sin(player->rot.y)) * 0.2;

	}
	if (keystate['d']){
		targetPos.x += float(cos(player->rot.y)) * 0.2;
		targetPos.z += float(sin(player->rot.y)) * 0.2;
	}

	for (size_t i = 0; i < objects.size(); ++i) {
		object* obj = objects[i];
		float dist = flatDistance(targetPos, obj->pos);
		vec4 correctedPos = vec4(obj->pos.x, targetPos.y, obj->pos.z, 1);
		if(dist < obj->collisionRadius){
			
			// Allows for 'rolling' around the trees.
			// interpolate with a relatively small constant allows for smooth sailing
			targetPos = interpolate(targetPos, targetPos - normalize(correctedPos - targetPos) * (obj->collisionRadius), 0.1);
		}
		if(keystate[' '] && canPost(dist, obj) ) {
			madness+= 4.0/NUM_PAGES;
			obj->postable = false;
			FSOUND_Stream_Stop( g_mp3_stream );
			FSOUND_Stream_Play(0,g_mp3_stream);


			if(currPage >= NUM_PAGES ){
				exit(0);
			}
			correctedPos.y = player->pos.y;
			vec4 relativeDir = normalize(player->pos - correctedPos);

			obj_page[currPage]->pos = obj->pos + relativeDir  * (TREE_TIGHTRADIUS);
			obj_page[currPage]->pos.y = player->pos.y;
			obj_page[currPage]->rot.y = player->rot.y;
			currPage++;
		}
	}

	targetPos.y = //interpolate(player->pos.y, 
					interpolatedHeight(targetPos.x, targetPos.z) + PLAYER_HEIGHT
				//	, 0.5)
					;

	if (keystate['p']){
		endGame();
	}
	// Go to your room, and STAY THERE
	if (30 - targetPos.z <= 0)	{targetPos.z = 29.999;}		// near
	if (targetPos.z + 31 <= 0)	{targetPos.z = -30.999;}	// far
	if (29 - (targetPos.x)<= 0)	{targetPos.x = 28.999;}		// right
	if (targetPos.x + 31 <= 0)	{targetPos.x = -30.999;}	// left

	player->pos = targetPos;
	redraw();
}

void main(int argc, char** argv)
{
	glutInit(&argc,argv);
	glutInitDisplayMode(GLUT_RGB | GLUT_DEPTH | GLUT_DOUBLE);
	glutCreateWindow("SENDER");
	glutDisplayFunc(&redraw);
	glutKeyboardFunc(&key_down);
	glutKeyboardUpFunc(&key_up);
	glutPassiveMotionFunc(mouseMovement);
	glutSetCursor(GLUT_CURSOR_NONE); 
	glutFullScreen();
	glutTimerFunc(20,&update,0);
	gl3wInit();

	// initialize ALL THE THINGS
	//init_lights();
	init_objects();
	init_program();
	init_vertex_buffer();

	if( FSOUND_Init(44000,64,0) == FALSE )
	{
		exit(0);
	}

	// attempt to open the mp3 file as a stream
	g_mp3_stream = FSOUND_Stream_Open( "boom.mp3" , FSOUND_2D , 0 , 0 );

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glEnable(GL_SCISSOR_TEST);
	glutMainLoop();
}