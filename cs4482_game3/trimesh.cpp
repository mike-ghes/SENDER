#define _CRT_SECURE_NO_WARNINGS
#include "trimesh.h"
#include "mat4x4.h"
#include "cs3388lib.h"


#define MAX_HEIGHT			10
#define INVERSE_MAX_HEIGHT	0.1
#define INVERSE256			0.00390625
#define PI					3.14159265359f

vertex interp(float alpha, float beta, vertex a, vertex b, vertex c)
{
	return vertex(alpha*a.p  + beta*b.p  + (1-alpha-beta)*c.p,
	              alpha*a.n  + beta*b.n  + (1-alpha-beta)*c.n,
	              alpha*a.uv + beta*b.uv + (1-alpha-beta)*c.uv);
}

void append(triangles& dst, const triangles& src)
{
	dst.insert(dst.end(),src.begin(),src.end());
}

triangles create_triangle(vertex a, vertex b, vertex c)
{
	vertex v[3] = { a, b, c };
	return triangles(v,v+3);
}

triangles create_quad(vertex a, vertex b, vertex c, vertex d)
{
	triangles quad;
	append(quad,create_triangle(a,b,c));  // first triangle
	append(quad,create_triangle(a,c,d));  // opposite triangle
	return quad;
}

triangles create_box()
{
	// the 8 corners of the box
	vec4 pos[8] = {
		vec4(-1,-1,-1,1),  // 0 (-,-,-)  or  (left,bottom,far)
		vec4( 1,-1,-1,1),  // 1 (+,-,-)  or  (right,bottom,far)
		vec4(-1, 1,-1,1),  // 2 (-,+,-)  or  (left,top,far)
		vec4( 1, 1,-1,1),  // 3 (+,+,-)  or  (right,top,far)
		vec4(-1,-1, 1,1),  // 4 (-,-,+)  or  (left,bottom,near)
		vec4( 1,-1, 1,1),  // 5 (+,-,+)  or  (right,bottom,near)
		vec4(-1, 1, 1,1),  // 6 (-,+,+)  or  (left,top,near)
		vec4( 1, 1, 1,1)   // 7 (+,+,+)  or  (right,top,near)
	};

	// normals we can assign to the 6 faces, i.e. -nx,nx,-ny,ny,-nz,nz
	vec4 nx(1,0,0,0);
	vec4 ny(0,1,0,0);
	vec4 nz(0,0,1,0);

	// define handy short-hand macro for indexing corner points
	#define QUAD(a,b,c,d,n) \
		create_quad(vertex(pos[a],n,vec2(0,0)),\
		            vertex(pos[b],n,vec2(1,0)),\
		            vertex(pos[c],n,vec2(1,1)),\
		            vertex(pos[d],n,vec2(0,1)))

	// append the triangles together
	triangles box;
	append(box,QUAD(0,4,6,2,-nx)); // left face
	append(box,QUAD(5,1,3,7, nx)); // right face
	append(box,QUAD(0,1,5,4,-ny)); // bottom face
	append(box,QUAD(6,7,3,2, ny)); // top face
	append(box,QUAD(4,5,7,6, nz)); // near face
	append(box,QUAD(1,0,2,3,-nz)); // far face
	return box;
}

triangles create_sphere(int segs, float radius)
{
	mat4x4 S = scaling(radius,radius,radius);
	triangles sphere;
	int hsegs = 2*segs;
	int vsegs = segs;
	for (int i = 0; i < vsegs; ++i) {
		float v0 = (float)i/vsegs;
		float v1 = (float)(i+1)/vsegs;
		float phi0 = PI*v0;
		float phi1 = PI*v1;

		for (int j = 0; j < hsegs; ++j) {
			float u0 = (float)j/hsegs;
			float u1 = (float)(j+1)/hsegs;
			float theta0 = 2*PI*u0;
			float theta1 = 2*PI*u1;
		
			// compute (x,y,z) coordinate for top and bottom points
			vec4 p[4] = {
				rotation_y(-theta0)*rotation_z(phi0)*vec4(0,1,0,1),
				rotation_y(-theta0)*rotation_z(phi1)*vec4(0,1,0,1),
				rotation_y(-theta1)*rotation_z(phi1)*vec4(0,1,0,1),
				rotation_y(-theta1)*rotation_z(phi0)*vec4(0,1,0,1)
			};
			
			// compute normals, which are basically just p since we use radius 1
			vec4 n[4];
			for (int k = 0; k < 4; ++k) {
				n[k]   = p[k];
				n[k].w = 0;
			}

			// record the four points of the quad
			triangles face = create_quad(vertex(S*p[0],n[0],vec2(u0,v0)),
			                             vertex(S*p[1],n[1],vec2(u0,v1)),
			                             vertex(S*p[2],n[2],vec2(u1,v1)),
			                             vertex(S*p[3],n[3],vec2(u1,v0)));
			append(sphere,face);
 		}
	}
	return sphere;
}


float height(bitmap* hm, int x, int z) {
	int p = 4 * ((z+32)*hm->wd + (x+32));
	float h = hm->pixels[p] * INVERSE256;
	return MAX_HEIGHT * h;
}

