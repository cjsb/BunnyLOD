/*
 *   Polygon Reduction Demo by Stan Melax (c) 1998
 *  Permission to use any of this code wherever you want is granted..
 *  Although, please do acknowledge authorship if appropriate.
 *
 *  This module initializes the bunny model data and calls
 *  the polygon reduction routine.  At each frame the RenderModel()
 *  routine is called to draw the model.  This module also
 *  animates the parameters (such as number of vertices to
 *  use) to show the model at various levels of detail.
 */

#define NOMINMAX
#include <windows.h>
#include <stdio.h>  
#include <math.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <GL/gl.h>
#pragma warning(disable : 4244)

#include "progmesh.h"
#include "rabdata.h"
#include "glutil.h"
#include "geometric.h"

int   render_num;   // number of vertices to draw with
float lodbase=0.5f; // the fraction of vertices used to morph toward
float morph=1.0f;   // where to render between 2 levels of detail
std::vector<float3> vert;       // global Array of vertices
std::vector<tridata> tri;       // global Array of triangles
std::vector<int> collapse_map;  // to which neighbor each vertex collapses
int renderpolycount=0;   // polygons rendered in the current frame
float3 model_position;         // position of bunny
float4 model_orientation(0,0,0,1);  // orientation of bunny

// Note that the use of the Map() function and the collapse_map
// Array isn't part of the polygon reduction algorithm.
// We just set up this system here in this module
// so that we could retrieve the model at any desired vertex count.
// Therefore if this part of the program confuses you, then
// dont worry about it.  It might help to look over the progmesh.cpp
// module first.

//       Map()
//
// When the model is rendered using a maximum of mx vertices
// then it is vertices 0 through mx-1 that are used.  
// We are able to do this because the vertex Array 
// gets sorted according to the collapse order.
// The Map() routine takes a vertex number 'a' and the
// maximum number of vertices 'mx' and returns the 
// appropriate vertex in the range 0 to mx-1.
// When 'a' is greater than 'mx' the Map() routine
// follows the chain of edge collapses until a vertex
// within the limit is reached.
//   An example to make this clear: assume there is
//   a triangle with vertices 1, 3 and 12.  But when
//   rendering the model we limit ourselves to 10 vertices.
//   In that case we find out how vertex 12 was removed
//   by the polygon reduction algorithm.  i.e. which
//   edge was collapsed.  Lets say that vertex 12 was collapsed
//   to vertex number 7.  This number would have been stored
//   in the collapse_map array (i.e. collapse_map[12]==7).
//   Since vertex 7 is in range (less than max of 10) we
//   will want to render the triangle 1,3,7.  
//   Pretend now that we want to limit ourselves to 5 vertices.
//   and vertex 7 was collapsed to vertex 3 
//   (i.e. collapse_map[7]==3).  Then triangle 1,3,12 would now be
//   triangle 1,3,3.  i.e. this polygon was removed by the
//   progressive mesh polygon reduction algorithm by the time
//   it had gotten down to 5 vertices.
//   No need to draw a one dimensional polygon. :-)
int Map(int a,int mx) {
	if(mx<=0) return 0;
	while(a>=mx) {  
		a=collapse_map[a];
	}
	return a;
}

void DrawModelTriangles() {
	assert(collapse_map.size());
	renderpolycount=0;
	for (unsigned int i = 0; i<tri.size(); i++) {
		int p0= Map(tri[i].v[0],render_num);
		int p1= Map(tri[i].v[1],render_num);
		int p2= Map(tri[i].v[2],render_num);
		// note:  serious optimization opportunity here,
		//  by sorting the triangles the following "continue" 
		//  could have been made into a "break" statement.
		if(p0==p1 || p1==p2 || p2==p0) continue;
		renderpolycount++;
		// if we are not currenly morphing between 2 levels of detail
		// (i.e. if morph=1.0) then q0,q1, and q2 are not necessary.
		int q0= Map(p0,(int)(render_num*lodbase));
		int q1= Map(p1,(int)(render_num*lodbase));
		int q2= Map(p2,(int)(render_num*lodbase));
		float3 v0, v1, v2;
		v0 = vert[p0]*morph + vert[q0]*(1-morph);
		v1 = vert[p1]*morph + vert[q1]*(1-morph);
		v2 = vert[p2]*morph + vert[q2]*(1-morph);
		glBegin(GL_POLYGON);
			// the purpose of the demo is to show polygons
			// therefore just use 1 face normal (flat shading)
			float3 nrml = cross(v1-v0,v2-v1);  
			if(0<length(nrml)) {
				nrml = normalize(nrml);
				glNormal3fv(nrml);
			}
			glVertex3fv(v0);
			glVertex3fv(v1);
			glVertex3fv(v2);
		glEnd();
	}
}


void PermuteVertices(std::vector<int> &permutation) {
	// rearrange the vertex Array 
	std::vector<float3> temp_Array;
	unsigned int i;
	assert(permutation.size() == vert.size());
	for (i = 0; i<vert.size(); i++) {
		temp_Array.push_back(vert[i]);
	}
	for(i=0;i<vert.size();i++) {
		vert[permutation[i]]=temp_Array[i];
	}
	// update the changes in the entries in the triangle Array
	for (i = 0; i<tri.size(); i++) {
		for(int j=0;j<3;j++) {
			tri[i].v[j] = permutation[tri[i].v[j]];
		}
	}
}

