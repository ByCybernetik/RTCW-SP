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

#ifndef __SDL_LOCAL_H__
#define __SDL_LOCAL_H__

#include <SDL2/SDL.h>
#include "../game/q_shared.h"

// Глобальное состояние SDL
typedef struct {
    SDL_Window *window;
    SDL_GLContext glcontext;
    qboolean initialized;
    qboolean fullscreen;
    int width;
    int height;
} sdlState_t;

extern sdlState_t sdl_state;

// Функции OpenGL реализации
qboolean GLimp_Init( void );
void GLimp_Shutdown( void );
rserr_t GLimp_SetMode( int *pwidth, int *pheight, int mode, qboolean fullscreen );
void GLimp_EndFrame( void );
qboolean GLimp_SwitchFullscreen( int width, int height );
void GLimp_SetGamma( unsigned char red[256], unsigned char green[256], unsigned char blue[256] );
void GLimp_GetCurrentVideoMode( int *width, int *height );
void GLimp_LogComment( char *comment );

// Функции ввода
void IN_Init( void );
void IN_Frame( void );
void IN_Shutdown( void );
void IN_MapKey( SDL_Keycode sym, int *key, int *down );
void IN_MouseEvent( int mstate );

// Системные функции
void Sys_Init( void );
void Sys_Quit( void );
int Sys_Milliseconds( void );
void Sys_Sleep( int msec );

#endif // __SDL_LOCAL_H__