triangles create_heightmap(bitmap* hm){
	triangles heightmap;

	for (int z = -32; z < 31; z++) {

		for (int x = -32; x < 31; x++) {
			
			vec4 p[4] = {	vec4((float)x,		height(hm, x,z),		(float)z,		1),
							vec4((float)x,		height(hm, x,z+1),		(float)(z+1),	1),
							vec4((float)(x+1),	height(hm, x+1,z+1),	(float)(z+1),	1),
							vec4((float)(x+1),	height(hm, x+1,z),		(float)z,		1)
			};
			
			// compute normals incorrectly
			vec4 n[4];
			for (int k = 0; k < 4; ++k) {
				//vec3 v1 = vec3(

				//n[k]	= cross(
				n[k]	= vec4(0,1,0,1);
				//n[k]	= vec4(n[k].x, n[k].y, n[k].z, 1);
			}


			// uv coordinates for textures.
			vec2 uv[4] = {	vec2(0,0),
							vec2(0,1),
							vec2(1,1),
							vec2(1,0)
			};


			// record the four points of the quad
			triangles face = create_quad(vertex(p[0],n[0],uv[0]),
			                             vertex(p[1],n[1],uv[1]),
			                             vertex(p[2],n[2],uv[2]),
			                             vertex(p[3],n[3],uv[3])
										);
			append(heightmap,face);
 		}
	}
	return heightmap;
}
























////////////// CODE BELOW TAKEN FROM glm.h AND glm.c IN THE GLUT SOURCE CODE ///////////
////////////// modified by adelong to build 'triangles' lists //////////////////////////

/*    
 *  GLM library.  Wavefront .obj file format reader/writer/manipulator.
 *
 *  Written by Nate Robins, 1997.
 *  email: ndr@pobox.com
 *  www: http://www.pobox.com/~ndr
 */


/* includes */
#include "glut.h"
#include <cassert>

// adelong
// Override the GLM calls to gl so that they build our own kind of triangle list instead
//
triangles* obj_tri = 0;
vec4 obj_tri_trans = vec4(0,0,0,0);
vec4 obj_tri_normal;
vec2 obj_tri_uv;

#define glBegin(mode) 0
#define glEnd() 0
#define glNormal3fv(n) obj_tri_normal = vec4((n)[0],(n)[1],(n)[2],0)
#define glTexCoord2fv(uv) obj_tri_uv = vec2((uv)[0],(uv)[1])
#define glVertex3fv(v) obj_tri->push_back(vertex(vec4((v)[0],(v)[1],(v)[2],1)+obj_tri_trans,obj_tri_normal,obj_tri_uv))
#define glEnable(flag) 0
#define glDisable(flag) 0
#define glTranslatef(dx,dy,dz) obj_tri_trans += vec4(dx,dy,dz,0);
#define glMaterialfv(side,param,val) 0
#define glMaterialf(side,param,val) 0
#define glColor3fv(clr) 0
#define glPushMatrix() 0
#define glPopMatrix() obj_tri_trans = vec4(0,0,0,0);


#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


/* defines */
#define GLM_NONE     (0)		/* render with only vertices */
#define GLM_FLAT     (1 << 0)		/* render with facet normals */
#define GLM_SMOOTH   (1 << 1)		/* render with vertex normals */
#define GLM_TEXTURE  (1 << 2)		/* render with texture coords */
#define GLM_COLOR    (1 << 3)		/* render with colors */
#define GLM_MATERIAL (1 << 4)		/* render with materials */


/* structs */

/* GLMmaterial: Structure that defines a material in a model. 
 */
typedef struct _GLMmaterial
{
  char* name;				/* name of material */
  GLfloat diffuse[4];			/* diffuse component */
  GLfloat ambient[4];			/* ambient component */
  GLfloat specular[4];			/* specular component */
  GLfloat emmissive[4];			/* emmissive component */
  GLfloat shininess;			/* specular exponent */
} GLMmaterial;

/* GLMtriangle: Structure that defines a triangle in a model.
 */
typedef struct {
  GLuint vindices[3];			/* array of triangle vertex indices */
  GLuint nindices[3];			/* array of triangle normal indices */
  GLuint tindices[3];			/* array of triangle texcoord indices*/
  GLuint findex;			/* index of triangle facet normal */
} GLMtriangle;

/* GLMgroup: Structure that defines a group in a model.
 */
typedef struct _GLMgroup {
  char*             name;		/* name of this group */
  GLuint            numtriangles;	/* number of triangles in this group */
  GLuint*           triangles;		/* array of triangle indices */
  GLuint            material;           /* index to material for group */
  struct _GLMgroup* next;		/* pointer to next group in model */
} GLMgroup;

/* GLMmodel: Structure that defines a model.
 */
typedef struct {
  char*    pathname;			/* path to this model */
  char*    mtllibname;			/* name of the material library */

  GLuint   numvertices;			/* number of vertices in model */
  GLfloat* vertices;			/* array of vertices  */

  GLuint   numnormals;			/* number of normals in model */
  GLfloat* normals;			/* array of normals */

  GLuint   numtexcoords;		/* number of texcoords in model */
  GLfloat* texcoords;			/* array of texture coordinates */

  GLuint   numfacetnorms;		/* number of facetnorms in model */
  GLfloat* facetnorms;			/* array of facetnorms */

  GLuint       numtriangles;		/* number of triangles in model */
  GLMtriangle* triangles;		/* array of triangles */

  GLuint       nummaterials;		/* number of materials in model */
  GLMmaterial* materials;		/* array of materials */

  GLuint       numgroups;		/* number of groups in model */
  GLMgroup*    groups;			/* linked list of groups */

  GLfloat position[3];			/* position of the model */

} GLMmodel;


/* public functions */

/* glmDelete: Deletes a GLMmodel structure.
 *
 * model - initialized GLMmodel structure
 */
GLvoid
glmDelete(GLMmodel* model);

/* glmReadOBJ: Reads a model description from a Wavefront .OBJ file.
 * Returns a pointer to the created object which should be free'd with
 * glmDelete().
 *
 * filename - name of the file containing the Wavefront .OBJ format data.  
 */
