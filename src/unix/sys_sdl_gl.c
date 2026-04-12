/*
===========================================================================

Return to Castle Wolfenstein single player GPL Source Code
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company. 

This file is part of the Return to Castle Wolfenstein single player GPL Source Code ("RTCW SP Source Code").  

RTCW SP Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

RTCW SP Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with RTCW SP Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the RTCW SP Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the RTCW SP Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

// sys_sdl_gl.c - SDL2 OpenGL context management for RTCW SP

#include "q_shared.h"
#include "../qcommon/qcommon.h"
#include "tr_public.h"
#include "../renderer/qgl.h"

#include <SDL2/SDL.h>

extern SDL_Window *Sys_GetWindowHandle( void );
extern void *Sys_GetGLContext( void );

// OpenGL function pointers
void ( APIENTRY * qglActiveTextureARB )( GLenum texture ) = NULL;
void ( APIENTRY * qglClientActiveTextureARB )( GLenum texture ) = NULL;
void ( APIENTRY * qglMultiTexCoord2f )( GLenum target, GLfloat s, GLfloat t ) = NULL;

// Core OpenGL functions (loaded via SDL)
void ( APIENTRY * qglClearColor )( GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha ) = NULL;
void ( APIENTRY * qglClear )( GLbitfield mask ) = NULL;
void ( APIENTRY * qglColorMask )( GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha ) = NULL;
void ( APIENTRY * qglAlphaFunc )( GLenum func, GLclampf ref ) = NULL;
void ( APIENTRY * qglBlendFunc )( GLenum sfactor, GLenum dfactor ) = NULL;
void ( APIENTRY * qglCullFace )( GLenum mode ) = NULL;
void ( APIENTRY * qglFrontFace )( GLenum mode ) = NULL;
void ( APIENTRY * qglPolygonOffset )( GLfloat factor, GLfloat units ) = NULL;
void ( APIENTRY * qglScissor )( GLint x, GLint y, GLsizei width, GLsizei height ) = NULL;
void ( APIENTRY * qglEnable )( GLenum cap ) = NULL;
void ( APIENTRY * qglDisable )( GLenum cap ) = NULL;
GLboolean ( APIENTRY * qglIsEnabled )( GLenum cap ) = NULL;
void ( APIENTRY * qglGetBooleanv )( GLenum pname, GLboolean *params ) = NULL;
void ( APIENTRY * qglGetIntegerv )( GLenum pname, GLint *params ) = NULL;
void ( APIENTRY * qglGetFloatv )( GLenum pname, GLfloat *params ) = NULL;
GLenum ( APIENTRY * qglGetError )( void ) = NULL;
const GLubyte * ( APIENTRY * qglGetString )( GLenum name ) = NULL;
void ( APIENTRY * qglFinish )( void ) = NULL;
void ( APIENTRY * qglFlush )( void ) = NULL;
void ( APIENTRY * qglHint )( GLenum target, GLenum mode ) = NULL;
void ( APIENTRY * qglReadPixels )( GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels ) = NULL;
void ( APIENTRY * qglViewport )( GLint x, GLint y, GLsizei width, GLsizei height ) = NULL;
void ( APIENTRY * qglDrawBuffer )( GLenum mode ) = NULL;
void ( APIENTRY * qglPixelStoref )( GLenum pname, GLfloat param ) = NULL;
void ( APIENTRY * qglPixelStorei )( GLenum pname, GLint param ) = NULL;

// Matrix operations
void ( APIENTRY * qglMatrixMode )( GLenum mode ) = NULL;
void ( APIENTRY * qglOrtho )( GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble nearVal, GLdouble farVal ) = NULL;
void ( APIENTRY * qglFrustum )( GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble nearVal, GLdouble farVal ) = NULL;
void ( APIENTRY * qglPopMatrix )( void ) = NULL;
void ( APIENTRY * qglPushMatrix )( void ) = NULL;
void ( APIENTRY * qglLoadIdentity )( void ) = NULL;
void ( APIENTRY * qglLoadMatrixd )( const GLdouble *m ) = NULL;
void ( APIENTRY * qglLoadMatrixf )( const GLfloat *m ) = NULL;
void ( APIENTRY * qglMultMatrixd )( const GLdouble *m ) = NULL;
void ( APIENTRY * qglMultMatrixf )( const GLfloat *m ) = NULL;
void ( APIENTRY * qglRotated )( GLdouble angle, GLdouble x, GLdouble y, GLdouble z ) = NULL;
void ( APIENTRY * qglRotatef )( GLfloat angle, GLfloat x, GLfloat y, GLfloat z ) = NULL;
void ( APIENTRY * qglScaled )( GLdouble x, GLdouble y, GLdouble z ) = NULL;
void ( APIENTRY * qglScalef )( GLfloat x, GLfloat y, GLfloat z ) = NULL;
void ( APIENTRY * qglTranslated )( GLdouble x, GLdouble y, GLdouble z ) = NULL;
void ( APIENTRY * qglTranslatef )( GLfloat x, GLfloat y, GLfloat z ) = NULL;

// Vertex arrays
void ( APIENTRY * qglVertexPointer )( GLint size, GLenum type, GLsizei stride, const GLvoid *pointer ) = NULL;
void ( APIENTRY * qglNormalPointer )( GLenum type, GLsizei stride, const GLvoid *pointer ) = NULL;
void ( APIENTRY * qglColorPointer )( GLint size, GLenum type, GLsizei stride, const GLvoid *pointer ) = NULL;
void ( APIENTRY * qglTexCoordPointer )( GLint size, GLenum type, GLsizei stride, const GLvoid *pointer ) = NULL;
void ( APIENTRY * qglEnableClientState )( GLenum cap ) = NULL;
void ( APIENTRY * qglDisableClientState )( GLenum cap ) = NULL;
void ( APIENTRY * qglArrayElement )( GLint i ) = NULL;
void ( APIENTRY * qglDrawArrays )( GLenum mode, GLint first, GLsizei count ) = NULL;
void ( APIENTRY * qglDrawElements )( GLenum mode, GLsizei count, GLenum type, const GLvoid *indices ) = NULL;

// Lighting
void ( APIENTRY * qglLightModelf )( GLenum pname, GLfloat param ) = NULL;
void ( APIENTRY * qglLightModeli )( GLenum pname, GLint param ) = NULL;
void ( APIENTRY * qglLightModelfv )( GLenum pname, const GLfloat *params ) = NULL;
void ( APIENTRY * qglLightModeliv )( GLenum pname, const GLint *params ) = NULL;
void ( APIENTRY * qglLightf )( GLenum light, GLenum pname, GLfloat param ) = NULL;
void ( APIENTRY * qglLighti )( GLenum light, GLenum pname, GLint param ) = NULL;
void ( APIENTRY * qglLightfv )( GLenum light, GLenum pname, const GLfloat *params ) = NULL;
void ( APIENTRY * qglLightiv )( GLenum light, GLenum pname, const GLint *params ) = NULL;
void ( APIENTRY * qglLightModelf )( GLenum pname, GLfloat param ) = NULL;

// Materials
void ( APIENTRY * qglMaterialf )( GLenum face, GLenum pname, GLfloat param ) = NULL;
void ( APIENTRY * qglMateriali )( GLenum face, GLenum pname, GLint param ) = NULL;
void ( APIENTRY * qglMaterialfv )( GLenum face, GLenum pname, const GLfloat *params ) = NULL;
void ( APIENTRY * qglMaterialiv )( GLenum face, GLenum pname, const GLint *params ) = NULL;
void ( APIENTRY * qglColorMaterial )( GLenum face, GLenum mode ) = NULL;
void ( APIENTRY * qglShadeModel )( GLenum mode ) = NULL;
void ( APIENTRY * qglColor4f )( GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha ) = NULL;
void ( APIENTRY * qglColor4ub )( GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha ) = NULL;
void ( APIENTRY * qglColor3f )( GLfloat red, GLfloat green, GLfloat blue ) = NULL;

// Texture mapping
void ( APIENTRY * qglTexParameterf )( GLenum target, GLenum pname, GLfloat param ) = NULL;
void ( APIENTRY * qglTexParameteri )( GLenum target, GLenum pname, GLint param ) = NULL;
void ( APIENTRY * qglTexParameterfv )( GLenum target, GLenum pname, const GLfloat *params ) = NULL;
void ( APIENTRY * qglTexParameteriv )( GLenum target, GLenum pname, const GLint *params ) = NULL;
void ( APIENTRY * qglTexImage1D )( GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid *pixels ) = NULL;
void ( APIENTRY * qglTexImage2D )( GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels ) = NULL;
void ( APIENTRY * qglTexSubImage1D )( GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const GLvoid *pixels ) = NULL;
void ( APIENTRY * qglTexSubImage2D )( GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels ) = NULL;
void ( APIENTRY * qglCopyTexImage1D )( GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border ) = NULL;
void ( APIENTRY * qglCopyTexImage2D )( GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border ) = NULL;
void ( APIENTRY * qglCopyTexSubImage1D )( GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width ) = NULL;
void ( APIENTRY * qglCopyTexSubImage2D )( GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height ) = NULL;
void ( APIENTRY * qglTexEnvf )( GLenum target, GLenum pname, GLfloat param ) = NULL;
void ( APIENTRY * qglTexEnvi )( GLenum target, GLenum pname, GLint param ) = NULL;
void ( APIENTRY * qglTexEnvfv )( GLenum target, GLenum pname, const GLfloat *params ) = NULL;
void ( APIENTRY * qglTexEnviv )( GLenum target, GLenum pname, const GLint *params ) = NULL;
void ( APIENTRY * qglGenTextures )( GLsizei n, GLuint *textures ) = NULL;
void ( APIENTRY * qglDeleteTextures )( GLsizei n, const GLuint *textures ) = NULL;
void ( APIENTRY * qglBindTexture )( GLenum target, GLuint texture ) = NULL;
void ( APIENTRY * qglTexCoord1f )( GLfloat s ) = NULL;
void ( APIENTRY * qglTexCoord2f )( GLfloat s, GLfloat t ) = NULL;
void ( APIENTRY * qglTexCoord3f )( GLfloat s, GLfloat t, GLfloat r ) = NULL;
void ( APIENTRY * qglTexCoord4f )( GLfloat s, GLfloat t, GLfloat r, GLfloat q ) = NULL;
void ( APIENTRY * qglTexCoord2fv )( const GLfloat *v ) = NULL;

// Geometry
void ( APIENTRY * qglVertex2f )( GLfloat x, GLfloat y ) = NULL;
void ( APIENTRY * qglVertex3f )( GLfloat x, GLfloat y, GLfloat z ) = NULL;
void ( APIENTRY * qglVertex3fv )( const GLfloat *v ) = NULL;
void ( APIENTRY * qglNormal3f )( GLfloat nx, GLfloat ny, GLfloat nz ) = NULL;
void ( APIENTRY * qglNormal3fv )( const GLfloat *v ) = NULL;
void ( APIENTRY * qglBegin )( GLenum mode ) = NULL;
void ( APIENTRY * qglEnd )( void ) = NULL;

// Fog
void ( APIENTRY * qglFogf )( GLenum pname, GLfloat param ) = NULL;
void ( APIENTRY * qglFogi )( GLenum pname, GLint param ) = NULL;
void ( APIENTRY * qglFogfv )( GLenum pname, const GLfloat *params ) = NULL;
void ( APIENTRY * qglFogiv )( GLenum pname, const GLint *params ) = NULL;

// Display lists
void ( APIENTRY * qglNewList )( GLuint list, GLenum mode ) = NULL;
void ( APIENTRY * qglEndList )( void ) = NULL;
void ( APIENTRY * qglCallList )( GLuint list ) = NULL;
void ( APIENTRY * qglCallLists )( GLsizei n, GLenum type, const GLvoid *lists ) = NULL;
void ( APIENTRY * qglDeleteLists )( GLuint list, GLsizei range ) = NULL;
GLuint ( APIENTRY * qglGenLists )( GLsizei range ) = NULL;
void ( APIENTRY * qglListBase )( GLuint base ) = NULL;

// Feedback and selection
void ( APIENTRY * qglFeedbackBuffer )( GLsizei size, GLenum type, GLfloat *buffer ) = NULL;
void ( APIENTRY * qglSelectBuffer )( GLsizei size, GLuint *buffer ) = NULL;
GLint ( APIENTRY * qglRenderMode )( GLenum mode ) = NULL;
void ( APIENTRY * qglInitNames )( void ) = NULL;
void ( APIENTRY * qglLoadName )( GLuint name ) = NULL;
void ( APIENTRY * qglPassThrough )( GLfloat token ) = NULL;
void ( APIENTRY * qglPopName )( void ) = NULL;
void ( APIENTRY * qglPushName )( GLuint name ) = NULL;

// Accumulation buffer
void ( APIENTRY * qglAccum )( GLenum op, GLfloat value ) = NULL;
void ( APIENTRY * qglClearAccum )( GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha ) = NULL;

// Stencil buffer
void ( APIENTRY * qglStencilFunc )( GLenum func, GLint ref, GLuint mask ) = NULL;
void ( APIENTRY * qglStencilMask )( GLuint mask ) = NULL;
void ( APIENTRY * qglStencilOp )( GLenum fail, GLenum zfail, GLenum zpass ) = NULL;
void ( APIENTRY * qglClearStencil )( GLint s ) = NULL;

// Depth buffer
void ( APIENTRY * qglDepthFunc )( GLenum func ) = NULL;
void ( APIENTRY * qglDepthMask )( GLboolean flag ) = NULL;
void ( APIENTRY * qglDepthRange )( GLdouble nearVal, GLdouble farVal ) = NULL;

// Rasterization
void ( APIENTRY * qglRasterPos2f )( GLfloat x, GLfloat y ) = NULL;
void ( APIENTRY * qglRasterPos3f )( GLfloat x, GLfloat y, GLfloat z ) = NULL;
void ( APIENTRY * qglBitmap )( GLsizei width, GLsizei height, GLfloat xorig, GLfloat yorig, GLfloat xmove, GLfloat ymove, const GLubyte *bitmap ) = NULL;

// Rectangle
void ( APIENTRY * qglRectf )( GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2 ) = NULL;
void ( APIENTRY * qglRecti )( GLint x1, GLint y1, GLint x2, GLint y2 ) = NULL;

// GetMap, GetPixelMap, GetPolygonStipple
void ( APIENTRY * qglGetMapdv )( GLenum target, GLenum query, GLdouble *v ) = NULL;
void ( APIENTRY * qglGetMapfv )( GLenum target, GLenum query, GLfloat *v ) = NULL;
void ( APIENTRY * qglGetMapiv )( GLenum target, GLenum query, GLint *v ) = NULL;
void ( APIENTRY * qglMap1d )( GLenum target, GLdouble u1, GLdouble u2, GLint stride, GLint order, const GLdouble *points ) = NULL;
void ( APIENTRY * qglMap1f )( GLenum target, GLfloat u1, GLfloat u2, GLint stride, GLint order, const GLfloat *points ) = NULL;
void ( APIENTRY * qglMap2d )( GLenum target, GLdouble u1, GLdouble u2, GLint ustride, GLint uorder, GLdouble v1, GLdouble v2, GLint vstride, GLint vorder, const GLdouble *points ) = NULL;
void ( APIENTRY * qglMap2f )( GLenum target, GLfloat u1, GLfloat u2, GLint ustride, GLint uorder, GLfloat v1, GLfloat v2, GLint vstride, GLint vorder, const GLfloat *points ) = NULL;
void ( APIENTRY * qglEvalCoord1d )( GLdouble u ) = NULL;
void ( APIENTRY * qglEvalCoord1f )( GLfloat u ) = NULL;
void ( APIENTRY * qglEvalCoord1dv )( const GLdouble *u ) = NULL;
void ( APIENTRY * qglEvalCoord1fv )( const GLfloat *u ) = NULL;
void ( APIENTRY * qglEvalCoord2d )( GLdouble u, GLdouble v ) = NULL;
void ( APIENTRY * qglEvalCoord2f )( GLfloat u, GLfloat v ) = NULL;
void ( APIENTRY * qglEvalCoord2dv )( const GLdouble *u ) = NULL;
void ( APIENTRY * qglEvalCoord2fv )( const GLfloat *u ) = NULL;
void ( APIENTRY * qglEvalMesh1 )( GLenum mode, GLint i1, GLint i2 ) = NULL;
void ( APIENTRY * qglEvalMesh2 )( GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2 ) = NULL;
void ( APIENTRY * qglEvalPoint1 )( GLint i ) = NULL;
void ( APIENTRY * qglEvalPoint2 )( GLint i, GLint j ) = NULL;
void ( APIENTRY * qglPixelMapfv )( GLenum map, GLsizei mapsize, const GLfloat *values ) = NULL;
void ( APIENTRY * qglPixelMapuiv )( GLenum map, GLsizei mapsize, const GLuint *values ) = NULL;
void ( APIENTRY * qglPixelMapusv )( GLenum map, GLsizei mapsize, const GLushort *values ) = NULL;
void ( APIENTRY * qglGetPixelMapfv )( GLenum map, GLfloat *values ) = NULL;
void ( APIENTRY * qglGetPixelMapuiv )( GLenum map, GLuint *values ) = NULL;
void ( APIENTRY * qglGetPixelMapusv )( GLenum map, GLushort *values ) = NULL;
void ( APIENTRY * qglGetPolygonStipple )( GLubyte *mask ) = NULL;
void ( APIENTRY * qglPolygonStipple )( const GLubyte *mask ) = NULL;

// Clip planes
void ( APIENTRY * qglClipPlane )( GLenum plane, const GLdouble *equation ) = NULL;
void ( APIENTRY * qglGetClipPlane )( GLenum plane, GLdouble *equation ) = NULL;

// Utility
void ( APIENTRY * qglIndexMask )( GLuint mask ) = NULL;
void ( APIENTRY * qglColorMask )( GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha ) = NULL;
void ( APIENTRY * qglIndexd )( GLdouble c ) = NULL;
void ( APIENTRY * qglIndexf )( GLfloat c ) = NULL;
void ( APIENTRY * qglIndexi )( GLint c ) = NULL;
void ( APIENTRY * qglIndexs )( GLshort c ) = NULL;
void ( APIENTRY * qglIndexub )( GLubyte c ) = NULL;
void ( APIENTRY * qglIndexdv )( const GLdouble *c ) = NULL;
void ( APIENTRY * qglIndexfv )( const GLfloat *c ) = NULL;
void ( APIENTRY * qglIndexiv )( const GLint *c ) = NULL;
void ( APIENTRY * qglIndexsv )( const GLshort *c ) = NULL;
void ( APIENTRY * qglIndexubv )( const GLubyte *c ) = NULL;
void ( APIENTRY * qglIndexPointer )( GLenum type, GLsizei stride, const GLvoid *pointer ) = NULL;
void ( APIENTRY * qglEdgeFlag )( GLboolean flag ) = NULL;
void ( APIENTRY * qglEdgeFlagv )( const GLboolean *flag ) = NULL;
void ( APIENTRY * qglEdgeFlagPointer )( GLsizei stride, const GLvoid *pointer ) = NULL;
void ( APIENTRY * qglGetPointerv )( GLenum pname, GLvoid **params ) = NULL;

/*
=================
GLimp_Init
=================
*/
qboolean GLimp_Init( void ) {
	Com_Printf( "Initializing SDL OpenGL\n" );
	
	// Set OpenGL attributes
	SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 8 );
	SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 8 );
	SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 8 );
	SDL_GL_SetAttribute( SDL_GL_ALPHA_SIZE, 8 );
	SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, 24 );
	SDL_GL_SetAttribute( SDL_GL_STENCIL_SIZE, 8 );
	SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
	
	// Try to set multisampling
	if ( r_multisample->integer > 0 ) {
		SDL_GL_SetAttribute( SDL_GL_MULTISAMPLEBUFFERS, 1 );
		SDL_GL_SetAttribute( SDL_GL_MULTISAMPLESAMPLES, r_multisample->integer );
	}
	
	return qtrue;
}

