/*
===========================================================================

Return to Castle Wolfenstein single player GPL Source Code
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company. 

This file is part of the Return to Castle Wolfenstein single player GPL Source Code (RTCW SP Source Code).  

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

// cl_scrn.c -- master for refresh, status bar, console, chat, notify, etc

#include "client.h"

qboolean scr_initialized;           // ready to draw

cvar_t      *cl_timegraph;
cvar_t      *cl_debuggraph;
cvar_t      *cl_graphheight;
cvar_t      *cl_graphscale;
cvar_t      *cl_graphshift;

/*
================
SCR_DrawNamedPic

Coordinates are 640*480 virtual values
=================
*/
void SCR_DrawNamedPic( float x, float y, float width, float height, const char *picname ) {
	qhandle_t hShader;

	assert( width != 0 );

	hShader = re.RegisterShader( picname );
	SCR_AdjustFrom640( &x, &y, &width, &height );
	re.DrawStretchPic( x, y, width, height, 0, 0, 1, 1, hShader );
}


/*
================
SCR_AdjustFrom640

Adjusted for resolution and screen aspect ratio
================
*/
void SCR_AdjustFrom640( float *x, float *y, float *w, float *h ) {
	float xscale;
	float yscale;

#if 0
	// adjust for wide screens
	if ( cls.glconfig.vidWidth * 480 > cls.glconfig.vidHeight * 640 ) {
		*x += 0.5 * ( cls.glconfig.vidWidth - ( cls.glconfig.vidHeight * 640 / 480 ) );
	}
#endif

	// scale for screen sizes
	xscale = cls.glconfig.vidWidth / 640.0;
	yscale = cls.glconfig.vidHeight / 480.0;
	if ( x ) {
		*x *= xscale;
	}
	if ( y ) {
		*y *= yscale;
	}
	if ( w ) {
		*w *= xscale;
	}
	if ( h ) {
		*h *= yscale;
	}
}

/*
================
SCR_FillRect

Coordinates are 640*480 virtual values
=================
*/
void SCR_FillRect( float x, float y, float width, float height, const float *color ) {
	re.SetColor( color );

	SCR_AdjustFrom640( &x, &y, &width, &height );
	re.DrawStretchPic( x, y, width, height, 0, 0, 0, 0, cls.whiteShader );

	re.SetColor( NULL );
}


/*
================
SCR_DrawPic

Coordinates are 640*480 virtual values
=================
*/
void SCR_DrawPic( float x, float y, float width, float height, qhandle_t hShader ) {
	SCR_AdjustFrom640( &x, &y, &width, &height );
	re.DrawStretchPic( x, y, width, height, 0, 0, 1, 1, hShader );
}



/*
** SCR_DrawChar
** chars are drawn at 640*480 virtual screen size
*/
static void SCR_DrawChar( int x, int y, float size, int ch ) {
	int row, col;
	float frow, fcol;
	float ax, ay, aw, ah;

	ch &= 255;

	if ( ch == ' ' ) {
		return;
	}

	if ( y < -size ) {
		return;
	}

	ax = x;
	ay = y;
	aw = size;
	ah = size;
	SCR_AdjustFrom640( &ax, &ay, &aw, &ah );

	row = ch >> 4;
	col = ch & 15;

	frow = row * 0.0625;
	fcol = col * 0.0625;
	size = 0.0625;

	re.DrawStretchPic( ax, ay, aw, ah,
					   fcol, frow,
					   fcol + size, frow + size,
					   cls.charSetShader );
}

/*
** SCR_DrawSmallChar
** small chars are drawn at native screen resolution
*/
void SCR_DrawSmallChar( int x, int y, int ch ) {
	int row, col;
	float frow, fcol;
	float size;

	ch &= 255;

	if ( ch == ' ' ) {
		return;
	}

	if ( y < -SMALLCHAR_HEIGHT ) {
		return;
	}

	row = ch >> 4;
	col = ch & 15;

	frow = row * 0.0625;
	fcol = col * 0.0625;
	size = 0.0625;

	re.DrawStretchPic( x, y, SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT,
					   fcol, frow,
					   fcol + size, frow + size,
					   cls.charSetShader );
}

static fontInfo_t cl_consoleFont;
static qboolean cl_consoleFontLoaded = qfalse;

static void SCR_LoadConsoleFont( void ) {
	if ( cl_consoleFontLoaded ) {
		return;
	}
	re.RegisterFont( "fonts/impact.ttf", 12, &cl_consoleFont );
	if ( cl_consoleFont.numRanges > 0 ) {
		cl_consoleFontLoaded = qtrue;
	}
}

