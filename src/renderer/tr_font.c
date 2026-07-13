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

// tr_font.c
//
// Font renderer based on stb_truetype. Glyphs are baked into a single
// atlas using stbtt_PackFontRanges() with oversampling for higher quality
// small fonts. Supports UTF-8 via fixed Unicode ranges (ASCII, Latin-1,
// Cyrillic).

#include "tr_local.h"
#include "../qcommon/qcommon.h"

#include <math.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "../stb/stb_truetype.h"

#define MAX_FONTS 6
#define FONT_ATLAS_SIZE 512
#define FONT_ATLAS_MAX_SIZE 2048
static int registeredFontCount = 0;
static fontInfo_t registeredFont[MAX_FONTS];

// Unicode ranges baked into every font atlas.
static const fontRange_t s_fontRanges[] = {
	{ FONT_RANGE_ASCII_FIRST,    FONT_RANGE_ASCII_COUNT,    0 },
	{ FONT_RANGE_LATIN1_FIRST,   FONT_RANGE_LATIN1_COUNT,   FONT_RANGE_ASCII_COUNT },
	{ FONT_RANGE_CYRILLIC_FIRST, FONT_RANGE_CYRILLIC_COUNT, FONT_RANGE_ASCII_COUNT + FONT_RANGE_LATIN1_COUNT }
};
#define NUM_FONT_RANGES ( sizeof( s_fontRanges ) / sizeof( s_fontRanges[0] ) )

void WriteTGA( char *filename, byte *data, int width, int height ) {
	byte    *buffer;
	int i, c;

	buffer = Z_Malloc( width * height * 4 + 18 );
	Com_Memset( buffer, 0, 18 );
	buffer[2] = 2;      // uncompressed type
	buffer[12] = width & 255;
	buffer[13] = width >> 8;
	buffer[14] = height & 255;
	buffer[15] = height >> 8;
	buffer[16] = 32;    // pixel size

	// swap rgb to bgr
	c = 18 + width * height * 4;
	for ( i = 18 ; i < c ; i += 4 )
	{
		buffer[i] = data[i - 18 + 2];       // blue
		buffer[i + 1] = data[i - 18 + 1];     // green
		buffer[i + 2] = data[i - 18 + 0];     // red
		buffer[i + 3] = data[i - 18 + 3];     // alpha
	}

	ri.FS_WriteFile( filename, buffer, c );

	Z_Free( buffer );
}

