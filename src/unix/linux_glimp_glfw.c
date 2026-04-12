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

/*
** GLIMP.C - OpenGL implementation с использованием GLFW
** 
** Адаптировано для современных Linux систем
** - Использует GLFW для создания окна и обработки ввода
** - Поддержка OpenGL 3.3+ Core Profile
** - Удалена зависимость от X11/GLX
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "../renderer/tr_local.h"
#include "../client/client.h"
#include "../client/keys.h"
#include "linux_local.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

// Объявление внешних указателей на функции OpenGL из qgl.h
extern void ( APIENTRY * qglBindTexture )( GLenum target, GLuint texture );
extern void ( APIENTRY * qglDeleteTextures )( GLsizei n, const GLuint *textures );
extern void ( APIENTRY * qglAccum )( GLenum op, GLfloat value );
extern void ( APIENTRY * qglAlphaFunc )( GLenum func, GLclampf ref );
extern void ( APIENTRY * qglArrayElement )( GLint i );
extern void ( APIENTRY * qglBegin )( GLenum mode );
extern void ( APIENTRY * qglBitmap )( GLsizei width, GLsizei height, GLfloat xorig, GLfloat yorig, GLfloat xmove, GLfloat ymove, const GLubyte *bitmap );
extern void ( APIENTRY * qglBlendFunc )( GLenum sfactor, GLenum dfactor );
extern void ( APIENTRY * qglCallList )( GLuint list );
extern void ( APIENTRY * qglCallLists )( GLsizei n, GLenum type, const GLvoid *lists );
extern void ( APIENTRY * qglClear )( GLbitfield mask );
extern void ( APIENTRY * qglClearAccum )( GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha );
extern void ( APIENTRY * qglClearColor )( GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha );
extern void ( APIENTRY * qglClearDepth )( GLclampd depth );
extern void ( APIENTRY * qglClearIndex )( GLfloat c );
extern void ( APIENTRY * qglClearStencil )( GLint s );
extern void ( APIENTRY * qglClipPlane )( GLenum plane, const GLdouble *equation );
extern void ( APIENTRY * qglColor3b )( GLbyte red, GLbyte green, GLbyte blue );
extern void ( APIENTRY * qglColor3bv )( const GLbyte *v );
extern void ( APIENTRY * qglColor3d )( GLdouble red, GLdouble green, GLdouble blue );
extern void ( APIENTRY * qglColor3dv )( const GLdouble *v );
extern void ( APIENTRY * qglColor3f )( GLfloat red, GLfloat green, GLfloat blue );
extern void ( APIENTRY * qglColor3fv )( const GLfloat *v );
extern void ( APIENTRY * qglColor3i )( GLint red, GLint green, GLint blue );
extern void ( APIENTRY * qglColor3iv )( const GLint *v );
extern void ( APIENTRY * qglColor3s )( GLshort red, GLshort green, GLshort blue );
extern void ( APIENTRY * qglColor3sv )( const GLshort *v );
typedef void (APIENTRY *PFNGLACTIVETEXTUREARBPROC)(GLenum texture);
typedef void (APIENTRY *PFNGLCLIENTACTIVETEXTUREARBPROC)(GLenum texture);
typedef void (APIENTRY *PFNGLLOCKARRAYSEXTPROC)(GLint first, GLsizei count);
typedef void (APIENTRY *PFNGLUNLOCKARRAYSEXTPROC)(void);
typedef void (APIENTRY *PFNGLPNTRIANGLESIATIPROC)(GLenum pname, GLint param);
typedef void (APIENTRY *PFNGLPNTRIANGLESFATIPROC)(GLenum pname, GLfloat param);

// Инициализация указателей на OpenGL функции
// Для Linux с GLFW просто перенаправляем qgl* на стандартные gl* функции
#define INIT_QGL_FUNC(x) qgl##x = gl##x;

static void InitQGLFunctions(void) {
    // Основные функции OpenGL 1.1
    INIT_QGL_FUNC(Accum);
    INIT_QGL_FUNC(AlphaFunc);
    INIT_QGL_FUNC(AreTexturesResident);
    INIT_QGL_FUNC(ArrayElement);
    INIT_QGL_FUNC(Begin);
    INIT_QGL_FUNC(BindTexture);
    INIT_QGL_FUNC(Bitmap);
    INIT_QGL_FUNC(BlendFunc);
    INIT_QGL_FUNC(CallList);
    INIT_QGL_FUNC(CallLists);
    INIT_QGL_FUNC(Clear);
    INIT_QGL_FUNC(ClearAccum);
    INIT_QGL_FUNC(ClearColor);
    INIT_QGL_FUNC(ClearDepth);
    INIT_QGL_FUNC(ClearIndex);
    INIT_QGL_FUNC(ClearStencil);
    INIT_QGL_FUNC(ClipPlane);
    INIT_QGL_FUNC(Color3b);
    INIT_QGL_FUNC(Color3bv);
    INIT_QGL_FUNC(Color3d);
    INIT_QGL_FUNC(Color3dv);
    INIT_QGL_FUNC(Color3f);
    INIT_QGL_FUNC(Color3fv);
    INIT_QGL_FUNC(Color3i);
    INIT_QGL_FUNC(Color3iv);
    INIT_QGL_FUNC(Color3s);
    INIT_QGL_FUNC(Color3sv);
    INIT_QGL_FUNC(Color3ub);
    INIT_QGL_FUNC(Color3ubv);
    INIT_QGL_FUNC(Color3ui);
    INIT_QGL_FUNC(Color3uiv);
    INIT_QGL_FUNC(Color3us);
    INIT_QGL_FUNC(Color3usv);
    INIT_QGL_FUNC(Color4b);
    INIT_QGL_FUNC(Color4bv);
    INIT_QGL_FUNC(Color4d);
    INIT_QGL_FUNC(Color4dv);
    INIT_QGL_FUNC(Color4f);
    INIT_QGL_FUNC(Color4fv);
    INIT_QGL_FUNC(Color4i);
    INIT_QGL_FUNC(Color4iv);
    INIT_QGL_FUNC(Color4s);
    INIT_QGL_FUNC(Color4sv);
    INIT_QGL_FUNC(Color4ub);
    INIT_QGL_FUNC(Color4ubv);
    INIT_QGL_FUNC(Color4ui);
    INIT_QGL_FUNC(Color4uiv);
    INIT_QGL_FUNC(Color4us);
    INIT_QGL_FUNC(Color4usv);
    INIT_QGL_FUNC(ColorMask);
    INIT_QGL_FUNC(ColorMaterial);
    INIT_QGL_FUNC(ColorPointer);
    INIT_QGL_FUNC(CopyPixels);
    INIT_QGL_FUNC(CopyTexImage1D);
    INIT_QGL_FUNC(CopyTexImage2D);
    INIT_QGL_FUNC(CopyTexSubImage1D);
    INIT_QGL_FUNC(CopyTexSubImage2D);
    INIT_QGL_FUNC(CullFace);
    INIT_QGL_FUNC(DeleteLists);
    INIT_QGL_FUNC(DeleteTextures);
    INIT_QGL_FUNC(DepthFunc);
    INIT_QGL_FUNC(DepthMask);
    INIT_QGL_FUNC(DepthRange);
    INIT_QGL_FUNC(Disable);
    INIT_QGL_FUNC(DisableClientState);
    INIT_QGL_FUNC(DrawArrays);
    INIT_QGL_FUNC(DrawBuffer);
    INIT_QGL_FUNC(DrawElements);
    INIT_QGL_FUNC(DrawPixels);
    INIT_QGL_FUNC(EdgeFlag);
    INIT_QGL_FUNC(EdgeFlagPointer);
    INIT_QGL_FUNC(EdgeFlagv);
    INIT_QGL_FUNC(Enable);
    INIT_QGL_FUNC(EnableClientState);
    INIT_QGL_FUNC(End);
    INIT_QGL_FUNC(EndList);
    INIT_QGL_FUNC(EvalCoord1d);
    INIT_QGL_FUNC(EvalCoord1dv);
    INIT_QGL_FUNC(EvalCoord1f);
    INIT_QGL_FUNC(EvalCoord1fv);
    INIT_QGL_FUNC(EvalCoord2d);
    INIT_QGL_FUNC(EvalCoord2dv);
    INIT_QGL_FUNC(EvalCoord2f);
    INIT_QGL_FUNC(EvalCoord2fv);
    INIT_QGL_FUNC(EvalMesh1);
    INIT_QGL_FUNC(EvalMesh2);
    INIT_QGL_FUNC(EvalPoint1);
    INIT_QGL_FUNC(EvalPoint2);
    INIT_QGL_FUNC(FeedbackBuffer);
    INIT_QGL_FUNC(Finish);
    INIT_QGL_FUNC(Flush);
    INIT_QGL_FUNC(Fogf);
    INIT_QGL_FUNC(Fogfv);
    INIT_QGL_FUNC(Fogi);
    INIT_QGL_FUNC(Fogiv);
    INIT_QGL_FUNC(FrontFace);
    INIT_QGL_FUNC(Frustum);
    INIT_QGL_FUNC(GenLists);
    INIT_QGL_FUNC(GenTextures);
    INIT_QGL_FUNC(GetBooleanv);
    INIT_QGL_FUNC(GetClipPlane);
    INIT_QGL_FUNC(GetDoublev);
    INIT_QGL_FUNC(GetError);
    INIT_QGL_FUNC(GetFloatv);
    INIT_QGL_FUNC(GetIntegerv);
    INIT_QGL_FUNC(GetLightfv);
    INIT_QGL_FUNC(GetLightiv);
    INIT_QGL_FUNC(GetMapdv);
    INIT_QGL_FUNC(GetMapfv);
    INIT_QGL_FUNC(GetMapiv);
    INIT_QGL_FUNC(GetMaterialfv);
    INIT_QGL_FUNC(GetMaterialiv);
    INIT_QGL_FUNC(GetPixelMapfv);
    INIT_QGL_FUNC(GetPixelMapuiv);
    INIT_QGL_FUNC(GetPixelMapusv);
    INIT_QGL_FUNC(GetPointerv);
    INIT_QGL_FUNC(GetPolygonStipple);
    INIT_QGL_FUNC(GetString);
    INIT_QGL_FUNC(GetTexEnvfv);
    INIT_QGL_FUNC(GetTexEnviv);
    INIT_QGL_FUNC(GetTexGendv);
    INIT_QGL_FUNC(GetTexGenfv);
    INIT_QGL_FUNC(GetTexGeniv);
    INIT_QGL_FUNC(GetTexImage);
    INIT_QGL_FUNC(GetTexLevelParameterfv);
    INIT_QGL_FUNC(GetTexLevelParameteriv);
    INIT_QGL_FUNC(GetTexParameterfv);
    INIT_QGL_FUNC(GetTexParameteriv);
    INIT_QGL_FUNC(Hint);
    INIT_QGL_FUNC(IndexMask);
    INIT_QGL_FUNC(IndexPointer);
    INIT_QGL_FUNC(Indexd);
    INIT_QGL_FUNC(Indexdv);
    INIT_QGL_FUNC(Indexf);
    INIT_QGL_FUNC(Indexfv);
    INIT_QGL_FUNC(Indexi);
    INIT_QGL_FUNC(Indexiv);
    INIT_QGL_FUNC(Indexs);
    INIT_QGL_FUNC(Indexsv);
    INIT_QGL_FUNC(Indexub);
    INIT_QGL_FUNC(Indexubv);
    INIT_QGL_FUNC(InitNames);
    INIT_QGL_FUNC(InterleavedArrays);
    INIT_QGL_FUNC(IsEnabled);
    INIT_QGL_FUNC(IsList);
    INIT_QGL_FUNC(IsTexture);
    INIT_QGL_FUNC(LightModelf);
    INIT_QGL_FUNC(LightModelfv);
    INIT_QGL_FUNC(LightModeli);
    INIT_QGL_FUNC(LightModeliv);
    INIT_QGL_FUNC(Lightf);
    INIT_QGL_FUNC(Lightfv);
    INIT_QGL_FUNC(Lighti);
    INIT_QGL_FUNC(Lightiv);
    INIT_QGL_FUNC(LineStipple);
    INIT_QGL_FUNC(LineWidth);
    INIT_QGL_FUNC(ListBase);
    INIT_QGL_FUNC(LoadIdentity);
    INIT_QGL_FUNC(LoadMatrixd);
    INIT_QGL_FUNC(LoadMatrixf);
    INIT_QGL_FUNC(LoadName);
    INIT_QGL_FUNC(LogicOp);
    INIT_QGL_FUNC(Map1d);
    INIT_QGL_FUNC(Map1f);
    INIT_QGL_FUNC(Map2d);
    INIT_QGL_FUNC(Map2f);
    INIT_QGL_FUNC(MapGrid1d);
    INIT_QGL_FUNC(MapGrid1f);
    INIT_QGL_FUNC(MapGrid2d);
    INIT_QGL_FUNC(MapGrid2f);
    INIT_QGL_FUNC(Materialf);
    INIT_QGL_FUNC(Materialfv);
    INIT_QGL_FUNC(Materiali);
    INIT_QGL_FUNC(Materialiv);
    INIT_QGL_FUNC(MatrixMode);
    INIT_QGL_FUNC(MultMatrixd);
    INIT_QGL_FUNC(MultMatrixf);
    INIT_QGL_FUNC(NewList);
    INIT_QGL_FUNC(Normal3b);
    INIT_QGL_FUNC(Normal3bv);
    INIT_QGL_FUNC(Normal3d);
    INIT_QGL_FUNC(Normal3dv);
    INIT_QGL_FUNC(Normal3f);
    INIT_QGL_FUNC(Normal3fv);
    INIT_QGL_FUNC(Normal3i);
    INIT_QGL_FUNC(Normal3iv);
    INIT_QGL_FUNC(Normal3s);
    INIT_QGL_FUNC(Normal3sv);
    INIT_QGL_FUNC(NormalPointer);
    INIT_QGL_FUNC(Ortho);
    INIT_QGL_FUNC(PassThrough);
    INIT_QGL_FUNC(PixelMapfv);
    INIT_QGL_FUNC(PixelMapuiv);
    INIT_QGL_FUNC(PixelMapusv);
    INIT_QGL_FUNC(PixelStoref);
    INIT_QGL_FUNC(PixelStorei);
    INIT_QGL_FUNC(PixelTransferf);
    INIT_QGL_FUNC(PixelTransferi);
    INIT_QGL_FUNC(PixelZoom);
    INIT_QGL_FUNC(PointSize);
    INIT_QGL_FUNC(PolygonMode);
    INIT_QGL_FUNC(PolygonOffset);
    INIT_QGL_FUNC(PolygonStipple);
    INIT_QGL_FUNC(PopAttrib);
    INIT_QGL_FUNC(PopClientAttrib);
    INIT_QGL_FUNC(PopMatrix);
    INIT_QGL_FUNC(PopName);
    INIT_QGL_FUNC(PrioritizeTextures);
    INIT_QGL_FUNC(PushAttrib);
    INIT_QGL_FUNC(PushClientAttrib);
    INIT_QGL_FUNC(PushMatrix);
    INIT_QGL_FUNC(PushName);
    INIT_QGL_FUNC(RasterPos2d);
    INIT_QGL_FUNC(RasterPos2dv);
    INIT_QGL_FUNC(RasterPos2f);
    INIT_QGL_FUNC(RasterPos2fv);
    INIT_QGL_FUNC(RasterPos2i);
    INIT_QGL_FUNC(RasterPos2iv);
    INIT_QGL_FUNC(RasterPos2s);
    INIT_QGL_FUNC(RasterPos2sv);
    INIT_QGL_FUNC(RasterPos3d);
    INIT_QGL_FUNC(RasterPos3dv);
    INIT_QGL_FUNC(RasterPos3f);
    INIT_QGL_FUNC(RasterPos3fv);
    INIT_QGL_FUNC(RasterPos3i);
    INIT_QGL_FUNC(RasterPos3iv);
    INIT_QGL_FUNC(RasterPos3s);
    INIT_QGL_FUNC(RasterPos3sv);
    INIT_QGL_FUNC(RasterPos4d);
    INIT_QGL_FUNC(RasterPos4dv);
    INIT_QGL_FUNC(RasterPos4f);
    INIT_QGL_FUNC(RasterPos4fv);
    INIT_QGL_FUNC(RasterPos4i);
    INIT_QGL_FUNC(RasterPos4iv);
    INIT_QGL_FUNC(RasterPos4s);
    INIT_QGL_FUNC(RasterPos4sv);
    INIT_QGL_FUNC(ReadBuffer);
    INIT_QGL_FUNC(ReadPixels);
    INIT_QGL_FUNC(Rectd);
    INIT_QGL_FUNC(Rectdv);
    INIT_QGL_FUNC(Rectf);
    INIT_QGL_FUNC(Rectfv);
    INIT_QGL_FUNC(Recti);
    INIT_QGL_FUNC(Rectiv);
    INIT_QGL_FUNC(Rects);
    INIT_QGL_FUNC(Rectsv);
    INIT_QGL_FUNC(RenderMode);
    INIT_QGL_FUNC(Rotated);
    INIT_QGL_FUNC(Rotatef);
    INIT_QGL_FUNC(Scalef);
    INIT_QGL_FUNC(Scaled);
    INIT_QGL_FUNC(Scissor);
    INIT_QGL_FUNC(SelectBuffer);
    INIT_QGL_FUNC(ShadeModel);
    INIT_QGL_FUNC(StencilFunc);
    INIT_QGL_FUNC(StencilMask);
    INIT_QGL_FUNC(StencilOp);
    INIT_QGL_FUNC(TexCoord1d);
    INIT_QGL_FUNC(TexCoord1dv);
    INIT_QGL_FUNC(TexCoord1f);
    INIT_QGL_FUNC(TexCoord1fv);
    INIT_QGL_FUNC(TexCoord1i);
    INIT_QGL_FUNC(TexCoord1iv);
    INIT_QGL_FUNC(TexCoord1s);
    INIT_QGL_FUNC(TexCoord1sv);
    INIT_QGL_FUNC(TexCoord2d);
    INIT_QGL_FUNC(TexCoord2dv);
    INIT_QGL_FUNC(TexCoord2f);
    INIT_QGL_FUNC(TexCoord2fv);
    INIT_QGL_FUNC(TexCoord2i);
    INIT_QGL_FUNC(TexCoord2iv);
    INIT_QGL_FUNC(TexCoord2s);
    INIT_QGL_FUNC(TexCoord2sv);
    INIT_QGL_FUNC(TexCoord3d);
    INIT_QGL_FUNC(TexCoord3dv);
    INIT_QGL_FUNC(TexCoord3f);
    INIT_QGL_FUNC(TexCoord3fv);
    INIT_QGL_FUNC(TexCoord3i);
    INIT_QGL_FUNC(TexCoord3iv);
    INIT_QGL_FUNC(TexCoord3s);
    INIT_QGL_FUNC(TexCoord3sv);
    INIT_QGL_FUNC(TexCoord4d);
    INIT_QGL_FUNC(TexCoord4dv);
    INIT_QGL_FUNC(TexCoord4f);
    INIT_QGL_FUNC(TexCoord4fv);
    INIT_QGL_FUNC(TexCoord4i);
    INIT_QGL_FUNC(TexCoord4iv);
    INIT_QGL_FUNC(TexCoord4s);
    INIT_QGL_FUNC(TexCoord4sv);
    INIT_QGL_FUNC(TexCoordPointer);
    INIT_QGL_FUNC(TexEnvf);
    INIT_QGL_FUNC(TexEnvfv);
    INIT_QGL_FUNC(TexEnvi);
    INIT_QGL_FUNC(TexEnviv);
    INIT_QGL_FUNC(TexGend);
    INIT_QGL_FUNC(TexGendv);
    INIT_QGL_FUNC(TexGenf);
    INIT_QGL_FUNC(TexGenfv);
    INIT_QGL_FUNC(TexGeni);
    INIT_QGL_FUNC(TexGeniv);
    INIT_QGL_FUNC(TexImage1D);
    INIT_QGL_FUNC(TexImage2D);
    INIT_QGL_FUNC(TexParameterf);
    INIT_QGL_FUNC(TexParameterfv);
    INIT_QGL_FUNC(TexParameteri);
    INIT_QGL_FUNC(TexParameteriv);
    INIT_QGL_FUNC(TexSubImage1D);
    INIT_QGL_FUNC(TexSubImage2D);
    INIT_QGL_FUNC(Translated);
    INIT_QGL_FUNC(Translatef);
    INIT_QGL_FUNC(Vertex2d);
    INIT_QGL_FUNC(Vertex2dv);
    INIT_QGL_FUNC(Vertex2f);
    INIT_QGL_FUNC(Vertex2fv);
    INIT_QGL_FUNC(Vertex2i);
    INIT_QGL_FUNC(Vertex2iv);
    INIT_QGL_FUNC(Vertex2s);
    INIT_QGL_FUNC(Vertex2sv);
    INIT_QGL_FUNC(Vertex3d);
    INIT_QGL_FUNC(Vertex3dv);
    INIT_QGL_FUNC(Vertex3f);
    INIT_QGL_FUNC(Vertex3fv);
    INIT_QGL_FUNC(Vertex3i);
    INIT_QGL_FUNC(Vertex3iv);
    INIT_QGL_FUNC(Vertex3s);
    INIT_QGL_FUNC(Vertex3sv);
    // Vertex4* функции удалены из OpenGL Core Profile 3.3+
    // INIT_QGL_FUNC(Vertex4d);
    // INIT_QGL_FUNC(Vertex4dv);
    // INIT_QGL_FUNC(Vertex4f);
    // INIT_QGL_FUNC(Vertex4fv);
    // INIT_QGL_FUNC(Vertex4i);
    // INIT_QGL_FUNC(Vertex4iv);
    // INIT_QGL_FUNC(Vertex4s);
    // INIT_QGL_FUNC(Vertex4sv);
    INIT_QGL_FUNC(VertexPointer);
    INIT_QGL_FUNC(Viewport);
    
    // Расширения - используем приведение типов для GLFWglproc
    qglMultiTexCoord2fARB = (PFNGLMULTITEXCOORD2FARBPROC)glfwGetProcAddress("glMultiTexCoord2fARB");
    qglActiveTextureARB = (PFNGLACTIVETEXTUREARBPROC)glfwGetProcAddress("glActiveTextureARB");
    qglClientActiveTextureARB = (PFNGLCLIENTACTIVETEXTUREARBPROC)glfwGetProcAddress("glClientActiveTextureARB");
    qglLockArraysEXT = (PFNGLLOCKARRAYSEXTPROC)glfwGetProcAddress("glLockArraysEXT");
    qglUnlockArraysEXT = (PFNGLUNLOCKARRAYSEXTPROC)glfwGetProcAddress("glUnlockArraysEXT");
    qglPNTrianglesiATI = (PFNGLPNTRIANGLESIATIPROC)glfwGetProcAddress("glPNTrianglesiATI");
    qglPNTrianglesfATI = (PFNGLPNTRIANGLESFATIPROC)glfwGetProcAddress("glPNTrianglesfATI");
}

// Глобальные переменные
static GLFWwindow* window = NULL;
static qboolean glfw_initialized = qfalse;
static qboolean mouse_grabbed = qfalse;
static int window_width = 640;
static int window_height = 480;
static qboolean fullscreen = qfalse;

// Внешние переменные из движка
extern refimport_t ri;

// Прототипы функций
static void GLFWAPI errorCallback(int error, const char* description);
static void GLFWAPI cursorPosCallback(GLFWwindow* window, double xpos, double ypos);
static void GLFWAPI mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
static void GLFWAPI scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
static void GLFWAPI keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
static void GLFWAPI framebufferSizeCallback(GLFWwindow* window, int width, int height);

// Таблица клавиш GLFW -> Quake
static int keymap[GLFW_KEY_LAST];

//===========================================================================
// Инициализация таблицы клавиш
//===========================================================================
static void InitKeymap(void) {
    int i;
    
    for (i = 0; i < GLFW_KEY_LAST; i++) {
        keymap[i] = K_NONE;
    }
    
    // Буквы
    keymap[GLFW_KEY_A] = K_A;
    keymap[GLFW_KEY_B] = K_B;
    keymap[GLFW_KEY_C] = K_C;
    keymap[GLFW_KEY_D] = K_D;
    keymap[GLFW_KEY_E] = K_E;
    keymap[GLFW_KEY_F] = K_F;
    keymap[GLFW_KEY_G] = K_G;
    keymap[GLFW_KEY_H] = K_H;
    keymap[GLFW_KEY_I] = K_I;
    keymap[GLFW_KEY_J] = K_J;
    keymap[GLFW_KEY_K] = K_K;
    keymap[GLFW_KEY_L] = K_L;
    keymap[GLFW_KEY_M] = K_M;
    keymap[GLFW_KEY_N] = K_N;
    keymap[GLFW_KEY_O] = K_O;
    keymap[GLFW_KEY_P] = K_P;
    keymap[GLFW_KEY_Q] = K_Q;
    keymap[GLFW_KEY_R] = K_R;
    keymap[GLFW_KEY_S] = K_S;
    keymap[GLFW_KEY_T] = K_T;
    keymap[GLFW_KEY_U] = K_U;
    keymap[GLFW_KEY_V] = K_V;
    keymap[GLFW_KEY_W] = K_W;
    keymap[GLFW_KEY_X] = K_X;
    keymap[GLFW_KEY_Y] = K_Y;
    keymap[GLFW_KEY_Z] = K_Z;
    
    // Цифры
    keymap[GLFW_KEY_0] = K_0;
    keymap[GLFW_KEY_1] = K_1;
    keymap[GLFW_KEY_2] = K_2;
    keymap[GLFW_KEY_3] = K_3;
    keymap[GLFW_KEY_4] = K_4;
    keymap[GLFW_KEY_5] = K_5;
    keymap[GLFW_KEY_6] = K_6;
    keymap[GLFW_KEY_7] = K_7;
    keymap[GLFW_KEY_8] = K_8;
    keymap[GLFW_KEY_9] = K_9;
    
    // Функциональные клавиши
    keymap[GLFW_KEY_F1] = K_F1;
    keymap[GLFW_KEY_F2] = K_F2;
    keymap[GLFW_KEY_F3] = K_F3;
    keymap[GLFW_KEY_F4] = K_F4;
    keymap[GLFW_KEY_F5] = K_F5;
    keymap[GLFW_KEY_F6] = K_F6;
    keymap[GLFW_KEY_F7] = K_F7;
    keymap[GLFW_KEY_F8] = K_F8;
    keymap[GLFW_KEY_F9] = K_F9;
    keymap[GLFW_KEY_F10] = K_F10;
    keymap[GLFW_KEY_F11] = K_F11;
    keymap[GLFW_KEY_F12] = K_F12;
    keymap[GLFW_KEY_F13] = K_F13;
    keymap[GLFW_KEY_F14] = K_F14;
    keymap[GLFW_KEY_F15] = K_F15;
    
    // Специальные клавиши
    keymap[GLFW_KEY_ESCAPE] = K_ESCAPE;
    keymap[GLFW_KEY_ENTER] = K_ENTER;
    keymap[GLFW_KEY_TAB] = K_TAB;
    keymap[GLFW_KEY_BACKSPACE] = K_BACKSPACE;
    keymap[GLFW_KEY_INSERT] = K_INS;
    keymap[GLFW_KEY_DELETE] = K_DEL;
    keymap[GLFW_KEY_PAGE_UP] = K_PGUP;
    keymap[GLFW_KEY_PAGE_DOWN] = K_PGDN;
    keymap[GLFW_KEY_HOME] = K_HOME;
    keymap[GLFW_KEY_END] = K_END;
    keymap[GLFW_KEY_UP] = K_UPARROW;
    keymap[GLFW_KEY_DOWN] = K_DOWNARROW;
    keymap[GLFW_KEY_LEFT] = K_LEFTARROW;
    keymap[GLFW_KEY_RIGHT] = K_RIGHTARROW;
    keymap[GLFW_KEY_SPACE] = K_SPACE;
    keymap[GLFW_KEY_MINUS] = K_MINUS;
    keymap[GLFW_KEY_EQUAL] = K_EQUALS;
    keymap[GLFW_KEY_LEFT_BRACKET] = K_LEFTBRACKET;
    keymap[GLFW_KEY_RIGHT_BRACKET] = K_RIGHTBRACKET;
    keymap[GLFW_KEY_BACKSLASH] = K_BACKSLASH;
    keymap[GLFW_KEY_SEMICOLON] = K_SEMICOLON;
    keymap[GLFW_KEY_APOSTROPHE] = K_APOSTROPHE;
    keymap[GLFW_KEY_COMMA] = K_COMMA;
    keymap[GLFW_KEY_PERIOD] = K_PERIOD;
    keymap[GLFW_KEY_SLASH] = K_SLASH;
    keymap[GLFW_KEY_GRAVE_ACCENT] = K_TILDE;
    keymap[GLFW_KEY_CAPS_LOCK] = K_CAPSLOCK;
    keymap[GLFW_KEY_SCROLL_LOCK] = K_SCROLLLOCK;
    keymap[GLFW_KEY_NUM_LOCK] = K_KP_NUMLOCK;
    keymap[GLFW_KEY_PRINT_SCREEN] = K_PRINTSCREEN;
    keymap[GLFW_KEY_PAUSE] = K_PAUSE;
    
    // Numpad - используем имена из keycodes.h
    keymap[GLFW_KEY_KP_0] = K_KP_INS;      // K_KP_0 не существует, используем K_KP_INS
    keymap[GLFW_KEY_KP_1] = K_KP_END;
    keymap[GLFW_KEY_KP_2] = K_KP_DOWNARROW;
    keymap[GLFW_KEY_KP_3] = K_KP_PGDN;
    keymap[GLFW_KEY_KP_4] = K_KP_LEFTARROW;
    keymap[GLFW_KEY_KP_5] = K_KP_5;
    keymap[GLFW_KEY_KP_6] = K_KP_RIGHTARROW;
    keymap[GLFW_KEY_KP_7] = K_KP_HOME;
    keymap[GLFW_KEY_KP_8] = K_KP_UPARROW;
    keymap[GLFW_KEY_KP_9] = K_KP_PGUP;
    keymap[GLFW_KEY_KP_DECIMAL] = K_KP_DEL;
    keymap[GLFW_KEY_KP_DIVIDE] = K_KP_SLASH;
    keymap[GLFW_KEY_KP_MULTIPLY] = K_KP_STAR;
    keymap[GLFW_KEY_KP_SUBTRACT] = K_KP_MINUS;
    keymap[GLFW_KEY_KP_ADD] = K_KP_PLUS;
    keymap[GLFW_KEY_KP_ENTER] = K_KP_ENTER;
    keymap[GLFW_KEY_NUM_LOCK] = K_KP_NUMLOCK;
    
    // Модификаторы - используем базовые имена
    keymap[GLFW_KEY_LEFT_SHIFT] = K_SHIFT;
    keymap[GLFW_KEY_RIGHT_SHIFT] = K_SHIFT;
    keymap[GLFW_KEY_LEFT_CONTROL] = K_CTRL;
    keymap[GLFW_KEY_RIGHT_CONTROL] = K_CTRL;
    keymap[GLFW_KEY_LEFT_ALT] = K_ALT;
    keymap[GLFW_KEY_RIGHT_ALT] = K_ALT;
    keymap[GLFW_KEY_LEFT_SUPER] = K_COMMAND;
    keymap[GLFW_KEY_RIGHT_SUPER] = K_COMMAND;
    keymap[GLFW_KEY_MENU] = K_PAUSE;  // K_MENU не существует
}

//===========================================================================
// Callback функции GLFW
//===========================================================================

static void GLFWAPI errorCallback(int error, const char* description) {
    Com_Printf("GLFW Error %d: %s\n", error, description);
}

static void GLFWAPI framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    window_width = width;
    window_height = height;
    glViewport(0, 0, width, height);
}

static void GLFWAPI cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    if (mouse_grabbed) {
        // В режиме захвата мыши используем относительное движение
        static double last_x = 0, last_y = 0;
        
        float mx = (float)(xpos - last_x);
        float my = (float)(ypos - last_y);
        
        last_x = xpos;
        last_y = ypos;
        
        // Отправляем события движения мыши в движок
        CL_MouseEvent((int)mx, (int)my, 0);
    }
}

static void GLFWAPI mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    int key;
    
    switch (button) {
        case GLFW_MOUSE_BUTTON_LEFT:
            key = K_MOUSE1;
            break;
        case GLFW_MOUSE_BUTTON_RIGHT:
            key = K_MOUSE2;
            break;
        case GLFW_MOUSE_BUTTON_MIDDLE:
            key = K_MOUSE3;
            break;
        case GLFW_MOUSE_BUTTON_4:
            key = K_MOUSE4;
            break;
        case GLFW_MOUSE_BUTTON_5:
            key = K_MOUSE5;
            break;
        default:
            return;
    }
    
    if (action == GLFW_PRESS) {
        CL_KeyEvent(key, qtrue, Sys_Milliseconds());
    } else if (action == GLFW_RELEASE) {
        CL_KeyEvent(key, qfalse, Sys_Milliseconds());
    }
}

static void GLFWAPI scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    if (yoffset > 0) {
        CL_KeyEvent(K_MWHEELUP, qtrue, Sys_Milliseconds());
        CL_KeyEvent(K_MWHEELUP, qfalse, Sys_Milliseconds());
    } else if (yoffset < 0) {
        CL_KeyEvent(K_MWHEELDOWN, qtrue, Sys_Milliseconds());
        CL_KeyEvent(K_MWHEELDOWN, qfalse, Sys_Milliseconds());
    }
}

static void GLFWAPI keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key >= 0 && key < GLFW_KEY_LAST) {
        int quake_key = keymap[key];
        
        // K_NONE не определён в keycodes.h, используем 0
        if (quake_key != 0) {
            if (action == GLFW_PRESS) {
                CL_KeyEvent(quake_key, qtrue, Sys_Milliseconds());
            } else if (action == GLFW_RELEASE) {
                CL_KeyEvent(quake_key, qfalse, Sys_Milliseconds());
            }
        }
    }
}

//===========================================================================
// Функции GLimp
//===========================================================================

// Определение rserr_t (из linux_glimp.c)
typedef enum
{
    RSERR_OK,
    RSERR_INVALID_FULLSCREEN,
    RSERR_INVALID_MODE,
    RSERR_UNKNOWN
} rserr_t;

#define WINDOW_CLASS_NAME "RTCW"

/*
=================
GLimp_Init
=================
*/
void GLimp_Init(void) {
    int major, minor;
    const char* version;
    
    Com_Printf("------- GLimp Initialization -------\n");
    
    if (glfw_initialized) {
        Com_Printf("GLFW уже инициализирован\n");
        return;
    }
    
    // Инициализация GLFW
    glfwSetErrorCallback(errorCallback);
    
    if (!glfwInit()) {
        Com_Printf("Не удалось инициализировать GLFW\n");
        return;
    }
    
    glfw_initialized = qtrue;
    
    // Настройка версии OpenGL
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_RED_BITS, 8);
    glfwWindowHint(GLFW_GREEN_BITS, 8);
    glfwWindowHint(GLFW_BLUE_BITS, 8);
    glfwWindowHint(GLFW_ALPHA_BITS, 8);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);
    glfwWindowHint(GLFW_STENCIL_BITS, 8);
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
    
    // Получение текущего режима
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    
    window_width = r_mode->integer < 0 ? 640 : mode->width;
    window_height = r_mode->integer < 0 ? 480 : mode->height;
    fullscreen = r_fullscreen->integer ? qtrue : qfalse;
    
    // Создание окна
    if (fullscreen) {
        window = glfwCreateWindow(mode->width, mode->height, 
                                  WINDOW_CLASS_NAME, monitor, NULL);
    } else {
        window = glfwCreateWindow(window_width, window_height, 
                                  WINDOW_CLASS_NAME, NULL, NULL);
    }
    
    if (!window) {
        Com_Printf("Не удалось создать окно GLFW\n");
        glfwTerminate();
        glfw_initialized = qfalse;
        return;
    }
    
    glfwMakeContextCurrent(window);
    
    // Инициализация указателей на функции OpenGL
    InitQGLFunctions();
    
    // Получение информации о контексте
    version = (const char*)glGetString(GL_VERSION);
    major = glfwGetWindowAttrib(window, GLFW_CONTEXT_VERSION_MAJOR);
    minor = glfwGetWindowAttrib(window, GLFW_CONTEXT_VERSION_MINOR);
    
    Com_Printf("OpenGL Version: %s\n", version);
    Com_Printf("OpenGL Context: %d.%d\n", major, minor);
    Com_Printf("GL_VENDOR: %s\n", glGetString(GL_VENDOR));
    Com_Printf("GL_RENDERER: %s\n", glGetString(GL_RENDERER));
    
    // Настройка callback функций
    InitKeymap();
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetKeyCallback(window, keyCallback);
    
    // Начальная настройка viewport
    glfwGetFramebufferSize(window, &window_width, &window_height);
    glViewport(0, 0, window_width, window_height);
    
    // Настройка VSync
    glfwSwapInterval(r_swapInterval->integer ? 1 : 0);
    
    Com_Printf("------------------------------------\n");
}