GLMmodel* 
glmReadOBJ(const char* filename);

/* glmDraw: Renders the model to the current OpenGL context using the
 * mode specified.
 *
 * model    - initialized GLMmodel structure
 * mode     - a bitwise OR of values describing what is to be rendered.
 *            GLM_NONE    -  render with only vertices
 *            GLM_FLAT    -  render with facet normals
 *            GLM_SMOOTH  -  render with vertex normals
 *            GLM_TEXTURE -  render with texture coords
 *            GLM_FLAT and GLM_SMOOTH should not both be specified.
 */
GLvoid
glmDraw(GLMmodel* model, GLuint mode);



/* defines */
#define T(x) model->triangles[(x)]


/* enums */
enum { X, Y, Z, W };			/* elements of a vertex */


/* typedefs */

/* _GLMnode: general purpose node
 */
typedef struct _GLMnode {
  GLuint           index;
  GLboolean        averaged;
  struct _GLMnode* next;
} GLMnode;

/* strdup is actually not a standard ANSI C or POSIX routine
   so implement a private one.  OpenVMS does not have a strdup; Linux's
   standard libc doesn't declare strdup by default (unless BSD or SVID
   interfaces are requested). */
static char *
stralloc(const char *string)
{
  char *copy;

  copy = (char*)malloc(strlen(string) + 1);
  if (copy == NULL)
    return NULL;
  strcpy(copy, string);
  return copy;
}

/* private functions */

/* _glmMax: returns the maximum of two floats */
static GLfloat
_glmMax(GLfloat a, GLfloat b) 
{
  if (a > b)
    return a;
  return b;
}

/* _glmAbs: returns the absolute value of a float */
static GLfloat
_glmAbs(GLfloat f)
{
  if (f < 0)
    return -f;
  return f;
}

/* _glmDot: compute the dot product of two vectors
 *
 * u - array of 3 GLfloats (GLfloat u[3])
 * v - array of 3 GLfloats (GLfloat v[3])
 */
static GLfloat
_glmDot(GLfloat* u, GLfloat* v)
{
  assert(u);
  assert(v);

  /* compute the dot product */
  return u[X] * v[X] + u[Y] * v[Y] + u[Z] * v[Z];
}

/* _glmCross: compute the cross product of two vectors
 *
 * u - array of 3 GLfloats (GLfloat u[3])
 * v - array of 3 GLfloats (GLfloat v[3])
 * n - array of 3 GLfloats (GLfloat n[3]) to return the cross product in
 */
static GLvoid
_glmCross(GLfloat* u, GLfloat* v, GLfloat* n)
{
  assert(u);
  assert(v);
  assert(n);

  /* compute the cross product (u x v for right-handed [ccw]) */
  n[X] = u[Y] * v[Z] - u[Z] * v[Y];
  n[Y] = u[Z] * v[X] - u[X] * v[Z];
  n[Z] = u[X] * v[Y] - u[Y] * v[X];
}

/* _glmNormalize: normalize a vector
 *
 * n - array of 3 GLfloats (GLfloat n[3]) to be normalized
 */
static GLvoid
_glmNormalize(GLfloat* n)
{
  GLfloat l;

  assert(n);

  /* normalize */
  l = (GLfloat)sqrt(n[X] * n[X] + n[Y] * n[Y] + n[Z] * n[Z]);
  n[0] /= l;
  n[1] /= l;
  n[2] /= l;
}

/* _glmEqual: compares two vectors and returns GL_TRUE if they are
 * equal (within a certain threshold) or GL_FALSE if not. An epsilon
 * that works fairly well is 0.000001.
 *
 * u - array of 3 GLfloats (GLfloat u[3])
 * v - array of 3 GLfloats (GLfloat v[3]) 
 */
static GLboolean
_glmEqual(GLfloat* u, GLfloat* v, GLfloat epsilon)
{
  if (_glmAbs(u[0] - v[0]) < epsilon &&
      _glmAbs(u[1] - v[1]) < epsilon &&
      _glmAbs(u[2] - v[2]) < epsilon) 
  {
    return GL_TRUE;
  }
  return GL_FALSE;
}

/* _glmWeldVectors: eliminate (weld) vectors that are within an
 * epsilon of each other.
 *
 * vectors    - array of GLfloat[3]'s to be welded
 * numvectors - number of GLfloat[3]'s in vectors
 * epsilon    - maximum difference between vectors 
 *
 */
GLfloat*
_glmWeldVectors(GLfloat* vectors, GLuint* numvectors, GLfloat epsilon)
{
  GLfloat* copies;
  GLuint   copied;
  GLuint   i, j;

  copies = (GLfloat*)malloc(sizeof(GLfloat) * 3 * (*numvectors + 1));
  memcpy(copies, vectors, (sizeof(GLfloat) * 3 * (*numvectors + 1)));

  copied = 1;
  for (i = 1; i <= *numvectors; i++) {
    for (j = 1; j <= copied; j++) {
      if (_glmEqual(&vectors[3 * i], &copies[3 * j], epsilon)) {
	goto duplicate;
      }
    }

    /* must not be any duplicates -- add to the copies array */
    copies[3 * copied + 0] = vectors[3 * i + 0];
    copies[3 * copied + 1] = vectors[3 * i + 1];
    copies[3 * copied + 2] = vectors[3 * i + 2];
    j = copied;				/* pass this along for below */
    copied++;

  duplicate:
    /* set the first component of this vector to point at the correct
       index into the new copies array */
    vectors[3 * i + 0] = (GLfloat)j;
  }

  *numvectors = copied-1;
  return copies;
}

