/*
===========================================================================
CSF (Compiled String File) loader for RTCW-SP.

Reads a Generals-style CSF v3 file and exposes label -> UTF-8 string lookup.
===========================================================================
*/

#include "csf_load.h"

#include <stdlib.h>
#include <string.h>

// File system access is provided by the hosting VM (UI or cgame) or by the
// engine/client directly.
#if defined( RTCW_CLIENT )
int FS_FOpenFileByMode( const char *qpath, fileHandle_t *f, fsMode_t mode );
int FS_Read( void *buffer, int len, fileHandle_t f );
void FS_FCloseFile( fileHandle_t f );
#define trap_FS_FOpenFile FS_FOpenFileByMode
#define trap_FS_Read FS_Read
#define trap_FS_FCloseFile FS_FCloseFile
#else
extern int trap_FS_FOpenFile( const char *qpath, fileHandle_t *f, fsMode_t mode );
extern int trap_FS_Read( void *buffer, int len, fileHandle_t f );
extern void trap_FS_FCloseFile( fileHandle_t f );
#endif

#if defined( RTCW_CLIENT )
#define CSF_Printf Com_Printf
#else
extern void Com_Printf( const char *fmt, ... );
#define CSF_Printf Com_Printf
#endif

#define CSF_ID             0x43534620  // "CSF "
#define CSF_LABEL          0x4C424C20  // "LBL "
#define CSF_STRING         0x53545220  // "STR "
#define CSF_STRINGWITHWAVE 0x53545257  // "STRW"

#define CSF_HASH_SIZE      1024

typedef struct csfEntry_s {
	char *label;
	char *text;
	struct csfEntry_s *next;
} csfEntry_t;

static csfEntry_t *csfHash[CSF_HASH_SIZE];
static qboolean csfLoaded = qfalse;

/*
===============
CSF_HashLabel
===============
*/
static unsigned int CSF_HashLabel( const char *s ) {
	unsigned int hash = 5381;
	int c;

	while ( ( c = (unsigned char)*s++ ) != 0 ) {
		// Hash must be case-insensitive because lookup compares with Q_stricmp.
		if ( c >= 'A' && c <= 'Z' ) {
			c += 'a' - 'A';
		}
		hash = ( ( hash << 5 ) + hash ) + c;
	}

	return hash % CSF_HASH_SIZE;
}

/*
===============
CSF_CopyString
===============
*/
static char *CSF_CopyString( const char *s ) {
	char *out;
	int len;

	len = strlen( s ) + 1;
	out = (char *)malloc( len );
	if ( out ) {
		memcpy( out, s, len );
	}
	return out;
}

/*
===============
CSF_ReadInt

Reads a little-endian 32-bit int from the buffer.
===============
*/
static int CSF_ReadInt( const byte **buf ) {
	int v;
	memcpy( &v, *buf, sizeof( v ) );
	*buf += sizeof( v );
	return v;
}

/*
===============
CSF_UTF16LEToUTF8

De-inverts each UTF-16LE code unit and encodes it as UTF-8.
Returns number of bytes written (excluding null terminator).
===============
*/
static int CSF_UTF16LEToUTF8( const unsigned short *in, int inLen, char *out, int outSize ) {
	int i, j;

	if ( outSize <= 0 ) {
		return 0;
	}

	for ( i = 0, j = 0; i < inLen && j < outSize - 1; i++ ) {
		unsigned int c = ( ~in[i] ) & 0xFFFF; // CSF stores inverted wide chars

		// Combine UTF-16 surrogate pairs into a single code point.
		if ( c >= 0xD800 && c <= 0xDBFF && i + 1 < inLen ) {
			unsigned int lo = ( ~in[i + 1] ) & 0xFFFF;
			if ( lo >= 0xDC00 && lo <= 0xDFFF ) {
				c = 0x10000 + ( ( c - 0xD800 ) << 10 ) + ( lo - 0xDC00 );
				i++;
			}
		}

		if ( c < 0x80 ) {
			if ( j + 1 >= outSize ) {
				break;
			}
			out[j++] = (char)c;
		} else if ( c < 0x800 ) {
			if ( j + 2 >= outSize ) {
				break;
			}
			out[j++] = 0xC0 | ( c >> 6 );
			out[j++] = 0x80 | ( c & 0x3F );
		} else if ( c < 0x10000 ) {
			if ( j + 3 >= outSize ) {
				break;
			}
			out[j++] = 0xE0 | ( c >> 12 );
			out[j++] = 0x80 | ( ( c >> 6 ) & 0x3F );
			out[j++] = 0x80 | ( c & 0x3F );
		} else {
			if ( j + 4 >= outSize ) {
				break;
			}
			out[j++] = 0xF0 | ( c >> 18 );
			out[j++] = 0x80 | ( ( c >> 12 ) & 0x3F );
			out[j++] = 0x80 | ( ( c >> 6 ) & 0x3F );
			out[j++] = 0x80 | ( c & 0x3F );
		}
	}

	out[j] = '\0';
	return j;
}