/*
=================
GLimp_Shutdown
=================
*/
void GLimp_Shutdown(void) {
    Com_Printf("GLimp_Shutdown\n");
    
    if (window) {
        glfwDestroyWindow(window);
        window = NULL;
    }
    
    if (glfw_initialized) {
        glfwTerminate();
        glfw_initialized = qfalse;
    }
}

/*
=================
GLimp_EndFrame
=================
*/
void GLimp_EndFrame(void) {
    if (window) {
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
}

/*
=================
GLimp_SetGamma
=================
*/
void GLimp_SetGamma(unsigned char red[256], unsigned char green[256], unsigned char blue[256]) {
    // В современном OpenGL гамма-коррекция делается через шейдеры или framebuffer
    // Эта функция может быть реализована через lookup texture
}

/*
=================
GLimp_SwitchFullscreen
=================
*/
qboolean GLimp_SwitchFullscreen(int width, int height) {
    if (!window) {
        return qfalse;
    }
    
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    
    if (fullscreen) {
        // Переход в оконный режим
        glfwSetWindowMonitor(window, NULL, 
                            (mode->width - width) / 2, 
                            (mode->height - height) / 2,
                            width, height, 0);
        fullscreen = qfalse;
    } else {
        // Переход в полноэкранный режим
        glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        fullscreen = qtrue;
    }
    
    return qtrue;
}

/*
=================
GLimp_GetClipboardData
=================
*/
char* GLimp_GetClipboardData(void) {
    if (!window) {
        return NULL;
    }
    
    const char* text = glfwGetClipboardString(window);
    if (text) {
        return strdup(text);
    }
    return NULL;
}

/*
=================
GLimp_SetClipboardData
=================
*/
void GLimp_SetClipboardData(const char* text) {
    if (window) {
        glfwSetClipboardString(window, text);
    }
}

/*
=================
Sys_ShowConsole
=================
*/
void Sys_ShowConsole(int level, qboolean show) {
    // Консоль отображается внутри игры
}

/*
=================
Conbuf_InterceptPrint
=================
*/
void Conbuf_InterceptPrint(const char* msg) {
    // Перехват вывода консоли (опционально)
}