/* _glmFindGroup: Find a group in the model
 */
GLMgroup*
_glmFindGroup(GLMmodel* model, char* name)
{
  GLMgroup* group;

  assert(model);

  group = model->groups;
  while(group) {
    if (!strcmp(name, group->name))
      break;
    group = group->next;
  }

  return group;
}

/* _glmAddGroup: Add a group to the model
 */
GLMgroup*
_glmAddGroup(GLMmodel* model, char* name)
{
  GLMgroup* group;

  group = _glmFindGroup(model, name);
  if (!group) {
    group = (GLMgroup*)malloc(sizeof(GLMgroup));
    group->name = stralloc(name);
    group->material = 0;
    group->numtriangles = 0;
    group->triangles = NULL;
    group->next = model->groups;
    model->groups = group;
    model->numgroups++;
  }

  return group;
}

/* _glmFindGroup: Find a material in the model
 */
GLuint
_glmFindMaterial(GLMmodel* model, char* name)
{
  GLuint i;

  for (i = 0; i < model->nummaterials; i++) {
    if (!strcmp(model->materials[i].name, name))
      goto found;
  }

  /* didn't find the name, so set it as the default material */
  printf("_glmFindMaterial():  can't find material \"%s\".\n", name);
  i = 0;

found:
  return i;
}


/* _glmDirName: return the directory given a path
 *
 * path - filesystem path
 *
 * The return value should be free'd.
 */
static char*
_glmDirName(char* path)
{
  char* dir;
  char* s;

  dir = stralloc(path);

  s = strrchr(dir, '/');
  if (s)
    s[1] = '\0';
  else
    dir[0] = '\0';

  return dir;
}


/* _glmReadMTL: read a wavefront material library file
 *
 * model - properly initialized GLMmodel structure
 * name  - name of the material library
 */
static GLvoid
_glmReadMTL(GLMmodel* model, char* name)
{
  FILE* file;
  char* dir;
  char* filename;
  char  buf[128];
  GLuint nummaterials, i;

  dir = _glmDirName(model->pathname);
  filename = (char*)malloc(sizeof(char) * (strlen(dir) + strlen(name) + 1));
  strcpy(filename, dir);
  strcat(filename, name);
  free(dir);

  /* open the file */
  file = fopen(filename, "r");
  if (!file) {
    fprintf(stderr, "_glmReadMTL() failed: can't open material file \"%s\".\n",
	    filename);
    exit(1);
  }
  free(filename);

  /* count the number of materials in the file */
  nummaterials = 1;
  while(fscanf(file, "%s", buf) != EOF) {
    switch(buf[0]) {
    case '#':				/* comment */
      /* eat up rest of line */
      fgets(buf, sizeof(buf), file);
      break;
    case 'n':				/* newmtl */
      fgets(buf, sizeof(buf), file);
      nummaterials++;
      sscanf(buf, "%s %s", buf, buf);
      break;
    default:
      /* eat up rest of line */
      fgets(buf, sizeof(buf), file);
      break;
    }
  }

  rewind(file);

  /* allocate memory for the materials */
  model->materials = (GLMmaterial*)malloc(sizeof(GLMmaterial) * nummaterials);
  model->nummaterials = nummaterials;

  /* set the default material */
  for (i = 0; i < nummaterials; i++) {
    model->materials[i].name = NULL;
    model->materials[i].shininess = 0;
    model->materials[i].diffuse[0] = 0.8;
    model->materials[i].diffuse[1] = 0.8;
    model->materials[i].diffuse[2] = 0.8;
    model->materials[i].diffuse[3] = 1.0;
    model->materials[i].ambient[0] = 0.2;
    model->materials[i].ambient[1] = 0.2;
    model->materials[i].ambient[2] = 0.2;
    model->materials[i].ambient[3] = 1.0;
    model->materials[i].specular[0] = 0.0;
    model->materials[i].specular[1] = 0.0;
    model->materials[i].specular[2] = 0.0;
    model->materials[i].specular[3] = 1.0;
  }
  model->materials[0].name = stralloc("default");

  /* now, read in the data */
  nummaterials = 0;
  while(fscanf(file, "%s", buf) != EOF) {
    switch(buf[0]) {
    case '#':				/* comment */
      /* eat up rest of line */
      fgets(buf, sizeof(buf), file);
      break;
    case 'n':				/* newmtl */
      fgets(buf, sizeof(buf), file);
      sscanf(buf, "%s %s", buf, buf);
      nummaterials++;
      model->materials[nummaterials].name = stralloc(buf);
      break;
    case 'N':
      fscanf(file, "%f", &model->materials[nummaterials].shininess);
      /* wavefront shininess is from [0, 1000], so scale for OpenGL */
      model->materials[nummaterials].shininess /= 1000.0;
      model->materials[nummaterials].shininess *= 128.0;
      break;
    case 'K':
      switch(buf[1]) {
      case 'd':
	fscanf(file, "%f %f %f",
	       &model->materials[nummaterials].diffuse[0],
	       &model->materials[nummaterials].diffuse[1],
	       &model->materials[nummaterials].diffuse[2]);
	break;
      case 's':
	fscanf(file, "%f %f %f",
	       &model->materials[nummaterials].specular[0],
	       &model->materials[nummaterials].specular[1],
	       &model->materials[nummaterials].specular[2]);
	break;
      case 'a':
	fscanf(file, "%f %f %f",
	       &model->materials[nummaterials].ambient[0],
	       &model->materials[nummaterials].ambient[1],
	       &model->materials[nummaterials].ambient[2]);
	break;
      default:
	/* eat up rest of line */
	fgets(buf, sizeof(buf), file);
	break;
      }
      break;
    default:
      /* eat up rest of line */
      fgets(buf, sizeof(buf), file);
      break;
    }
  }
}