/*
===============
CSF_AddEntry
===============
*/
static void CSF_AddEntry( csfEntry_t *entry ) {
	unsigned int h = CSF_HashLabel( entry->label );
	entry->next = csfHash[h];
	csfHash[h] = entry;
}

/*
===============
CSF_GetLanguageCode

Normalizes a raw language identifier to a short code used for CSF file names.
Supports old numeric cl_language values and a few common names.
===============
*/
CSF_API const char *CSF_GetLanguageCode( const char *raw ) {
	if ( !raw || !raw[0] ) {
		return "us";
	}

	if ( !strcmp( raw, "0" ) || !Q_stricmp( raw, "us" ) || !Q_stricmp( raw, "american" ) || !Q_stricmp( raw, "english" ) ) {
		return "us";
	}
	if ( !strcmp( raw, "1" ) || !Q_stricmp( raw, "fe" ) || !Q_stricmp( raw, "french" ) || !Q_stricmp( raw, "fr" ) ) {
		return "fe";
	}
	if ( !strcmp( raw, "2" ) || !Q_stricmp( raw, "ge" ) || !Q_stricmp( raw, "german" ) || !Q_stricmp( raw, "de" ) ) {
		return "ge";
	}
	if ( !strcmp( raw, "3" ) || !Q_stricmp( raw, "it" ) || !Q_stricmp( raw, "italian" ) ) {
		return "it";
	}
	if ( !strcmp( raw, "4" ) || !Q_stricmp( raw, "sp" ) || !Q_stricmp( raw, "spanish" ) || !Q_stricmp( raw, "es" ) ) {
		return "sp";
	}
	if ( !Q_stricmp( raw, "ru" ) || !Q_stricmp( raw, "russian" ) ) {
		return "ru";
	}

	return raw;
}

/*
===============
CSF_LoadForLanguage

Loads text/rtcw_<code>.csf if available, otherwise falls back to text/rtcw.csf.
===============
*/
CSF_API qboolean CSF_LoadForLanguage( const char *rawLang ) {
	char filename[MAX_QPATH];
	const char *lang;

	lang = CSF_GetLanguageCode( rawLang );
	CSF_Printf( "CSF_LoadForLanguage: raw='%s' code='%s'\n", rawLang ? rawLang : "(null)", lang );

	Com_sprintf( filename, sizeof( filename ), "text/rtcw_%s.csf", lang );
	CSF_Printf( "CSF_LoadForLanguage: trying '%s'\n", filename );
	if ( CSF_Load( filename ) ) {
		return qtrue;
	}

	CSF_Printf( "CSF_LoadForLanguage: falling back to 'text/rtcw.csf'\n" );
	return CSF_Load( "text/rtcw.csf" );
}

