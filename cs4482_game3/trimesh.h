#ifndef __TRIMESH_H__
#define __TRIMESH_H__

// trimesh.h 
//    Defines a simple vertex structure (with attributes), 
//    and provides a few functions for building triangle meshes 
//    geometry out of triangle lists.
//    These lists can then be used with GL_TRIANGLES ordering.

#include "vec4.h"
#include "vec2.h"
#include "cs3388lib.h"
#include <vector>

//
// vertex -- a vertex and its attributes
//
struct vertex {
	vec4 p;
	vec4 n;
	vec2 uv;

	vertex() { } // default constructor
	vertex(vec4 p, vec4 n, vec2 uv = vec2(0,0)): p(p), n(n), uv(uv) { } // convenience constructor
};

// interpolate the vertex attributes of a,b,c using barycentric coordinate (alpha,beta,1-alpha-beta).
// the result is a new set of 'vertex' attributes with interpolated position, normal, and uv coordinate.
vertex interp(float alpha, float beta, vertex a, vertex b, vertex c);

// 'triangles' is just an array of vertices, where size will be 3x the number of triangles.
typedef std::vector<vertex> triangles;

// create a single triangle with (a,b,c) in counter-clockwise order when viewed from front
triangles create_triangle(vertex a, vertex b, vertex c);

// create a single quad with (a,b,c,d) in counter-clockwise order when viewed from front
triangles create_quad(vertex a, vertex b, vertex c, vertex d);

triangles create_heightmap(bitmap* hm);

// create a 6-sided 2x2x2 box centered at (0,0,0);
triangles create_box();

// create a sphere of radius 1 where 'segs' controls the number of segments;
triangles create_sphere(int segs = 12, float radius = 1.0f);

// load a triangle list from an obj file; simplified in the following sense:
//   - all material information is ignored
//   - all faces in the mesh must be CONVEX, since the import code is 
//     not smart enough to decompose arbitrary faces into the right triangles
// .obj files can be exported from most 3D modelling packages that
// deal with polygon meshes. For example, in Maya the 'objExport' plugin comes
// with the software; if you activate the plugin, .obj is available for Export.
// 
triangles load_obj(const char* filename);

#endif // __TRIMESH_H__