/* _glmFirstPass: first pass at a Wavefront OBJ file that gets all the
 * statistics of the model (such as #vertices, #normals, etc)
 *
 * model - properly initialized GLMmodel structure
 * file  - (fopen'd) file descriptor 
 */
static GLvoid
_glmFirstPass(GLMmodel* model, FILE* file) 
{
  GLuint    numvertices;		/* number of vertices in model */
  GLuint    numnormals;			/* number of normals in model */
  GLuint    numtexcoords;		/* number of texcoords in model */
  GLuint    numtriangles;		/* number of triangles in model */
  GLMgroup* group;			/* current group */
  unsigned  v, n, t;
  char      buf[128];

  /* make a default group */
  group = _glmAddGroup(model, "default");

  numvertices = numnormals = numtexcoords = numtriangles = 0;
  while(fscanf(file, "%s", buf) != EOF) {
    switch(buf[0]) {
    case '#':				/* comment */
      /* eat up rest of line */
      fgets(buf, sizeof(buf), file);
      break;
    case 'v':				/* v, vn, vt */
      switch(buf[1]) {
      case '\0':			/* vertex */
	/* eat up rest of line */
	fgets(buf, sizeof(buf), file);
	numvertices++;
	break;
      case 'n':				/* normal */
	/* eat up rest of line */
	fgets(buf, sizeof(buf), file);
	numnormals++;
	break;
      case 't':				/* texcoord */
	/* eat up rest of line */
	fgets(buf, sizeof(buf), file);
	numtexcoords++;
	break;
      default:
	printf("_glmFirstPass(): Unknown token \"%s\".\n", buf);
	exit(1);
	break;
      }
      break;
    case 'm':
      fgets(buf, sizeof(buf), file);
      sscanf(buf, "%s %s", buf, buf);
      model->mtllibname = stralloc(buf);
      _glmReadMTL(model, buf);
      break;
    case 'u':
      /* eat up rest of line */
      fgets(buf, sizeof(buf), file);
      break;
    case 'g':				/* group */
      /* eat up rest of line */
      fgets(buf, sizeof(buf), file);
      sscanf(buf, "%s", buf);
      group = _glmAddGroup(model, buf);
      break;
    case 'f':				/* face */
      v = n = t = 0;
      fscanf(file, "%s", buf);
      /* can be one of %d, %d//%d, %d/%d, %d/%d/%d %d//%d */
      if (strstr(buf, "//")) {
	/* v//n */
	sscanf(buf, "%d//%d", &v, &n);
	fscanf(file, "%d//%d", &v, &n);
	fscanf(file, "%d//%d", &v, &n);
	numtriangles++;
	group->numtriangles++;
	while(fscanf(file, "%d//%d", &v, &n) > 0) {
	  numtriangles++;
	  group->numtriangles++;
	}
      } else if (sscanf(buf, "%d/%d/%d", &v, &t, &n) == 3) {
	/* v/t/n */
	fscanf(file, "%d/%d/%d", &v, &t, &n);
	fscanf(file, "%d/%d/%d", &v, &t, &n);
	numtriangles++;
	group->numtriangles++;
	while(fscanf(file, "%d/%d/%d", &v, &t, &n) > 0) {
	  numtriangles++;
	  group->numtriangles++;
	}
      } else if (sscanf(buf, "%d/%d", &v, &t) == 2) {
	/* v/t */
	fscanf(file, "%d/%d", &v, &t);
	fscanf(file, "%d/%d", &v, &t);
	numtriangles++;
	group->numtriangles++;
	while(fscanf(file, "%d/%d", &v, &t) > 0) {
	  numtriangles++;
	  group->numtriangles++;
	}
      } else {
	/* v */
	fscanf(file, "%d", &v);
	fscanf(file, "%d", &v);
	numtriangles++;
	group->numtriangles++;
	while(fscanf(file, "%d", &v) > 0) {
	  numtriangles++;
	  group->numtriangles++;
	}
      }
      break;

    default:
      /* eat up rest of line */
      fgets(buf, sizeof(buf), file);
      break;
    }
  }

#if 0
  /* announce the model statistics */
  printf(" Vertices: %d\n", numvertices);
  printf(" Normals: %d\n", numnormals);
  printf(" Texcoords: %d\n", numtexcoords);
  printf(" Triangles: %d\n", numtriangles);
  printf(" Groups: %d\n", model->numgroups);
#endif

  /* set the stats in the model structure */
  model->numvertices  = numvertices;
  model->numnormals   = numnormals;
  model->numtexcoords = numtexcoords;
  model->numtriangles = numtriangles;

  /* allocate memory for the triangles in each group */
  group = model->groups;
  while(group) {
    group->triangles = (GLuint*)malloc(sizeof(GLuint) * group->numtriangles);
    group->numtriangles = 0;
    group = group->next;
  }
}

/* _glmSecondPass: second pass at a Wavefront OBJ file that gets all
 * the data.
 *
 * model - properly initialized GLMmodel structure
 * file  - (fopen'd) file descriptor 
 */
