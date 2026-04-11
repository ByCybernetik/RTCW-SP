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

/*
** SDL2 OpenGL Implementation
** 
** Modern replacement for linux_glimp.c using SDL2 for window management
** and OpenGL context creation. Supports x64 architecture properly.
*/

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <GL/gl.h>
#include <stdint.h>  // Для intptr_t, uintptr_t

#include "../renderer/tr_local.h"
#include "../client/client.h"
#include "sdl_local.h"

// Глобальное состояние SDL
static SDL_Window *sdl_window = NULL;
static SDL_GLContext sdl_glcontext = NULL;
static qboolean sdl_initialized = qfalse;
static qboolean sdl_fullscreen = qfalse;
static int sdl_vidWidth = 640;
static int sdl_vidHeight = 480;

// Для совместимости со старым кодом
glwstate_t glw_state;

typedef enum
{
    RSERR_OK,
    RSERR_INVALID_FULLSCREEN,
    RSERR_INVALID_MODE,
    RSERR_UNKNOWN
} rserr_t;

/*
=================
GLimp_Shutdown
=================
*/
void GLimp_Shutdown( void ) {
    if ( !sdl_initialized ) {
        return;
    }

    // Уничтожаем OpenGL контекст
    if ( sdl_glcontext ) {
        SDL_GL_DeleteContext( sdl_glcontext );
        sdl_glcontext = NULL;
    }

    // Уничтожаем окно
    if ( sdl_window ) {
        SDL_DestroyWindow( sdl_window );
        sdl_window = NULL;
    }

    sdl_initialized = qfalse;
    
    Com_Printf( "SDL2 OpenGL shutdown successful\n" );
}

/*
=================
GLimp_Init
=================
*/
qboolean GLimp_Init( void ) {
    if ( sdl_initialized ) {
        return qtrue;
    }

    // Инициализируем видеоподсистему SDL2
    if ( SDL_InitSubSystem( SDL_INIT_VIDEO ) < 0 ) {
        Com_Printf( S_COLOR_RED "SDL_InitVideo failed: %s\n", SDL_GetError() );
        return qfalse;
    }

    // Устанавливаем атрибуты OpenGL
    SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 8 );
    SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 8 );
    SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 8 );
    SDL_GL_SetAttribute( SDL_GL_ALPHA_SIZE, 8 );
    SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, 24 );
    SDL_GL_SetAttribute( SDL_GL_STENCIL_SIZE, 8 );
    SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
    
    // Совместимость с OpenGL 1.x (как в оригинальном RTCW)
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 1 );
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 5 );
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY );

    sdl_initialized = qtrue;
    
    Com_Printf( "SDL2 OpenGL initialized successfully\n" );
    
    return qtrue;
}

/*
=================
GLimp_SetMode
=================
*/
rserr_t GLimp_SetMode( int *pwidth, int *pheight, int mode, qboolean fullscreen ) {
    Uint32 window_flags = 0;
    int width, height;
    
    Com_Printf( "------ GLimp_SetMode ------\n" );
    
    // Получаем размеры из r_mode
    if ( !R_GetModeInfo( &width, &height, mode ) ) {
        Com_Printf( "Invalid mode\n" );
        return RSERR_INVALID_MODE;
    }
    
    *pwidth = width;
    *pheight = height;
    
    Com_Printf( "Requested size: %d x %d [%s]\n", 
                width, height, fullscreen ? "fullscreen" : "windowed" );
    
    // Флаги окна
    window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
    
    if ( fullscreen ) {
        window_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
        sdl_fullscreen = qtrue;
    } else {
        sdl_fullscreen = qfalse;
    }
    
    // Если окно уже существует, уничтожаем его
    if ( sdl_window ) {
        SDL_DestroyWindow( sdl_window );
        sdl_window = NULL;
    }
    
    // Создаем окно
    sdl_window = SDL_CreateWindow( "Return to Castle Wolfenstein",
                                   SDL_WINDOWPOS_CENTERED,
                                   SDL_WINDOWPOS_CENTERED,
                                   width, height,
                                   window_flags );
    
    if ( !sdl_window ) {
        Com_Printf( S_COLOR_RED "SDL_CreateWindow failed: %s\n", SDL_GetError() );
        return RSERR_INVALID_MODE;
    }
    
    // Создаем OpenGL контекст
    sdl_glcontext = SDL_GL_CreateContext( sdl_window );
    
    if ( !sdl_glcontext ) {
        Com_Printf( S_COLOR_RED "SDL_GL_CreateContext failed: %s\n", SDL_GetError() );
        SDL_DestroyWindow( sdl_window );
        sdl_window = NULL;
        return RSERR_INVALID_MODE;
    }
    
    // Включаем VSync если нужно
    SDL_GL_SetSwapInterval( r_swapinterval->integer );
    
    sdl_vidWidth = width;
    sdl_vidHeight = height;
    
    Com_Printf( "SDL2 window created: %dx%d\n", width, height );
    
    return RSERR_OK;
}