/*
===============
CSF_Load

Loads a CSF file and builds the lookup table.
Returns qtrue on success.
===============
*/
CSF_API qboolean CSF_Load( const char *filename ) {
	fileHandle_t f;
	int len, i, j;
	int parsedLabels;
	int loadedLabels;
	int numStrings, skip, langId;
	const char *errorReason = NULL;
	byte *buf, *bufEnd;
	const byte *p;
	int id, version, numLabels;

	CSF_Shutdown();
	parsedLabels = 0;
	loadedLabels = 0;

	CSF_Printf( "CSF_Load: module hash=%p loading '%s'...\n", (void *)csfHash, filename );

	len = trap_FS_FOpenFile( filename, &f, FS_READ );
	if ( len <= 0 ) {
		CSF_Printf( "CSF_Load: '%s' open failed (len=%d)\n", filename, len );
		return qfalse;
	}

	buf = (byte *)malloc( len );
	if ( !buf ) {
		trap_FS_FCloseFile( f );
		CSF_Printf( "CSF_Load: '%s' malloc(%d) failed\n", filename, len );
		return qfalse;
	}

	trap_FS_Read( buf, len, f );
	trap_FS_FCloseFile( f );

	if ( len < 24 ) {
		free( buf );
		CSF_Printf( "CSF_Load: '%s' too short (%d bytes)\n", filename, len );
		return qfalse;
	}

	p = buf;
	bufEnd = buf + len;

	id = CSF_ReadInt( &p );
	version = CSF_ReadInt( &p );
	numLabels = CSF_ReadInt( &p );
	numStrings = CSF_ReadInt( &p );
	skip = CSF_ReadInt( &p );
	langId = CSF_ReadInt( &p );

	CSF_Printf( "CSF_Load: '%s' header id=0x%X version=%d numLabels=%d numStrings=%d skip=%d langId=%d\n",
	            filename, id, version, numLabels, numStrings, skip, langId );

	if ( id != CSF_ID || version != 3 || numLabels <= 0 || numLabels > 1000000 ) {
		free( buf );
		CSF_Printf( "CSF_Load: '%s' bad header (id=0x%X version=%d numLabels=%d)\n",
		            filename, id, version, numLabels );
		return qfalse;
	}

	for ( i = 0; i < numLabels; i++ ) {
		int labelId, labelNumStrings, labelLen;
		char label[256];
		csfEntry_t *entry = NULL;

		if ( p + 12 > bufEnd ) {
			errorReason = "truncated label header";
			break;
		}

		labelId = CSF_ReadInt( &p );
		labelNumStrings = CSF_ReadInt( &p );
		labelLen = CSF_ReadInt( &p );

		if ( labelId != CSF_LABEL ) {
			errorReason = va( "bad labelId 0x%X at label %d", labelId, i );
			break;
		}
		if ( labelNumStrings < 0 || labelNumStrings > 100 ) {
			errorReason = va( "bad labelNumStrings %d at label %d", labelNumStrings, i );
			break;
		}
		if ( labelLen <= 0 || labelLen >= sizeof( label ) ) {
			errorReason = va( "bad labelLen %d at label %d", labelLen, i );
			break;
		}
		if ( p + labelLen > bufEnd ) {
			errorReason = va( "label name truncated at label %d", i );
			break;
		}

		memcpy( label, p, labelLen );
		label[labelLen] = '\0';
		p += labelLen;

		for ( j = 0; j < labelNumStrings; j++ ) {
			int strId, textLen;

			if ( p + 8 > bufEnd ) {
				errorReason = va( "truncated string header at label '%s' string %d", label, j );
				break;
			}

			strId = CSF_ReadInt( &p );
			textLen = CSF_ReadInt( &p );

			if ( strId != CSF_STRING && strId != CSF_STRINGWITHWAVE ) {
				errorReason = va( "bad strId 0x%X at label '%s' string %d", strId, label, j );
				break;
			}
			if ( textLen < 0 || textLen > 100000 ) {
				errorReason = va( "bad textLen %d at label '%s' string %d", textLen, label, j );
				break;
			}
			if ( p + textLen * sizeof( unsigned short ) > bufEnd ) {
				errorReason = va( "string text truncated at label '%s' string %d", label, j );
				break;
			}

			{
				unsigned short *wbuf = (unsigned short *)malloc( textLen * sizeof( unsigned short ) );
				char *utf8 = (char *)malloc( textLen * 3 + 1 );
				if ( !wbuf || !utf8 ) {
					free( wbuf );
					free( utf8 );
					errorReason = va( "OOM at label '%s' string %d", label, j );
					break;
				}

				memcpy( wbuf, p, textLen * sizeof( unsigned short ) );
				p += textLen * sizeof( unsigned short );

				CSF_UTF16LEToUTF8( wbuf, textLen, utf8, textLen * 3 + 1 );

				// Only use the first string for a label.
				if ( j == 0 ) {
					entry = (csfEntry_t *)malloc( sizeof( *entry ) );
					if ( entry ) {
						entry->label = CSF_CopyString( label );
						entry->text = CSF_CopyString( utf8 );
						entry->next = NULL;
						if ( !entry->label || !entry->text ) {
							free( entry->label );
							free( entry->text );
							free( entry );
							entry = NULL;
							errorReason = va( "OOM copying label '%s'", label );
						} else {
							CSF_AddEntry( entry );
							loadedLabels++;
						}
					} else {
						errorReason = va( "OOM allocating entry for label '%s'", label );
					}
				}

				free( wbuf );
				free( utf8 );
			}

			if ( strId == CSF_STRINGWITHWAVE ) {
				int waveLen;

				if ( p + 4 > bufEnd ) {
					errorReason = va( "truncated waveLen at label '%s'", label );
					break;
				}
				waveLen = CSF_ReadInt( &p );
				if ( waveLen < 0 || waveLen >= 1024 ) {
					errorReason = va( "bad waveLen %d at label '%s'", waveLen, label );
					break;
				}
				if ( p + waveLen > bufEnd ) {
					errorReason = va( "wave text truncated at label '%s'", label );
					break;
				}
				p += waveLen;
			}
		}

		if ( j < labelNumStrings ) {
			if ( !errorReason ) {
				errorReason = va( "string loop aborted at label '%s'", label );
			}
			break;
		}
		parsedLabels++;
	}

	free( buf );

	if ( parsedLabels < numLabels ) {
		// Partial table is worse than none: let callers fall back to rtcw.csf.
		CSF_Printf( "CSF_Load: '%s' ABORTED after label %d/%d (%s). Table cleared.\n",
		            filename, parsedLabels, numLabels, errorReason ? errorReason : "unknown" );
		CSF_Shutdown();
		return qfalse;
	}

	csfLoaded = qtrue;
	CSF_Printf( "CSF_Load: '%s' SUCCESS: %d labels parsed, %d entries loaded\n",
	            filename, parsedLabels, loadedLabels );
	return qtrue;
}