/*
==================
SCR_ConsoleScale

2D scaling factor matching cgame/UI: vidHeight / 480.
==================
*/
static float SCR_ConsoleScale( void ) {
	return cls.glconfig.vidHeight / 480.0f;
}

/*
==================
SCR_DrawConsoleGlyph

x/y are native screen pixels. The glyph is scaled by vidHeight/480 to match
the rest of the TTF text. y is the top of the console cell; baseline is
derived internally. Returns horizontal advance in screen pixels.
==================
*/
static float SCR_DrawConsoleGlyph( float x, float y, int codepoint, vec4_t color ) {
	glyphInfo_t *glyph;
	float scale;
	float useScale;
	float screenScale;
	float w, h;
	float yadj;
	float baseline;

	if ( !cl_consoleFontLoaded ) {
		SCR_LoadConsoleFont();
	}
	if ( !cl_consoleFontLoaded ) {
		return SMALLCHAR_WIDTH;
	}

	scale = 0.12f;
	useScale = scale * cl_consoleFont.glyphScale;
	screenScale = SCR_ConsoleScale();

	if ( codepoint == ' ' ) {
		glyph = R_GetGlyph( &cl_consoleFont, ' ' );
		if ( glyph && glyph->glyph ) {
			return glyph->xSkip * useScale * screenScale;
		}
		return SMALLCHAR_WIDTH * scale * screenScale;
	}

	glyph = R_GetGlyph( &cl_consoleFont, codepoint );
	if ( !glyph || glyph->glyph == 0 ) {
		return SMALLCHAR_WIDTH * scale * screenScale;
	}

	w = glyph->imageWidth * useScale * screenScale;
	h = glyph->imageHeight * useScale * screenScale;
	yadj = useScale * glyph->top * screenScale;
	baseline = y + SMALLCHAR_HEIGHT * 0.8f;

	re.SetColor( color );
	re.DrawStretchPic( x, baseline - yadj, w, h, glyph->s, glyph->t, glyph->s2, glyph->t2, glyph->glyph );
	return glyph->xSkip * useScale * screenScale;
}

/*
==================
SCR_DrawConsoleString

Draws a console line stored as a short array (low byte = UTF-8 byte,
high byte = color index). x/y and advance are native screen pixels.
==================
*/
void SCR_DrawConsoleString( int x, int y, const short *text, int len ) {
	int i;
	int currentColor;
	float cx;
	vec4_t color;
	char utf8buf[8];
	int charLen;
	int codepoint;
	int idx;

	if ( !cl_consoleFontLoaded ) {
		SCR_LoadConsoleFont();
	}
	if ( !cl_consoleFontLoaded ) {
		return;
	}

	currentColor = 7;
	memcpy( color, g_color_table[currentColor], sizeof( vec4_t ) );
	re.SetColor( color );

	cx = x;
	i = 0;
	while ( i < len ) {
		unsigned char b = text[i] & 0xff;

		if ( b == ' ' ) {
			cx += SCR_DrawConsoleGlyph( cx, y, ' ', color );
			i++;
			continue;
		}

		if ( ( b & 0x80 ) == 0 ) {
			charLen = 1;
		} else if ( ( b & 0xE0 ) == 0xC0 ) {
			charLen = 2;
		} else if ( ( b & 0xF0 ) == 0xE0 ) {
			charLen = 3;
		} else if ( ( b & 0xF8 ) == 0xF0 ) {
			charLen = 4;
		} else {
			i++;
			continue;
		}
		if ( i + charLen > len ) {
			break;
		}

		if ( ( ( text[i] >> 8 ) & 7 ) != currentColor ) {
			currentColor = ( text[i] >> 8 ) & 7;
			memcpy( color, g_color_table[currentColor], sizeof( vec4_t ) );
			re.SetColor( color );
		}

		for ( idx = 0; idx < charLen; idx++ ) {
			utf8buf[idx] = text[i + idx] & 0xff;
		}
		utf8buf[charLen] = 0;
		idx = 0;
		codepoint = Q_UTF8_ReadChar( utf8buf, &idx );

		cx += SCR_DrawConsoleGlyph( cx, y, codepoint, color );
		i += charLen;
	}

	re.SetColor( NULL );
}