/*
=================
GLimp_EndFrame
=================
*/
void GLimp_EndFrame( void ) {
    if ( !sdl_initialized || !sdl_window ) {
        return;
    }
    
    // Меняем буферы
    SDL_GL_SwapWindow( sdl_window );
}

/*
=================
GLimp_SwitchFullscreen
=================
*/
qboolean GLimp_SwitchFullscreen( int width, int height ) {
    if ( !sdl_window ) {
        return qfalse;
    }
    
    if ( sdl_fullscreen ) {
        SDL_SetWindowFullscreen( sdl_window, 0 );
        SDL_SetWindowSize( sdl_window, width, height );
        SDL_SetWindowPosition( sdl_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED );
        sdl_fullscreen = qfalse;
    } else {
        SDL_SetWindowFullscreen( sdl_window, SDL_WINDOW_FULLSCREEN_DESKTOP );
        sdl_fullscreen = qtrue;
    }
    
    sdl_vidWidth = width;
    sdl_vidHeight = height;
    
    return qtrue;
}

/*
=================
GLimp_SetGamma
=================
*/
void GLimp_SetGamma( unsigned char red[256], unsigned char green[256], unsigned char blue[256] ) {
    // SDL2 не имеет прямой поддержки гаммы через таблицу lookup
    // Используем встроенную функцию гаммы SDL
    SDL_Window *window = sdl_window;
    
    if ( !window ) {
        return;
    }
    
    // Создаем таблицы гаммы
    Uint16 redTable[256];
    Uint16 greenTable[256];
    Uint16 blueTable[256];
    
    for ( int i = 0; i < 256; i++ ) {
        redTable[i] = red[i] << 8;
        greenTable[i] = green[i] << 8;
        blueTable[i] = blue[i] << 8;
    }
    
    // Применяем гамму
    SDL_SetWindowGammaRamp( window, redTable, greenTable, blueTable );
}

/*
=================
GLimp_GetCurrentVideoMode
=================
*/
void GLimp_GetCurrentVideoMode( int *width, int *height ) {
    if ( sdl_window && width && height ) {
        SDL_GetWindowSize( sdl_window, width, height );
    } else if ( width && height ) {
        *width = sdl_vidWidth;
        *height = sdl_vidHeight;
    }
}

/*
=================
GLimp_LogComment
=================
*/
void GLimp_LogComment( char *comment ) {
    // Логирование можно реализовать при необходимости
    // Сейчас просто игнорируем
}

/*
=================
IN_Init - Инициализация ввода через SDL2
=================
*/
void IN_Init( void ) {
    // Инициализируем подсистему ввода SDL2
    if ( SDL_InitSubSystem( SDL_INIT_EVENTS | SDL_INIT_JOYSTICK ) < 0 ) {
        Com_Printf( S_COLOR_YELLOW "SDL_Init input subsystem failed: %s\n", SDL_GetError() );
        // Не фатально, продолжаем работу
    }
    
    Com_Printf( "SDL2 input initialized\n" );
}

/*
=================
IN_Frame - Обработка событий ввода
=================
*/
void IN_Frame( void ) {
    SDL_Event event;
    
    // Обрабатываем события SDL
    while ( SDL_PollEvent( &event ) ) {
        switch ( event.type ) {
            case SDL_QUIT:
                Cbuf_ExecuteText( EXEC_NOW, "quit\n" );
                break;
                
            case SDL_KEYDOWN:
            case SDL_KEYUP:
                // Обработка клавиатуры будет в отдельной функции
                break;
                
            case SDL_MOUSEMOTION:
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
                // Обработка мыши будет в отдельной функции
                break;
                
            default:
                break;
        }
    }
}

/*
=================
IN_Shutdown - Завершение работы системы ввода
=================
*/
void IN_Shutdown( void ) {
    SDL_QuitSubSystem( SDL_INIT_EVENTS | SDL_INIT_JOYSTICK );
    Com_Printf( "SDL2 input shutdown\n" );
}

/*
=================
Sys_Init - Инициализация системных функций
=================
*/
void Sys_Init( void ) {
    // Базовая инициализация системы
    Com_Printf( "SDL2 system initialized\n" );
}

/*
=================
Sys_Quit - Завершение работы приложения
=================
*/
void Sys_Quit( void ) {
    GLimp_Shutdown();
    SDL_Quit();
}

/*
=================
main - Точка входа приложения
=================
*/
#ifdef MAIN_FUNCTION
int main( int argc, char *argv[] ) {
    // Инициализируем SDL2
    if ( SDL_Init( SDL_INIT_EVERYTHING ) < 0 ) {
        fprintf( stderr, "SDL_Init failed: %s\n", SDL_GetError() );
        return 1;
    }
    
    // Запускаем движок
    Com_Init( argc, argv );
    
    // Основной цикл
    while ( 1 ) {
        Com_Frame();
    }
    
    // Завершение работы
    SDL_Quit();
    
    return 0;
}
#endif