/*
===============
CSF_Shutdown
===============
*/
CSF_API void CSF_Shutdown( void ) {
	int i;
	int count = 0;

	CSF_Printf( "CSF_Shutdown: module hash=%p loaded=%d\n", (void *)csfHash, csfLoaded );
	for ( i = 0; i < CSF_HASH_SIZE; i++ ) {
		csfEntry_t *e = csfHash[i];
		while ( e ) {
			csfEntry_t *next = e->next;
			free( e->label );
			free( e->text );
			free( e );
			e = next;
			count++;
		}
		csfHash[i] = NULL;
	}

	csfLoaded = qfalse;
	CSF_Printf( "CSF_Shutdown: freed %d entries\n", count );
}

/*
===============
CSF_GetString

Returns the UTF-8 translation for a label, or NULL if not found.
===============
*/
CSF_API const char *CSF_GetString( const char *label ) {
	unsigned int h;
	csfEntry_t *e;

	if ( !csfLoaded || !label || !label[0] ) {
		return NULL;
	}

	h = CSF_HashLabel( label );
	for ( e = csfHash[h]; e; e = e->next ) {
		if ( !Q_stricmp( e->label, label ) ) {
			return e->text;
		}
	}

	return NULL;
}

/*
===============
CSF_IsLoaded
===============
*/
CSF_API qboolean CSF_IsLoaded( void ) {
	return csfLoaded;
}