/*
==================
SCR_DrawConsoleStringChar

Draws a plain UTF-8 char string with embedded color codes.
x/y and advance are native screen pixels.
==================
*/
void SCR_DrawConsoleStringChar( int x, int y, const char *text, vec4_t color ) {
	const char *s;
	float cx;
	int idx;
	int codepoint;

	if ( !cl_consoleFontLoaded ) {
		SCR_LoadConsoleFont();
	}
	if ( !cl_consoleFontLoaded ) {
		return;
	}

	re.SetColor( color );
	s = text;
	cx = x;
	while ( *s ) {
		if ( Q_IsColorString( s ) ) {
			memcpy( color, g_color_table[ColorIndex( *( s + 1 ) )], sizeof( vec4_t ) );
			re.SetColor( color );
			s += 2;
			continue;
		}
		idx = 0;
		codepoint = Q_UTF8_ReadChar( s, &idx );
		cx += SCR_DrawConsoleGlyph( cx, y, codepoint, color );
		s += idx;
	}

	re.SetColor( NULL );
}

/*
==================
SCR_DrawConsoleStringFixed

Fixed-width console string for edit fields. Each glyph is forced into a
SMALLCHAR_WIDTH * 0.12 * screenScale wide cell so the cursor math in
Field_Draw stays valid.
==================
*/
void SCR_DrawConsoleStringFixed( int x, int y, const char *text, vec4_t color ) {
	const char *s;
	float cx;
	float cellWidth;
	int idx;
	int codepoint;

	if ( !cl_consoleFontLoaded ) {
		SCR_LoadConsoleFont();
	}
	if ( !cl_consoleFontLoaded ) {
		return;
	}

	cellWidth = SMALLCHAR_WIDTH * SCR_ConsoleScale();

	re.SetColor( color );
	s = text;
	cx = x;
	while ( *s ) {
		if ( Q_IsColorString( s ) ) {
			memcpy( color, g_color_table[ColorIndex( *( s + 1 ) )], sizeof( vec4_t ) );
			re.SetColor( color );
			s += 2;
			continue;
		}
		idx = 0;
		codepoint = Q_UTF8_ReadChar( s, &idx );
		SCR_DrawConsoleGlyph( cx, y, codepoint, color );
		cx += cellWidth;
		s += idx;
	}

	re.SetColor( NULL );
}

/*
==================
SCR_ConsoleStringWidth

Returns the screen-pixel width of a UTF-8 string using the console font.
==================
*/
float SCR_ConsoleStringWidth( const char *text ) {
	const char *s;
	float scale;
	float useScale;
	float screenScale;
	float width;
	int idx;
	int codepoint;
	glyphInfo_t *glyph;

	if ( !cl_consoleFontLoaded ) {
		SCR_LoadConsoleFont();
	}
	if ( !cl_consoleFontLoaded ) {
		return strlen( text ) * SMALLCHAR_WIDTH;
	}

	scale = 0.12f;
	useScale = scale * cl_consoleFont.glyphScale;
	screenScale = SCR_ConsoleScale();
	width = 0.0f;
	s = text;
	while ( *s ) {
		if ( Q_IsColorString( s ) ) {
			s += 2;
			continue;
		}
		idx = 0;
		codepoint = Q_UTF8_ReadChar( s, &idx );
		glyph = R_GetGlyph( &cl_consoleFont, codepoint );
		if ( glyph && glyph->glyph ) {
			width += glyph->xSkip * useScale * screenScale;
		} else {
			width += SMALLCHAR_WIDTH * scale * screenScale;
		}
		s += idx;
	}

	return width;
}


/*
==================
SCR_DrawBigString[Color]

Draws a multi-colored string with a drop shadow, optionally forcing
to a fixed color.

Coordinates are at 640 by 480 virtual resolution
==================
*/
void SCR_DrawStringExt( int x, int y, float size, const char *string, float *setColor, qboolean forceColor ) {
	vec4_t color;
	const char  *s;
	int xx;

	// draw the drop shadow
	color[0] = color[1] = color[2] = 0;
	color[3] = setColor[3];
	re.SetColor( color );
	s = string;
	xx = x;
	while ( *s ) {
		if ( Q_IsColorString( s ) ) {
			s += 2;
			continue;
		}
		SCR_DrawChar( xx + 2, y + 2, size, *s );
		xx += size;
		s++;
	}


	// draw the colored text
	s = string;
	xx = x;
	re.SetColor( setColor );
	while ( *s ) {
		if ( Q_IsColorString( s ) ) {
			if ( !forceColor ) {
				memcpy( color, g_color_table[ColorIndex( *( s + 1 ) )], sizeof( color ) );
				color[3] = setColor[3];
				re.SetColor( color );
			}
			s += 2;
			continue;
		}
		SCR_DrawChar( xx, y, size, *s );
		xx += size;
		s++;
	}
	re.SetColor( NULL );
}


