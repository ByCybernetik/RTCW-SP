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
** SDL2 Main Implementation
** 
** Main entry point and system functions using SDL2.
** Properly handles x64 architecture with correct pointer types.
*/

#include <SDL2/SDL.h>
#include <stdint.h>  // Для intptr_t, uintptr_t - важно для x64!
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>

#include "../game/q_shared.h"
#include "../qcommon/qcommon.h"
#include "sdl_local.h"

// Глобальное состояние
sdlState_t sdl_state = {
    NULL,   // window
    NULL,   // glcontext
    qfalse, // initialized
    qfalse, // fullscreen
    640,    // width
    480     // height
};

// Для совместимости со старым кодом
glwstate_t glw_state;

/*
=================
Sys_Milliseconds - Получение текущего времени в миллисекундах
=================
*/
int Sys_Milliseconds( void ) {
    static uint64_t base_time = 0;
    uint64_t current_time;
    
    // Используем SDL_GetTicks64 для x64 совместимости
    current_time = SDL_GetTicks64();
    
    if ( base_time == 0 ) {
        base_time = current_time;
        return 0;
    }
    
    return (int)( current_time - base_time );
}

/*
=================
Sys_Sleep - Задержка на указанное время
=================
*/
void Sys_Sleep( int msec ) {
    SDL_Delay( msec );
}

/*
=================
Sys_Init - Инициализация системных функций
=================
*/
void Sys_Init( void ) {
    Com_Printf( "SDL2 system initialization\n" );
    
    // Инициализируем базовые подсистемы SDL2
    if ( SDL_Init( SDL_INIT_TIMER ) < 0 ) {
        Com_Printf( S_COLOR_RED "SDL_Init timer failed: %s\n", SDL_GetError() );
        // Не фатально, продолжаем
    }
    
    sdl_state.initialized = qtrue;
    
    Com_Printf( "SDL2 version: %d.%d.%d\n", 
                SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_PATCHLEVEL );
}

/*
=================
Sys_Quit - Завершение работы приложения
=================
*/
void Sys_Quit( void ) {
    Com_Printf( "Shutting down SDL2 subsystems...\n" );
    
    // Завершаем работу с OpenGL
    GLimp_Shutdown();
    
    // Завершаем аудиоподсистему
    SNDDMA_Shutdown();
    
    // Завершаем систему ввода
    IN_Shutdown();
    
    // Завершаем все подсистемы SDL2
    SDL_Quit();
    
    sdl_state.initialized = qfalse;
    
    Com_Printf( "SDL2 shutdown complete\n" );
}

/*
=================
Sys_Error - Обработка критической ошибки
=================
*/
void Sys_Error( const char *error, ... ) {
    va_list argptr;
    char text[1024];
    
    va_start( argptr, error );
    vsprintf( text, error, argptr );
    va_end( argptr );
    
    // Показываем сообщение об ошибке через SDL
    SDL_ShowSimpleMessageBox( SDL_MESSAGEBOX_ERROR, 
                              "RTCW Error", 
                              text, 
                              sdl_state.window );
    
    // Выводим в консоль
    fprintf( stderr, "SDL2 Error: %s\n", text );
    
    // Завершаем работу
    SDL_Quit();
    exit( 1 );
}

/*
=================
Sys_UnloadGame - Выгрузка игровой DLL
=================
*/
void Sys_UnloadGame( void ) {
    // Реализация зависит от системы загрузки модулей
    // В SDL2 можно использовать SDL_UnloadObject
}

/*
=================
Sys_GetGameAPI - Загрузка игровой DLL
=================
*/
void *Sys_GetGameAPI( void *parms ) {
    // Реализация зависит от системы загрузки модулей
    // В SDL2 можно использовать SDL_LoadObject
    return NULL;
}

/*
=================
main - Точка входа приложения
=================
*/
#if defined(__linux__) || defined(MAIN_FUNCTION)
int main( int argc, char *argv[] ) {
    // Инициализируем SDL2
    if ( SDL_Init( SDL_INIT_EVERYTHING ) < 0 ) {
        fprintf( stderr, "SDL_Init failed: %s\n", SDL_GetError() );
        return 1;
    }
    
    // Устанавливаем обработку ошибок
    SDL_SetError( "SDL2 initialization error" );
    
    Com_Printf( "====================================\n" );
    Com_Printf( "Return to Castle Wolfenstein\n" );
    Com_Printf( "SDL2 x64 Build\n" );
    Com_Printf( "====================================\n" );
    
    // Инициализируем движок
    Com_Init( argc, argv );
    
    // Основной цикл игры
    while ( 1 ) {
        Com_Frame();
    }
    
    // Эта строка никогда не будет достигнута
    SDL_Quit();
    
    return 0;
}
#endif