void RE_RegisterFont( const char *fontName, int pointSize, fontInfo_t *font ) {
	stbtt_fontinfo fontInfo;
	stbtt_pack_context pc;
	stbtt_packedchar packedChars[GLYPHS_PER_FONT];
	int i, j, len;
	int spaceGlyphIndex;
	int fontQuality, fontOversample;
	int atlasSize;
	unsigned char *atlas = NULL;
	unsigned char *imageBuff = NULL;
	image_t *image;
	qhandle_t h;
	char name[MAX_QPATH];
	char shaderName[MAX_QPATH];
	void *faceData;
	float glyphScale;
	float fontBakeSize;
	int packResult;

	if ( pointSize <= 0 ) {
		pointSize = 12;
	}

	R_SyncRenderThread();

	if ( registeredFontCount >= MAX_FONTS ) {
		ri.Printf( PRINT_ALL, "RE_RegisterFont: Too many fonts registered already.\n" );
		return;
	}

	fontQuality = r_fontQuality->integer;
	if ( fontQuality < 1 ) {
		fontQuality = 1;
	} else if ( fontQuality > 4 ) {
		fontQuality = 4;
	}

	fontOversample = r_fontOversample->integer;
	if ( fontOversample < 1 ) {
		fontOversample = 1;
	} else if ( fontOversample > 4 ) {
		fontOversample = 4;
	}

	Com_sprintf( name, sizeof( name ), "fonts/fontImage_%i_%i_%i", pointSize, fontQuality, fontOversample );

	for ( i = 0; i < registeredFontCount; i++ ) {
		if ( Q_stricmp( name, registeredFont[i].name ) == 0 ) {
			memcpy( font, &registeredFont[i], sizeof( fontInfo_t ) );
			return;
		}
	}

	len = ri.FS_ReadFile( fontName, &faceData );
	if ( len <= 0 ) {
		ri.Printf( PRINT_ALL, "RE_RegisterFont: Unable to read font file '%s'\n", fontName );
		return;
	}

	if ( !stbtt_InitFont( &fontInfo, (const unsigned char *)faceData, 0 ) ) {
		ri.Printf( PRINT_ALL, "RE_RegisterFont: stb_truetype, unable to initialize font '%s'.\n", fontName );
		ri.FS_FreeFile( faceData );
		return;
	}

	fontBakeSize = (float)( pointSize * fontQuality );

	Com_Memset( packedChars, 0, sizeof( packedChars ) );
	// Index of the space glyph in the ASCII range; used as fallback metrics
	// for characters missing from the font (e.g. Cyrillic in a Latin-only TTF).
	spaceGlyphIndex = ' ' - FONT_RANGE_ASCII_FIRST;

	atlas = Z_Malloc( FONT_ATLAS_MAX_SIZE * FONT_ATLAS_MAX_SIZE );
	if ( atlas == NULL ) {
		ri.Printf( PRINT_ALL, "RE_RegisterFont: Z_Malloc failure during atlas creation.\n" );
		ri.FS_FreeFile( faceData );
		return;
	}

	// Try increasing atlas sizes until packing succeeds.
	packResult = 0;
	for ( atlasSize = FONT_ATLAS_SIZE; atlasSize <= FONT_ATLAS_MAX_SIZE; atlasSize *= 2 ) {
		Com_Memset( atlas, 0, atlasSize * atlasSize );
		if ( !stbtt_PackBegin( &pc, atlas, atlasSize, atlasSize, 0, 1, NULL ) ) {
			ri.Printf( PRINT_ALL, "RE_RegisterFont: stbtt_PackBegin failed for %dx%d atlas\n", atlasSize, atlasSize );
			continue;
		}
		stbtt_PackSetOversampling( &pc, fontOversample, fontOversample );
		packResult = 1;
		for ( i = 0; i < NUM_FONT_RANGES; i++ ) {
			if ( !stbtt_PackFontRange( &pc, (const unsigned char *)faceData, 0,
			                            -fontBakeSize, s_fontRanges[i].firstCodepoint,
			                            s_fontRanges[i].numChars,
			                            &packedChars[s_fontRanges[i].glyphBaseIndex] ) ) {
				packResult = 0;
				ri.Printf( PRINT_ALL, "RE_RegisterFont: pack range %d (cp %d, %d chars) failed\n",
				           i, s_fontRanges[i].firstCodepoint, s_fontRanges[i].numChars );
				break;
			}
		}
		stbtt_PackEnd( &pc );
		if ( packResult ) {
			break;
		}
	}

	if ( !packResult ) {
		ri.Printf( PRINT_ALL, "RE_RegisterFont: failed to pack font '%s' size %d\n", fontName, pointSize );
		Z_Free( atlas );
		ri.FS_FreeFile( faceData );
		return;
	}

	// Convert 8-bit atlas into RGBA for the renderer.
	imageBuff = Z_Malloc( atlasSize * atlasSize * 4 );
	if ( imageBuff == NULL ) {
		ri.Printf( PRINT_ALL, "RE_RegisterFont: Z_Malloc failure during image buffer creation.\n" );
		Z_Free( atlas );
		ri.FS_FreeFile( faceData );
		return;
	}

	for ( i = 0; i < atlasSize * atlasSize; i++ ) {
		float alpha;
		imageBuff[i * 4 + 0] = 255;
		imageBuff[i * 4 + 1] = 255;
		imageBuff[i * 4 + 2] = 255;
		alpha = (float)atlas[i] / 255.0f;
		if ( alpha < 0.0f ) {
			alpha = 0.0f;
		} else if ( alpha > 1.0f ) {
			alpha = 1.0f;
		}
		// apply gamma correction so antialiasing looks smoother
		alpha = pow( alpha, 1.0f / 2.2f );
		imageBuff[i * 4 + 3] = (unsigned char)( alpha * 255.0f );
	}

	Com_sprintf( shaderName, sizeof( shaderName ), "fonts/fontImage_%i_%i_%i",
	             pointSize, fontQuality, fontOversample );

	if ( r_saveFontData->integer ) {
		char tgaName[MAX_QPATH];
		Com_sprintf( tgaName, sizeof( tgaName ), "%s.tga", shaderName );
		WriteTGA( tgaName, imageBuff, atlasSize, atlasSize );
	}

	image = R_CreateImage( shaderName, imageBuff, atlasSize, atlasSize, qfalse, qfalse, GL_CLAMP );
	h = RE_RegisterShaderFromImage( shaderName, LIGHTMAP_2D, image, qfalse );

	// Convert stbtt_packedchar metrics to glyphInfo_t.
	// The bitmap was baked with oversampling, so the atlas pixel size of the
	// glyph is larger than its logical screen size. Divide image dimensions
	// by fontOversample so the on-screen size matches xadvance/yoff metrics.
	for ( i = 0; i < NUM_FONT_RANGES; i++ ) {
		const fontRange_t *range = &s_fontRanges[i];
		for ( j = 0; j < range->numChars; j++ ) {
			stbtt_packedchar *pcinfo = &packedChars[range->glyphBaseIndex + j];
			glyphInfo_t *glyph = &font->glyphs[range->glyphBaseIndex + j];
			int gw_atlas = pcinfo->x1 - pcinfo->x0;
			int gh_atlas = pcinfo->y1 - pcinfo->y0;
			int gw = ( gw_atlas + fontOversample / 2 ) / fontOversample;
			int gh = ( gh_atlas + fontOversample / 2 ) / fontOversample;

			Com_Memset( glyph, 0, sizeof( glyphInfo_t ) );

			int xSkip;

			glyph->height      = gh;
			glyph->pitch       = gw;
			glyph->top         = (int)( -pcinfo->yoff + 0.5f );
			glyph->bottom      = (int)( -pcinfo->yoff2 + 0.5f );
			xSkip              = (int)( pcinfo->xadvance + 0.5f ) + 1;
			if ( xSkip < gw + 2 ) {
				xSkip = gw + 2; // guarantee a small gap between glyphs
			}
			glyph->xSkip       = xSkip;
			glyph->imageWidth  = gw;
			glyph->imageHeight = gh;
			glyph->s           = (float)pcinfo->x0 / (float)atlasSize;
			glyph->t           = (float)pcinfo->y0 / (float)atlasSize;
			glyph->s2          = (float)pcinfo->x1 / (float)atlasSize;
			glyph->t2          = (float)pcinfo->y1 / (float)atlasSize;
			glyph->glyph       = h;
			Q_strncpyz( glyph->shaderName, shaderName, sizeof( glyph->shaderName ) );
		}
	}

	// Store the range table for R_GetGlyph lookups.
	font->numRanges = NUM_FONT_RANGES;
	for ( i = 0; i < NUM_FONT_RANGES; i++ ) {
		font->ranges[i] = s_fontRanges[i];
	}

	// 48pt is UI scale 1.0. Divide by the quality multiplier because the
	// baked glyphs are rendered at pointSize * fontQuality pixels.
	// Oversampling only improves anti-aliasing without changing glyph metrics.
	glyphScale = 48.0f / (float)pointSize / (float)fontQuality;
	font->glyphScale = glyphScale;

	// Apply fallback metrics for any glyph that was not rendered by stbtt
	// (missing glyph in the TTF). Use the space glyph as a safe fallback.
	if ( font->glyphs[spaceGlyphIndex].xSkip > 0 ) {
		for ( i = 0; i < GLYPHS_PER_FONT; i++ ) {
			if ( font->glyphs[i].xSkip <= 0 || font->glyphs[i].xSkip == INT_MIN ) {
				font->glyphs[i].xSkip = font->glyphs[spaceGlyphIndex].xSkip;
			}
		}
	}
	Q_strncpyz( font->name, name, sizeof( font->name ) );

	memcpy( &registeredFont[registeredFontCount++], font, sizeof( fontInfo_t ) );

	Z_Free( imageBuff );
	Z_Free( atlas );
	ri.FS_FreeFile( faceData );
}

void R_InitFreeType() {
	registeredFontCount = 0;
}

void R_DoneFreeType() {
	registeredFontCount = 0;
}