void SCR_DrawBigString( int x, int y, const char *s, float alpha ) {
	float color[4];

	color[0] = color[1] = color[2] = 1.0;
	color[3] = alpha;
	SCR_DrawStringExt( x, y, BIGCHAR_WIDTH, s, color, qfalse );
}

void SCR_DrawBigStringColor( int x, int y, const char *s, vec4_t color ) {
	SCR_DrawStringExt( x, y, BIGCHAR_WIDTH, s, color, qtrue );
}


/*
==================
SCR_DrawSmallString[Color]

Draws a multi-colored string with a drop shadow, optionally forcing
to a fixed color.

Coordinates are at 640 by 480 virtual resolution
==================
*/
void SCR_DrawSmallStringExt( int x, int y, const char *string, float *setColor, qboolean forceColor ) {
	vec4_t color;
	const char  *s;
	int xx;

	// draw the colored text
	s = string;
	xx = x;
	re.SetColor( setColor );
	while ( *s ) {
		if ( Q_IsColorString( s ) ) {
			if ( !forceColor ) {
				memcpy( color, g_color_table[ColorIndex( *( s + 1 ) )], sizeof( color ) );
				color[3] = setColor[3];
				re.SetColor( color );
			}
			s += 2;
			continue;
		}
		SCR_DrawSmallChar( xx, y, *s );
		xx += SMALLCHAR_WIDTH;
		s++;
	}
	re.SetColor( NULL );
}



/*
** SCR_Strlen -- skips color escape codes
*/
static int SCR_Strlen( const char *str ) {
	const char *s = str;
	int count = 0;

	while ( *s ) {
		if ( Q_IsColorString( s ) ) {
			s += 2;
		} else {
			count++;
			s++;
		}
	}

	return count;
}

/*
** SCR_GetBigStringWidth
*/
int SCR_GetBigStringWidth( const char *str ) {
	return SCR_Strlen( str ) * 16;
}


//===============================================================================

/*
=================
SCR_DrawDemoRecording
=================
*/
void SCR_DrawDemoRecording( void ) {
	char string[1024];
	int pos;

	if ( !clc.demorecording ) {
		return;
	}

	pos = FS_FTell( clc.demofile );
	sprintf( string, "RECORDING %s: %ik", clc.demoName, pos / 1024 );

	SCR_DrawStringExt( 320 - strlen( string ) * 4, 20, 8, string, g_color_table[7], qtrue );
}


/*
===============================================================================

DEBUG GRAPH

===============================================================================
*/

typedef struct
{
	float value;
	int color;
} graphsamp_t;

static int current;
static graphsamp_t values[1024];

/*
==============
SCR_DebugGraph
==============
*/
void SCR_DebugGraph( float value, int color ) {
	values[current & 1023].value = value;
	values[current & 1023].color = color;
	current++;
}

/*
==============
SCR_DrawDebugGraph
==============
*/
void SCR_DrawDebugGraph( void ) {
	int a, x, y, w, i, h;
	float v;
	int color;

	//
	// draw the graph
	//
	w = cls.glconfig.vidWidth;
	x = 0;
	y = cls.glconfig.vidHeight;
	re.SetColor( g_color_table[0] );
	re.DrawStretchPic( x, y - cl_graphheight->integer,
					   w, cl_graphheight->integer, 0, 0, 0, 0, cls.whiteShader );
	re.SetColor( NULL );

	for ( a = 0 ; a < w ; a++ )
	{
		i = ( current - 1 - a + 1024 ) & 1023;
		v = values[i].value;
		color = values[i].color;
		v = v * cl_graphscale->integer + cl_graphshift->integer;

		if ( v < 0 ) {
			v += cl_graphheight->integer * ( 1 + (int)( -v / cl_graphheight->integer ) );
		}
		h = (int)v % cl_graphheight->integer;
		re.DrawStretchPic( x + w - 1 - a, y - h, 1, h, 0, 0, 0, 0, cls.whiteShader );
	}
}

//=============================================================================