void GetRabbitData(){
	// Copy the geometry from the arrays of data in rabdata.cpp into
	// the vert and tri Arrays which we send to the reduction routine
	int i;
	for(i=0;i<RABBIT_VERTEX_NUM;i++) {
		float *vp=rabbit_vertices[i];
		vert.push_back(float3(vp[0],vp[1],vp[2]));
	}
	for(i=0;i<RABBIT_TRIANGLE_NUM;i++) {
		tridata td;
		td.v[0]=rabbit_triangles[i][0];
		td.v[1]=rabbit_triangles[i][1];
		td.v[2]=rabbit_triangles[i][2];
		tri.push_back(td);
	}
	render_num=(int)vert.size();  // by default lets use all the model to render
}


void InitModel() {
	std::vector<int> permutation;
	GetRabbitData();  

	ProgressiveMesh(vert,tri,collapse_map,permutation);

	PermuteVertices(permutation);
	model_position    = float3(0,0,-3);
	float4 yaw = QuatFromAxisAngle(float3(0, 1, 0), -3.14f / 4);    // 45 degrees
	float4 pitch = QuatFromAxisAngle(float3(1, 0, 0), 3.14f / 12);  // 15 degrees 
	model_orientation = qmul(pitch,yaw);
}

void StatusDraw() {
	//  Draw a slider type widget looking thing
	// to show portion of vertices being used
	float b = (float)render_num / (float)vert.size();
	float a = b*(lodbase );
	glDisable(GL_LIGHTING);
	glMatrixMode( GL_PROJECTION );
	glPushMatrix();
	glLoadIdentity();
	glOrtho(-0.15,15,-0.1,1.1,-0.1,100);
	glMatrixMode( GL_MODELVIEW );

	glPushMatrix();
	glLoadIdentity();
	glBegin(GL_POLYGON);
	glColor3f(1,0,0);
	glVertex2f(0,0);
	glVertex2f(1,0);
	glVertex2f(1,a);
	glVertex2f(0,a);
	glEnd();
	glBegin(GL_POLYGON);
	glColor3f(1,0,0);
	glVertex2f(0,a);
	glVertex2f(morph,a);
	glVertex2f(morph,b);
	glVertex2f(0,b);
	glEnd();
	glBegin(GL_POLYGON);
	glColor3f(0,0,1);
	glVertex2f(morph,a);
	glVertex2f(1,a);
	glVertex2f(1,b);
	glVertex2f(morph,b);
	glEnd();
	glBegin(GL_POLYGON);
	glColor3f(0,0,1);
	glVertex2f(0,b);
	glVertex2f(1,b);
	glVertex2f(1,1);
	glVertex2f(0,1);
	glEnd();
	glPopMatrix();
	glMatrixMode( GL_PROJECTION );
	glPopMatrix();
	glMatrixMode( GL_MODELVIEW );
}

/*
 *  The following is just a quick hack to animate
 *  the object through various polygon reduced versions.
 */
struct keyframethings {
	float t;   // timestamp
	float n;   // portion of vertices used to start
	float dn;  // rate of change in "n"
	float m;   // morph value
	float dm;  // rate of change in "m"
} keys[]={
	{0 ,1   ,0 ,1, 0},
	{2 ,1   ,-1,1, 0},
	{10,0   ,1 ,1, 0},
	{18,1   ,0 ,1, 0},
	{20,1   ,0 ,1,-1},
	{24,0.5 ,0 ,1, 0},
	{26,0.5 ,0 ,1,-1},
	{30,0.25,0 ,1, 0},
	{32,0.25,0 ,1,-1},
	{36,0.125,0,1, 0},
	{38,0.25,0 ,0, 1},
	{42,0.5 ,0 ,0, 1},
	{46,1   ,0 ,0, 1},
	{50,1   ,0 ,1, 0},
};
void AnimateParameters( float DeltaT  ) {
	static float time=0;  // global time - used for animation
	time+=DeltaT;
	if(time>=50) time=0;  // repeat cycle every so many seconds
	int k=0;
	while(time>keys[k+1].t) {
		k++;
	}
	float interp = (time-keys[k].t)/(keys[k+1].t-keys[k].t);
	render_num = vert.size()*(keys[k].n + interp*keys[k].dn);
	morph    = keys[k].m + interp*keys[k].dm;
	morph = (morph>1.0f) ? 1.0f : morph;  // clamp value
	if (render_num> (int) vert.size()) render_num = (int)vert.size();
	if(render_num<0       ) render_num=0;
}

char *RenderModel( float DeltaT ) {
	AnimateParameters(DeltaT);
	
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glColor3f(1,1,1);
	glPushMatrix();
	glMultMatrixf(Pose(model_position, model_orientation).matrix());
	DrawModelTriangles();
	StatusDraw();
	glPopMatrix();

	static char buf[256];
	sprintf_s(buf,"Polys: %d  Vertices: %d ",renderpolycount,render_num);
	if(morph<1.0) {
		sprintf_s(buf+strlen(buf),256-strlen(buf),"<-> %d  morph: %4.2f ",
		        (int)(lodbase *render_num),morph); 
	}
	return buf;
}

