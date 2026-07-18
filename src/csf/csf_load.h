/*
===========================================================================
CSF (Compiled String File) loader for RTCW-SP.

Binary format is compatible with Command & Conquer: Generals CSF v3:

  Header (24 bytes):
    int32  id          'CSF ' (0x43534620)
    int32  version     3
    int32  num_labels
    int32  num_strings
    int32  skip        (unused)
    int32  langid

  For each label:
    int32  id          'LBL ' (0x4C424C20)
    int32  num_strings
    int32  label_len
    char   label[label_len]

    For each string in label:
      int32  id        'STR ' (0x53545220) or 'STRW'
      int32  text_len  (in WideChars)
      wchar  text[text_len]  (UTF-16LE, each wchar is bitwise inverted)
      [if id == 'STRW':
        int32  wave_len
        char   wave[wave_len]
      ]

The text is stored as UTF-8 internally. Lookup is hash-based.
===========================================================================
*/

#ifndef CSF_LOAD_H
#define CSF_LOAD_H

#include "../game/q_shared.h"

#if defined( __GNUC__ ) || defined( __clang__ )
#define CSF_API __attribute__( ( visibility( "hidden" ) ) )
#else
#define CSF_API
#endif

CSF_API qboolean CSF_Load( const char *filename );
CSF_API void CSF_Shutdown( void );
CSF_API const char *CSF_GetString( const char *label );
CSF_API qboolean CSF_IsLoaded( void );
CSF_API const char *CSF_GetLanguageCode( const char *raw );
CSF_API qboolean CSF_LoadForLanguage( const char *rawLang );

#endif // CSF_LOAD_H