/*
==================
SCR_Init
==================
*/
void SCR_Init( void ) {
	cl_timegraph = Cvar_Get( "timegraph", "0", CVAR_CHEAT );
	cl_debuggraph = Cvar_Get( "debuggraph", "0", CVAR_CHEAT );
	cl_graphheight = Cvar_Get( "graphheight", "32", CVAR_CHEAT );
	cl_graphscale = Cvar_Get( "graphscale", "1", CVAR_CHEAT );
	cl_graphshift = Cvar_Get( "graphshift", "0", CVAR_CHEAT );

	scr_initialized = qtrue;
}


//=======================================================

/*
==================
SCR_DrawScreenField

This will be called twice if rendering in stereo mode
==================
*/
void SCR_DrawScreenField( stereoFrame_t stereoFrame ) {
	re.BeginFrame( stereoFrame );

	// wide aspect ratio screens need to have the sides cleared
	// unless they are displaying game renderings
	if ( cls.state != CA_ACTIVE ) {
		if ( cls.glconfig.vidWidth * 480 > cls.glconfig.vidHeight * 640 ) {
			re.SetColor( g_color_table[0] );
			re.DrawStretchPic( 0, 0, cls.glconfig.vidWidth, cls.glconfig.vidHeight, 0, 0, 0, 0, cls.whiteShader );
			re.SetColor( NULL );
		}
	}

	if ( !uivm ) {
		Com_DPrintf( "draw screen without UI loaded\n" );
		return;
	}

	// if the menu is going to cover the entire screen, we
	// don't need to render anything under it
	if ( !VM_Call( uivm, UI_IS_FULLSCREEN ) ) {
		switch ( cls.state ) {
		default:
			Com_Error( ERR_FATAL, "SCR_DrawScreenField: bad cls.state" );
			break;
		case CA_CINEMATIC:
			SCR_DrawCinematic();
			break;
		case CA_DISCONNECTED:
			// force menu up
			S_StopAllSounds();
			VM_Call( uivm, UI_SET_ACTIVE_MENU, UIMENU_MAIN );
			break;
		case CA_CONNECTING:
		case CA_CHALLENGING:
		case CA_CONNECTED:
			// connecting clients will only show the connection dialog
			// refresh to update the time
			VM_Call( uivm, UI_REFRESH, cls.realtime );
			VM_Call( uivm, UI_DRAW_CONNECT_SCREEN, qfalse );
			break;
//			// Ridah, if the cgame is valid, fall through to there
//			if (!cls.cgameStarted || !com_sv_running->integer) {
//				// connecting clients will only show the connection dialog
//				VM_Call( uivm, UI_DRAW_CONNECT_SCREEN, qfalse );
//				break;
//			}
		case CA_LOADING:
		case CA_PRIMED:
			// draw the game information screen and loading progress
			CL_CGameRendering( stereoFrame );

			// also draw the connection information, so it doesn't
			// flash away too briefly on local or lan games
			//if (!com_sv_running->value || Cvar_VariableIntegerValue("sv_cheats"))	// Ridah, don't draw useless text if not in dev mode
			VM_Call( uivm, UI_REFRESH, cls.realtime );
			VM_Call( uivm, UI_DRAW_CONNECT_SCREEN, qtrue );
			break;
		case CA_ACTIVE:
			CL_CGameRendering( stereoFrame );
			SCR_DrawDemoRecording();
			break;
		}
	}

	// the menu draws next
	if ( cls.keyCatchers & KEYCATCH_UI && uivm ) {
		VM_Call( uivm, UI_REFRESH, cls.realtime );
	}

	// console draws next
	Con_DrawConsole();

	// debug graph can be drawn on top of anything
	if ( cl_debuggraph->integer || cl_timegraph->integer || cl_debugMove->integer ) {
		SCR_DrawDebugGraph();
	}
}

/*
==================
SCR_UpdateScreen

This is called every frame, and can also be called explicitly to flush
text to the screen.
==================
*/
void SCR_UpdateScreen( void ) {
	static int recursive;

	if ( !scr_initialized ) {
		return;             // not initialized yet
	}

	if ( ++recursive > 2 ) {
		Com_Error( ERR_FATAL, "SCR_UpdateScreen: recursively called" );
	}
	recursive = 1;

	// if running in stereo, we need to draw the frame twice
	if ( cls.glconfig.stereoEnabled ) {
		SCR_DrawScreenField( STEREO_LEFT );
		SCR_DrawScreenField( STEREO_RIGHT );
	} else {
		SCR_DrawScreenField( STEREO_CENTER );
	}

	if ( com_speeds->integer ) {
		re.EndFrame( &time_frontend, &time_backend );
	} else {
		re.EndFrame( NULL, NULL );
	}

	recursive = 0;
}
