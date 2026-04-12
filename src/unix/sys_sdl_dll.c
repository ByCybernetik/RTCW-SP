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

// sys_sdl_dll.c - SDL2-based DLL loading for cross-platform support

#include "q_shared.h"
#include "../qcommon/qcommon.h"
#include <SDL2/SDL_loadso.h>

/*
=================
Sys_UnloadDll

Unload a dynamically loaded library
=================
*/
void Sys_UnloadDll( void *dllHandle ) {
	if ( !dllHandle ) {
		return;
	}
	SDL_UnloadObject( dllHandle );
}

/*
=================
Sys_LoadDll

Load a native DLL library (replaces VM system)
=================
*/
extern char *FS_BuildOSPath( const char *base, const char *game, const char *qpath );

void * QDECL Sys_LoadDll( const char *name, int( QDECL **entryPoint )( int, ... ),
						  int ( QDECL *systemcalls )( int, ... ) ) {
	void *libHandle;
	void ( QDECL *dllEntry )( int ( QDECL *syscallptr )( int, ... ) );
	char filename[MAX_QPATH];
	char *basepath;
	char *gamedir;
	char *fn;

#ifdef _WIN32
	Com_sprintf( filename, sizeof( filename ), "%s.dll", name );
#elif defined(__APPLE__)
	Com_sprintf( filename, sizeof( filename ), "%s.dylib", name );
#else
	Com_sprintf( filename, sizeof( filename ), "%s.so", name );
#endif

	// Try to load from current directory first (for development)
	libHandle = SDL_LoadObject( filename );
	
	if ( !libHandle ) {
		// Try base path
		basepath = Cvar_VariableString( "fs_basepath" );
		gamedir = Cvar_VariableString( "fs_game" );
		
		fn = FS_BuildOSPath( basepath, gamedir, filename );
		libHandle = SDL_LoadObject( fn );
	}

	if ( !libHandle ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: Could not load DLL '%s': %s\n", 
					filename, SDL_GetError() );
		return NULL;
	}

	dllEntry = ( void ( QDECL * )( int ( QDECL * )( int, ... ) ) )SDL_LoadFunction( libHandle, "dllEntry" );
	*entryPoint = ( int ( QDECL * )( int,... ) )SDL_LoadFunction( libHandle, "vmMain" );
	
	if ( !*entryPoint || !dllEntry ) {
		Com_Printf( S_COLOR_RED "ERROR: DLL '%s' missing required exports (dllEntry/vmMain)\n", filename );
		SDL_UnloadObject( libHandle );
		return NULL;
	}
	
	dllEntry( systemcalls );

	Com_Printf( "Loaded DLL: %s\n", filename );
	return libHandle;
}