static GLvoid
_glmSecondPass(GLMmodel* model, FILE* file) 
{
  GLuint    numvertices;		/* number of vertices in model */
  GLuint    numnormals;			/* number of normals in model */
  GLuint    numtexcoords;		/* number of texcoords in model */
  GLuint    numtriangles;		/* number of triangles in model */
  GLfloat*  vertices;			/* array of vertices  */
  GLfloat*  normals;			/* array of normals */
  GLfloat*  texcoords;			/* array of texture coordinates */
  GLMgroup* group;			/* current group pointer */
  GLuint    material;			/* current material */
  GLuint    v, n, t;
  char      buf[128];

  /* set the pointer shortcuts */
  vertices     = model->vertices;
  normals      = model->normals;
  texcoords    = model->texcoords;
  group        = model->groups;

  /* on the second pass through the file, read all the data into the
     allocated arrays */
  numvertices = numnormals = numtexcoords = 1;
  numtriangles = 0;
  material = 0;
  while(fscanf(file, "%s", buf) != EOF) {
    switch(buf[0]) {
    case '#':				/* comment */
      /* eat up rest of line */
      fgets(buf, sizeof(buf), file);
      break;
    case 'v':				/* v, vn, vt */
      switch(buf[1]) {
      case '\0':			/* vertex */
	fscanf(file, "%f %f %f", 
	       &vertices[3 * numvertices + X], 
	       &vertices[3 * numvertices + Y], 
	       &vertices[3 * numvertices + Z]);
	numvertices++;
	break;
      case 'n':				/* normal */
	fscanf(file, "%f %f %f", 
	       &normals[3 * numnormals + X],
	       &normals[3 * numnormals + Y], 
	       &normals[3 * numnormals + Z]);
	numnormals++;
	break;
      case 't':				/* texcoord */
	fscanf(file, "%f %f", 
	       &texcoords[2 * numtexcoords + X],
	       &texcoords[2 * numtexcoords + Y]);
	numtexcoords++;
	break;
      }
      break;
    case 'u':
      fgets(buf, sizeof(buf), file);
      sscanf(buf, "%s %s", buf, buf);
      group->material = material = _glmFindMaterial(model, buf);
      break;
    case 'g':				/* group */
      /* eat up rest of line */
      fgets(buf, sizeof(buf), file);
      sscanf(buf, "%s", buf);
      group = _glmFindGroup(model, buf);
      group->material = material;
      break;
    case 'f':				/* face */
      v = n = t = 0;
      fscanf(file, "%s", buf);
      /* can be one of %d, %d//%d, %d/%d, %d/%d/%d %d//%d */
      if (strstr(buf, "//")) {
	/* v//n */
	sscanf(buf, "%d//%d", &v, &n);
	T(numtriangles).vindices[0] = v;
	T(numtriangles).nindices[0] = n;
	fscanf(file, "%d//%d", &v, &n);
	T(numtriangles).vindices[1] = v;
	T(numtriangles).nindices[1] = n;
	fscanf(file, "%d//%d", &v, &n);
	T(numtriangles).vindices[2] = v;
	T(numtriangles).nindices[2] = n;
	group->triangles[group->numtriangles++] = numtriangles;
	numtriangles++;
	while(fscanf(file, "%d//%d", &v, &n) > 0) {
	  T(numtriangles).vindices[0] = T(numtriangles-1).vindices[0];
	  T(numtriangles).nindices[0] = T(numtriangles-1).nindices[0];
	  T(numtriangles).vindices[1] = T(numtriangles-1).vindices[2];
	  T(numtriangles).nindices[1] = T(numtriangles-1).nindices[2];
	  T(numtriangles).vindices[2] = v;
	  T(numtriangles).nindices[2] = n;
	  group->triangles[group->numtriangles++] = numtriangles;
	  numtriangles++;
	}
      } else if (sscanf(buf, "%d/%d/%d", &v, &t, &n) == 3) {
	/* v/t/n */
	T(numtriangles).vindices[0] = v;
	T(numtriangles).tindices[0] = t;
	T(numtriangles).nindices[0] = n;
	fscanf(file, "%d/%d/%d", &v, &t, &n);
	T(numtriangles).vindices[1] = v;
	T(numtriangles).tindices[1] = t;
	T(numtriangles).nindices[1] = n;
	fscanf(file, "%d/%d/%d", &v, &t, &n);
	T(numtriangles).vindices[2] = v;
	T(numtriangles).tindices[2] = t;
	T(numtriangles).nindices[2] = n;
	group->triangles[group->numtriangles++] = numtriangles;
	numtriangles++;
	while(fscanf(file, "%d/%d/%d", &v, &t, &n) > 0) {
	  T(numtriangles).vindices[0] = T(numtriangles-1).vindices[0];
	  T(numtriangles).tindices[0] = T(numtriangles-1).tindices[0];
	  T(numtriangles).nindices[0] = T(numtriangles-1).nindices[0];
	  T(numtriangles).vindices[1] = T(numtriangles-1).vindices[2];
	  T(numtriangles).tindices[1] = T(numtriangles-1).tindices[2];
	  T(numtriangles).nindices[1] = T(numtriangles-1).nindices[2];
	  T(numtriangles).vindices[2] = v;
	  T(numtriangles).tindices[2] = t;
	  T(numtriangles).nindices[2] = n;
	  group->triangles[group->numtriangles++] = numtriangles;
	  numtriangles++;
	}
      } else if (sscanf(buf, "%d/%d", &v, &t) == 2) {
	/* v/t */
	T(numtriangles).vindices[0] = v;
	T(numtriangles).tindices[0] = t;
	fscanf(file, "%d/%d", &v, &t);
	T(numtriangles).vindices[1] = v;
	T(numtriangles).tindices[1] = t;
	fscanf(file, "%d/%d", &v, &t);
	T(numtriangles).vindices[2] = v;
	T(numtriangles).tindices[2] = t;
	group->triangles[group->numtriangles++] = numtriangles;
	numtriangles++;
	while(fscanf(file, "%d/%d", &v, &t) > 0) {
	  T(numtriangles).vindices[0] = T(numtriangles-1).vindices[0];
	  T(numtriangles).tindices[0] = T(numtriangles-1).tindices[0];
	  T(numtriangles).vindices[1] = T(numtriangles-1).vindices[2];
	  T(numtriangles).tindices[1] = T(numtriangles-1).tindices[2];
	  T(numtriangles).vindices[2] = v;
	  T(numtriangles).tindices[2] = t;
	  group->triangles[group->numtriangles++] = numtriangles;
	  numtriangles++;
	}
      } else {
	/* v */
	sscanf(buf, "%d", &v);
	T(numtriangles).vindices[0] = v;
	fscanf(file, "%d", &v);
	T(numtriangles).vindices[1] = v;
	fscanf(file, "%d", &v);
	T(numtriangles).vindices[2] = v;
	group->triangles[group->numtriangles++] = numtriangles;
	numtriangles++;
	while(fscanf(file, "%d", &v) > 0) {
	  T(numtriangles).vindices[0] = T(numtriangles-1).vindices[0];
	  T(numtriangles).vindices[1] = T(numtriangles-1).vindices[2];
	  T(numtriangles).vindices[2] = v;
	  group->triangles[group->numtriangles++] = numtriangles;
	  numtriangles++;
	}
      }
      break;

    default:
      /* eat up rest of line */
      fgets(buf, sizeof(buf), file);
      break;
    }
  }

#if 0
  /* announce the memory requirements */
  printf(" Memory: %d bytes\n",
	 numvertices  * 3*sizeof(GLfloat) +
	 numnormals   * 3*sizeof(GLfloat) * (numnormals ? 1 : 0) +
	 numtexcoords * 3*sizeof(GLfloat) * (numtexcoords ? 1 : 0) +
	 numtriangles * sizeof(GLMtriangle));
#endif
}

