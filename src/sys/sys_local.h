/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#ifndef __SYS_LOCAL_H__
#define __SYS_LOCAL_H__

#include "qcommon/q_shared.h"

// System-dependent functions
void Sys_Init( void );
sysEvent_t Sys_GetEvent( void );
void Sys_Shutdown( void );
int Sys_Milliseconds( void );
void Sys_Error( const char *error, ... );
void Sys_Print( const char *msg );
void Sys_UnloadGame( void );
void *Sys_GetGameAPI( void *parms );
void Sys_UnloadDll( void *dllHandle );
void *Sys_LoadDll( const char *name, int (QDECL **entryPoint)(int, ...), int (QDECL *kernel)(int, ...) );

#endif // __SYS_LOCAL_H__