/*
=================
GLimp_Shutdown
=================
*/
void GLimp_Shutdown( void ) {
	Com_Printf( "Shutting down SDL OpenGL\n" );
}

/*
=================
GLimp_LoadOpenGL
=================
*/
qboolean GLimp_LoadOpenGL( const char *name ) {
	// OpenGL functions are loaded automatically by SDL
	// Just verify we have a valid context
	
	void *glcontext = Sys_GetGLContext();
	if ( !glcontext ) {
		Com_Printf( S_COLOR_RED "ERROR: No OpenGL context available\n" );
		return qfalse;
	}
	
	// Load core functions using SDL_GL_GetProcAddress
	#define LOAD_GL_FUNC(name) \
		qgl##name = (void *)SDL_GL_GetProcAddress(#name); \
		if ( !qgl##name ) { \
			Com_DPrintf( S_COLOR_YELLOW "WARNING: Could not load %s\n", #name ); \
		}
	
	// Load essential functions
	LOAD_GL_FUNC(ClearColor);
	LOAD_GL_FUNC(Clear);
	LOAD_GL_FUNC(ColorMask);
	LOAD_GL_FUNC(AlphaFunc);
	LOAD_GL_FUNC(BlendFunc);
	LOAD_GL_FUNC(CullFace);
	LOAD_GL_FUNC(FrontFace);
	LOAD_GL_FUNC(Enable);
	LOAD_GL_FUNC(Disable);
	LOAD_GL_FUNC(IsEnabled);
	LOAD_GL_FUNC(GetError);
	LOAD_GL_FUNC(GetString);
	LOAD_GL_FUNC(Viewport);
	LOAD_GL_FUNC(MatrixMode);
	LOAD_GL_FUNC(LoadIdentity);
	LOAD_GL_FUNC(LoadMatrixf);
	LOAD_GL_FUNC(MultMatrixf);
	LOAD_GL_FUNC(Rotatef);
	LOAD_GL_FUNC(Translatef);
	LOAD_GL_FUNC(Scalef);
	LOAD_GL_FUNC(Begin);
	LOAD_GL_FUNC(End);
	LOAD_GL_FUNC(Vertex3f);
	LOAD_GL_FUNC(TexCoord2f);
	LOAD_GL_FUNC(Color4f);
	LOAD_GL_FUNC(Normal3f);
	LOAD_GL_FUNC(TexParameterf);
	LOAD_GL_FUNC(TexParameteri);
	LOAD_GL_FUNC(TexImage2D);
	LOAD_GL_FUNC(TexSubImage2D);
	LOAD_GL_FUNC(BindTexture);
	LOAD_GL_FUNC(GenTextures);
	LOAD_GL_FUNC(DeleteTextures);
	LOAD_GL_FUNC(PixelStorei);
	LOAD_GL_FUNC(ReadPixels);
	LOAD_GL_FUNC(Finish);
	LOAD_GL_FUNC(Flush);
	
	// Try to load ARB extensions
	qglActiveTextureARB = (void (APIENTRY *)(GLenum))SDL_GL_GetProcAddress("glActiveTextureARB");
	if ( !qglActiveTextureARB ) {
		qglActiveTextureARB = (void (APIENTRY *)(GLenum))SDL_GL_GetProcAddress("glActiveTexture");
	}
	
	#undef LOAD_GL_FUNC
	
	const char *vendor = (const char *)qglGetString( GL_VENDOR );
	const char *renderer = (const char *)qglGetString( GL_RENDERER );
	const char *version = (const char *)qglGetString( GL_VERSION );
	
	Com_Printf( "GL_VENDOR: %s\n", vendor );
	Com_Printf( "GL_RENDERER: %s\n", renderer );
	Com_Printf( "GL_VERSION: %s\n", version );
	
	return qtrue;
}

/*
=================
GLimp_UnloadOpenGL
=================
*/
void GLimp_UnloadOpenGL( void ) {
	// Nothing to do - SDL handles unloading
}

/*
=================
GLimp_SwapBuffers
=================
*/
void GLimp_SwapBuffers( void ) {
	extern void Sys_SwapBuffers( void );
	Sys_SwapBuffers();
}

/*
=================
GLimp_SetGammaRamp
=================
*/
void GLimp_SetGammaRamp( const unsigned short *ramp ) {
	SDL_Window *window = Sys_GetWindowHandle();
	if ( window ) {
		SDL_SetWindowGammaRamp( window, ramp, ramp + 256, ramp + 512 );
	}
}

/*
=================
GLimp_GetGammaRamp
=================
*/
void GLimp_GetGammaRamp( unsigned short *ramp ) {
	SDL_Window *window = Sys_GetWindowHandle();
	if ( window ) {
		SDL_GetWindowGammaRamp( window, ramp, ramp + 256, ramp + 512 );
	}
}