/* glmDelete: Deletes a GLMmodel structure.
 *
 * model - initialized GLMmodel structure
 */
GLvoid
glmDelete(GLMmodel* model)
{
  GLMgroup* group;
  GLuint i;

  assert(model);

  if (model->pathname)   free(model->pathname);
  if (model->mtllibname) free(model->mtllibname);
  if (model->vertices)   free(model->vertices);
  if (model->normals)    free(model->normals);
  if (model->texcoords)  free(model->texcoords);
  if (model->facetnorms) free(model->facetnorms);
  if (model->triangles)  free(model->triangles);
  if (model->materials) {
    for (i = 0; i < model->nummaterials; i++)
      free(model->materials[i].name);
  }
  free(model->materials);
  while(model->groups) {
    group = model->groups;
    model->groups = model->groups->next;
    free(group->name);
    free(group->triangles);
    free(group);
  }

  free(model);
}


/* glmReadOBJ: Reads a model description from a Wavefront .OBJ file.
 * Returns a pointer to the created object which should be free'd with
 * glmDelete().
 *
 * filename - name of the file containing the Wavefront .OBJ format data.  
 */
GLMmodel* 
glmReadOBJ(const char* filename)
{
  GLMmodel* model;
  FILE*     file;

  /* open the file */
  file = fopen(filename, "r");
  if (!file) {
    fprintf(stderr, "glmReadOBJ() failed: can't open data file \"%s\".\n",
	    filename);
    exit(1);
  }

#if 0
  /* announce the model name */
  printf("Model: %s\n", filename);
#endif

  /* allocate a new model */
  model = (GLMmodel*)malloc(sizeof(GLMmodel));
  model->pathname      = stralloc(filename);
  model->mtllibname    = NULL;
  model->numvertices   = 0;
  model->vertices      = NULL;
  model->numnormals    = 0;
  model->normals       = NULL;
  model->numtexcoords  = 0;
  model->texcoords     = NULL;
  model->numfacetnorms = 0;
  model->facetnorms    = NULL;
  model->numtriangles  = 0;
  model->triangles     = NULL;
  model->nummaterials  = 0;
  model->materials     = NULL;
  model->numgroups     = 0;
  model->groups        = NULL;
  model->position[0]   = 0.0;
  model->position[1]   = 0.0;
  model->position[2]   = 0.0;

  /* make a first pass through the file to get a count of the number
     of vertices, normals, texcoords & triangles */
  _glmFirstPass(model, file);

  /* allocate memory */
  model->vertices = (GLfloat*)malloc(sizeof(GLfloat) *
				     3 * (model->numvertices + 1));
  model->triangles = (GLMtriangle*)malloc(sizeof(GLMtriangle) *
					  model->numtriangles);
  if (model->numnormals) {
    model->normals = (GLfloat*)malloc(sizeof(GLfloat) *
				      3 * (model->numnormals + 1));
  }
  if (model->numtexcoords) {
    model->texcoords = (GLfloat*)malloc(sizeof(GLfloat) *
					2 * (model->numtexcoords + 1));
  }

  /* rewind to beginning of file and read in the data this pass */
  rewind(file);

  _glmSecondPass(model, file);

  /* close the file */
  fclose(file);

  return model;
}


