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

// sys_sdl_main.c - SDL2-based system layer for RTCW SP

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include "../renderer/tr_public.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>

static SDL_Window *sdl_window = NULL;
static SDL_GLContext sdl_glcontext = NULL;
static qboolean sdl_initialized = qfalse;
static int sdl_screen_width = 0;
static int sdl_screen_height = 0;

// Console variables
static cvar_t *r_sdlDriver = NULL;
static cvar_t *in_sdlGrab = NULL;

/*
=================
Sys_Init
=================
*/
void Sys_Init( void ) {
	r_sdlDriver = Cvar_Get( "r_sdlDriver", "1", CVAR_ARCHIVE );
	in_sdlGrab = Cvar_Get( "in_sdlGrab", "0", CVAR_ARCHIVE );
	
	Com_Printf( "SDL initialized\n" );
}

/*
=================
Sys_Quit
=================
*/
void Sys_Quit( void ) {
	if ( sdl_glcontext ) {
		SDL_GL_DeleteContext( sdl_glcontext );
		sdl_glcontext = NULL;
	}
	
	if ( sdl_window ) {
		SDL_DestroyWindow( sdl_window );
		sdl_window = NULL;
	}
	
	SDL_Quit();
	sdl_initialized = qfalse;
}

/*
=================
Sys_Error
=================
*/
void QDECL Sys_Error( const char *error, ... ) {
	va_list argptr;
	char text[1024];
	
	va_start( argptr, error );
	Q_vsnprintf( text, sizeof( text ), error, argptr );
	va_end( argptr );
	
	SDL_ShowSimpleMessageBox( SDL_MESSAGEBOX_ERROR, "Error", text, sdl_window );
	
	Sys_Quit();
	exit( 1 );
}

/*
=================
Sys_Milliseconds
=================
*/
int Sys_Milliseconds( void ) {
	static Uint32 base = 0;
	Uint32 now;
	
	if ( !base ) {
		base = SDL_GetTicks();
		return 0;
	}
	
	now = SDL_GetTicks() - base;
	return (int)now;
}

/*
=================
Sys_Sleep
=================
*/
void Sys_Sleep( int msec ) {
	SDL_Delay( msec );
}

/*
=================
SNPrintf
=================
*/
int QDECL SNPrintf( char *dest, size_t size, const char *fmt, ... ) {
	va_list argptr;
	int val;
	
	va_start( argptr, fmt );
	val = vsnprintf( dest, size, fmt, argptr );
	va_end( argptr );
	
	return val;
}

/*
=================
Sys_GetClipboardData
=================
*/
char *Sys_GetClipboardData( void ) {
	char *text = NULL;
	char *clipText;
	
	clipText = SDL_GetClipboardText();
	if ( clipText && clipText[0] ) {
		size_t len = strlen( clipText ) + 1;
		text = Z_Malloc( len );
		strcpy( text, clipText );
		SDL_free( clipText );
		
		// Convert line endings
		char *p = text;
		while ( *p ) {
			if ( *p == '\n' ) {
				memmove( p + 1, p, strlen( p ) + 1 );
				*p = '\r';
			}
			p++;
		}
	}
	
	return text;
}

/*
=================
Sys_CopyToClipboard
=================
*/
void Sys_CopyToClipboard( const char *text ) {
	char *temp = NULL;
	const char *p;
	size_t len;
	
	// Convert \r\n to \n
	len = strlen( text ) + 1;
	temp = Z_Malloc( len );
	strcpy( temp, text );
	
	p = temp;
	while ( *p ) {
		if ( *p == '\r' ) {
			memmove( p, p + 1, strlen( p ) );
		} else {
			p++;
		}
	}
	
	SDL_SetClipboardText( temp );
	Z_Free( temp );
}

/*
=================
Sys_CreateWindow
=================
*/
qboolean Sys_CreateWindow( const char *title, int x, int y, int width, int height, 
						   qboolean fullscreen, qboolean opengl ) {
	Uint32 flags = 0;
	
	if ( opengl ) {
		flags |= SDL_WINDOW_OPENGL;
	}
	
	if ( fullscreen ) {
		flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	}
	
	if ( sdl_window ) {
		SDL_DestroyWindow( sdl_window );
	}
	
	sdl_window = SDL_CreateWindow( title, x, y, width, height, flags );
	
	if ( !sdl_window ) {
		Com_Printf( S_COLOR_RED "ERROR: Could not create SDL window: %s\n", SDL_GetError() );
		return qfalse;
	}
	
	if ( opengl ) {
		sdl_glcontext = SDL_GL_CreateContext( sdl_window );
		
		if ( !sdl_glcontext ) {
			Com_Printf( S_COLOR_RED "ERROR: Could not create OpenGL context: %s\n", SDL_GetError() );
			SDL_DestroyWindow( sdl_window );
			sdl_window = NULL;
			return qfalse;
		}
		
		SDL_GL_SetSwapInterval( r_swapInterval->integer );
	}
	
	sdl_screen_width = width;
	sdl_screen_height = height;
	sdl_initialized = qtrue;
	
	return qtrue;
}

/*
=================
Sys_DestroyWindow
=================
*/
void Sys_DestroyWindow( void ) {
	if ( sdl_glcontext ) {
		SDL_GL_DeleteContext( sdl_glcontext );
		sdl_glcontext = NULL;
	}
	
	if ( sdl_window ) {
		SDL_DestroyWindow( sdl_window );
		sdl_window = NULL;
	}
	
	sdl_initialized = qfalse;
}

/*
=================
Sys_GetWindowHandle
=================
*/
void *Sys_GetWindowHandle( void ) {
	return sdl_window;
}

/*
=================
Sys_GetGLContext
=================
*/
void *Sys_GetGLContext( void ) {
	return sdl_glcontext;
}

/*
=================
Sys_SwapBuffers
=================
*/
void Sys_SwapBuffers( void ) {
	if ( sdl_glcontext ) {
		SDL_GL_SwapWindow( sdl_window );
	}
}

/*
=================
Sys_GetScreenSize
=================
*/
void Sys_GetScreenSize( int *width, int *height ) {
	if ( width ) *width = sdl_screen_width;
	if ( height ) *height = sdl_screen_height;
}

/*
=================
Sys_SetWindowTitle
=================
*/
void Sys_SetWindowTitle( const char *title ) {
	if ( sdl_window ) {
		SDL_SetWindowTitle( sdl_window, title );
	}
}

/*
=================
Sys_GrabCursor
=================
*/
void Sys_GrabCursor( qboolean grab ) {
	if ( grab ) {
		SDL_SetRelativeMouseMode( SDL_TRUE );
	} else {
		SDL_SetRelativeMouseMode( SDL_FALSE );
	}
}

/*
=================
Sys_GetCurrentTime
=================
*/
unsigned long Sys_CurrentTime( void ) {
	return (unsigned long)SDL_GetTicks();
}
