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
along with RTCW SP Source Code.  If not, see <www.gnu.org/licenses/>.

In addition, the RTCW SP Source Code is also subject to certain additional terms. 
You should have received a copy of these additional terms immediately following the terms and conditions of the 
GNU General Public License which accompanied the RTCW SP Source Code.  
If not, please request a copy in writing from id Software at the address below.  

If you have questions concerning this license or the applicable additional terms, you may contact in writing 
id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

// cl_splines_stub.c - Stub implementations for camera spline functions
// These are temporary stubs to allow building without full splines system

#include "../game/q_shared.h"

qboolean loadCamera( int camNum, const char *name ) {
    (void)camNum;
    (void)name;
    return qfalse;
}

void startCamera( int camNum, int time ) {
    (void)camNum;
    (void)time;
}

qboolean getCameraInfo( int camNum, int time, vec3_t origin, vec3_t angles, float *fov ) {
    (void)camNum;
    (void)time;
    (void)origin;
    (void)angles;
    (void)fov;
    return qfalse;
}
