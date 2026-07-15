/*
===========================================================================
CSF (Compiled String File) loader for RTCW-SP.

Reads a Generals-style CSF v3 file and exposes label -> UTF-8 string lookup.
===========================================================================
*/

#include "csf_load.h"

#include <stdlib.h>
#include <string.h>

// File system access is provided by the hosting VM (UI or cgame).
extern int trap_FS_FOpenFile( const char *qpath, fileHandle_t *f, fsMode_t mode );
extern int trap_FS_Read( void *buffer, int len, fileHandle_t f );
extern void trap_FS_FCloseFile( fileHandle_t f );
extern void Com_Printf( const char *fmt, ... );

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

	while ( ( c = *s++ ) != 0 ) {
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
		} else {
			if ( j + 3 >= outSize ) {
				break;
			}
			out[j++] = 0xE0 | ( c >> 12 );
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
CSF_Load

Loads a CSF file and builds the lookup table.
Returns qtrue on success.
===============
*/
qboolean CSF_Load( const char *filename ) {
	fileHandle_t f;
	int len, i, j;
	byte *buf, *bufEnd;
	const byte *p;
	int id, version, numLabels;

	CSF_Shutdown();

	len = trap_FS_FOpenFile( filename, &f, FS_READ );
	if ( len <= 0 ) {
		return qfalse;
	}

	buf = (byte *)malloc( len );
	if ( !buf ) {
		trap_FS_FCloseFile( f );
		return qfalse;
	}

	trap_FS_Read( buf, len, f );
	trap_FS_FCloseFile( f );

	if ( len < 24 ) {
		free( buf );
		return qfalse;
	}

	p = buf;
	bufEnd = buf + len;

	id = CSF_ReadInt( &p );
	version = CSF_ReadInt( &p );
	numLabels = CSF_ReadInt( &p );
	// Skip numStrings, skip, langId — not needed for lookup.
	p += 3 * sizeof( int );

	if ( id != CSF_ID || version != 3 || numLabels <= 0 || numLabels > 1000000 ) {
		free( buf );
		return qfalse;
	}

	for ( i = 0; i < numLabels; i++ ) {
		int labelId, labelNumStrings, labelLen;
		char label[256];
		csfEntry_t *entry = NULL;

		if ( p + 12 > bufEnd ) {
			break;
		}

		labelId = CSF_ReadInt( &p );
		labelNumStrings = CSF_ReadInt( &p );
		labelLen = CSF_ReadInt( &p );

		if ( labelId != CSF_LABEL ) {
			break;
		}
		if ( labelLen <= 0 || labelLen >= sizeof( label ) ) {
			break;
		}
		if ( p + labelLen > bufEnd ) {
			break;
		}

		memcpy( label, p, labelLen );
		label[labelLen] = '\0';
		p += labelLen;

		for ( j = 0; j < labelNumStrings; j++ ) {
			int strId, textLen;

			if ( p + 8 > bufEnd ) {
				break;
			}

			strId = CSF_ReadInt( &p );
			textLen = CSF_ReadInt( &p );

			if ( strId != CSF_STRING && strId != CSF_STRINGWITHWAVE ) {
				break;
			}
			if ( textLen < 0 || textLen > 100000 ) {
				break;
			}
			if ( p + textLen * sizeof( unsigned short ) > bufEnd ) {
				break;
			}

			{
				unsigned short *wbuf = (unsigned short *)malloc( textLen * sizeof( unsigned short ) );
				char *utf8 = (char *)malloc( textLen * 3 + 1 );
				if ( !wbuf || !utf8 ) {
					free( wbuf );
					free( utf8 );
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
						CSF_AddEntry( entry );
					}
				}

				free( wbuf );
				free( utf8 );
			}

			if ( strId == CSF_STRINGWITHWAVE ) {
				int waveLen;

				if ( p + 4 > bufEnd ) {
					break;
				}
				waveLen = CSF_ReadInt( &p );
				if ( waveLen < 0 || waveLen >= 1024 ) {
					break;
				}
				if ( p + waveLen > bufEnd ) {
					break;
				}
				p += waveLen;
			}
		}
	}

	free( buf );

	csfLoaded = qtrue;
	return qtrue;
}

/*
===============
CSF_Shutdown
===============
*/
void CSF_Shutdown( void ) {
	int i;

	for ( i = 0; i < CSF_HASH_SIZE; i++ ) {
		csfEntry_t *e = csfHash[i];
		while ( e ) {
			csfEntry_t *next = e->next;
			free( e->label );
			free( e->text );
			free( e );
			e = next;
		}
		csfHash[i] = NULL;
	}

	csfLoaded = qfalse;
}

/*
===============
CSF_GetString

Returns the UTF-8 translation for a label, or NULL if not found.
===============
*/
const char *CSF_GetString( const char *label ) {
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
qboolean CSF_IsLoaded( void ) {
	return csfLoaded;
}
