/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Return to Castle Wolfenstein source code.

Return to Castle Wolfenstein source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

Return to Castle Wolfenstein source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Return to Castle Wolfenstein source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

// bg_lib.h - Compatibility header for standard library functions
// This file provides compatibility with the original RTCW codebase
// On modern systems, we use standard C library functions directly

#ifndef __BG_LIB_H__
#define __BG_LIB_H__

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

// Standard library functions are available directly from C library
// No need to redefine them

// Memory functions
#define Com_Memset memset
#define Com_Memcpy memcpy
#define Com_Memcmp memcmp

// String functions
#define Com_Strlen strlen
#define Com_Strcpy strcpy
#define Com_Strncpy strncpy
#define Com_Strcat strcat
#define Com_Strncmp strncmp
#define Com_Strstr strstr
#define Com_Sprintf sprintf

// Math functions
#define Com_Fabs fabs
#define Com_Sqrt sqrt
#define Com_Floor floor
#define Com_Ceil ceil
#define Com_Sin sin
#define Com_Cos cos
#define Com_Tan tan
#define Com_Atan atan
#define Com_Atan2 atan2
#define Com_Pow pow
#define Com_Exp exp
#define Com_Log log
#define Com_Log10 log10

// Random number generation
#define Com_Rand rand
#define Com_Srand srand

#endif // __BG_LIB_H__