/* glmDraw: Renders the model to the current OpenGL context using the
 * mode specified.
 *
 * model    - initialized GLMmodel structure
 * mode     - a bitwise OR of values describing what is to be rendered.
 *            GLM_NONE     -  render with only vertices
 *            GLM_FLAT     -  render with facet normals
 *            GLM_SMOOTH   -  render with vertex normals
 *            GLM_TEXTURE  -  render with texture coords
 *            GLM_COLOR    -  render with colors (color material)
 *            GLM_MATERIAL -  render with materials
 *            GLM_COLOR and GLM_MATERIAL should not both be specified.  
 *            GLM_FLAT and GLM_SMOOTH should not both be specified.  
 */
GLvoid
glmDraw(GLMmodel* model, GLuint mode)
{
  GLuint i;
  GLMgroup* group;

  assert(model);
  assert(model->vertices);

  /* do a bit of warning */
  if (mode & GLM_FLAT && !model->facetnorms) {
    printf("glmDraw() warning: flat render mode requested "
	   "with no facet normals defined.\n");
    mode &= ~GLM_FLAT;
  }
  if (mode & GLM_SMOOTH && !model->normals) {
    printf("glmDraw() warning: smooth render mode requested "
	   "with no normals defined.\n");
    mode &= ~GLM_SMOOTH;
  }
  if (mode & GLM_TEXTURE && !model->texcoords) {
    printf("glmDraw() warning: texture render mode requested "
	   "with no texture coordinates defined.\n");
    mode &= ~GLM_TEXTURE;
  }
  if (mode & GLM_FLAT && mode & GLM_SMOOTH) {
    printf("glmDraw() warning: flat render mode requested "
	   "and smooth render mode requested (using smooth).\n");
    mode &= ~GLM_FLAT;
  }
  if (mode & GLM_COLOR && !model->materials) {
    printf("glmDraw() warning: color render mode requested "
	   "with no materials defined.\n");
    mode &= ~GLM_COLOR;
  }
  if (mode & GLM_MATERIAL && !model->materials) {
    printf("glmDraw() warning: material render mode requested "
	   "with no materials defined.\n");
    mode &= ~GLM_MATERIAL;
  }
  if (mode & GLM_COLOR && mode & GLM_MATERIAL) {
    printf("glmDraw() warning: color and material render mode requested "
	   "using only material mode\n");
    mode &= ~GLM_COLOR;
  }
  if (mode & GLM_COLOR)
    glEnable(GL_COLOR_MATERIAL);
  if (mode & GLM_MATERIAL)
    glDisable(GL_COLOR_MATERIAL);

  glPushMatrix();
  glTranslatef(model->position[0], model->position[1], model->position[2]);

  glBegin(GL_TRIANGLES);
  group = model->groups;
  while (group) {
    if (mode & GLM_MATERIAL) {
      glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, 
		   model->materials[group->material].ambient);
      glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, 
		   model->materials[group->material].diffuse);
      glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, 
		   model->materials[group->material].specular);
       glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 
  		  model->materials[group->material].shininess);
    }

    if (mode & GLM_COLOR) {
      glColor3fv(model->materials[group->material].diffuse);
    }

    for (i = 0; i < group->numtriangles; i++) {
      if (mode & GLM_FLAT)
	glNormal3fv(&model->facetnorms[3 * T(group->triangles[i]).findex]);
      
      if (mode & GLM_SMOOTH)
	glNormal3fv(&model->normals[3 * T(group->triangles[i]).nindices[0]]);
      if (mode & GLM_TEXTURE)
	glTexCoord2fv(&model->texcoords[2*T(group->triangles[i]).tindices[0]]);
      glVertex3fv(&model->vertices[3 * T(group->triangles[i]).vindices[0]]);
#if 0
      printf("%f %f %f\n", 
	     model->vertices[3 * T(group->triangles[i]).vindices[0] + X],
	     model->vertices[3 * T(group->triangles[i]).vindices[0] + Y],
	     model->vertices[3 * T(group->triangles[i]).vindices[0] + Z]);
#endif
      
      if (mode & GLM_SMOOTH)
	glNormal3fv(&model->normals[3 * T(group->triangles[i]).nindices[1]]);
      if (mode & GLM_TEXTURE)
	glTexCoord2fv(&model->texcoords[2*T(group->triangles[i]).tindices[1]]);
      glVertex3fv(&model->vertices[3 * T(group->triangles[i]).vindices[1]]);
#if 0
      printf("%f %f %f\n", 
	     model->vertices[3 * T(group->triangles[i]).vindices[1] + X],
	     model->vertices[3 * T(group->triangles[i]).vindices[1] + Y],
	     model->vertices[3 * T(group->triangles[i]).vindices[1] + Z]);
#endif
      
      if (mode & GLM_SMOOTH)
	glNormal3fv(&model->normals[3 * T(group->triangles[i]).nindices[2]]);
      if (mode & GLM_TEXTURE)
	glTexCoord2fv(&model->texcoords[2*T(group->triangles[i]).tindices[2]]);
      glVertex3fv(&model->vertices[3 * T(group->triangles[i]).vindices[2]]);
#if 0
      printf("%f %f %f\n", 
	     model->vertices[3 * T(group->triangles[i]).vindices[2] + X],
	     model->vertices[3 * T(group->triangles[i]).vindices[2] + Y],
	     model->vertices[3 * T(group->triangles[i]).vindices[2] + Z]);
#endif
      
    }
    
    group = group->next;
  }
  glEnd();

  glPopMatrix();
}



















///////////////////////////






















// adelong
triangles load_obj(const char* filename)
{
	GLMmodel* model = glmReadOBJ(filename);
	triangles tri;
	obj_tri = &tri;
	glmDraw(model,GLM_SMOOTH | GLM_TEXTURE);
	glmDelete(model);
	obj_tri = 0;
	return tri;
}

