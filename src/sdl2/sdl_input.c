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
** SDL2 Input Implementation
** 
** Modern replacement for linux_joystick.c and input handling
** using SDL2 for keyboard, mouse and joystick input.
** Properly handles x64 pointer sizes.
*/

#include <SDL2/SDL.h>
#include <stdint.h>  // Для intptr_t, uintptr_t - важно для x64!

#include "../game/q_shared.h"
#include "../client/client.h"
#include "sdl_local.h"

// Состояние мыши
static int oldButtonState = 0;
static int mouseX = 0, mouseY = 0;
static qboolean mouseActive = qfalse;

// Таблица соответствия клавиш SDL2 и RTCW
static const int sdl2rtcw_keys[SDL_NUM_SCANCODES] = {
    [SDL_SCANCODE_ESCAPE] = K_ESCAPE,
    [SDL_SCANCODE_1] = K_1,
    [SDL_SCANCODE_2] = K_2,
    [SDL_SCANCODE_3] = K_3,
    [SDL_SCANCODE_4] = K_4,
    [SDL_SCANCODE_5] = K_5,
    [SDL_SCANCODE_6] = K_6,
    [SDL_SCANCODE_7] = K_7,
    [SDL_SCANCODE_8] = K_8,
    [SDL_SCANCODE_9] = K_9,
    [SDL_SCANCODE_0] = K_0,
    [SDL_SCANCODE_BACKSPACE] = K_BACKSPACE,
    [SDL_SCANCODE_TAB] = K_TAB,
    [SDL_SCANCODE_Q] = K_Q,
    [SDL_SCANCODE_W] = K_W,
    [SDL_SCANCODE_E] = K_E,
    [SDL_SCANCODE_R] = K_R,
    [SDL_SCANCODE_T] = K_T,
    [SDL_SCANCODE_Y] = K_Y,
    [SDL_SCANCODE_U] = K_U,
    [SDL_SCANCODE_I] = K_I,
    [SDL_SCANCODE_O] = K_O,
    [SDL_SCANCODE_P] = K_P,
    [SDL_SCANCODE_RETURN] = K_ENTER,
    [SDL_SCANCODE_LCTRL] = K_CTRL,
    [SDL_SCANCODE_RCTRL] = K_CTRL,
    [SDL_SCANCODE_A] = K_A,
    [SDL_SCANCODE_S] = K_S,
    [SDL_SCANCODE_D] = K_D,
    [SDL_SCANCODE_F] = K_F,
    [SDL_SCANCODE_G] = K_G,
    [SDL_SCANCODE_H] = K_H,
    [SDL_SCANCODE_J] = K_J,
    [SDL_SCANCODE_K] = K_K,
    [SDL_SCANCODE_L] = K_L,
    [SDL_SCANCODE_SEMICOLON] = K_SEMICOLON,
    [SDL_SCANCODE_APOSTROPHE] = K_QUOTE,
    [SDL_SCANCODE_GRAVE] = K_CONSOLE,
    [SDL_SCANCODE_LSHIFT] = K_SHIFT,
    [SDL_SCANCODE_RSHIFT] = K_SHIFT,
    [SDL_SCANCODE_BACKSLASH] = K_BACKSLASH,
    [SDL_SCANCODE_Z] = K_Z,
    [SDL_SCANCODE_X] = K_X,
    [SDL_SCANCODE_C] = K_C,
    [SDL_SCANCODE_V] = K_V,
    [SDL_SCANCODE_B] = K_B,
    [SDL_SCANCODE_N] = K_N,
    [SDL_SCANCODE_M] = K_M,
    [SDL_SCANCODE_COMMA] = K_COMMA,
    [SDL_SCANCODE_PERIOD] = K_PERIOD,
    [SDL_SCANCODE_SLASH] = K_SLASH,
    [SDL_SCANCODE_SPACE] = K_SPACE,
    [SDL_SCANCODE_LEFT] = K_LEFTARROW,
    [SDL_SCANCODE_RIGHT] = K_RIGHTARROW,
    [SDL_SCANCODE_UP] = K_UPARROW,
    [SDL_SCANCODE_DOWN] = K_DOWNARROW,
    [SDL_SCANCODE_LALT] = K_ALT,
    [SDL_SCANCODE_RALT] = K_ALT,
    [SDL_SCANCODE_HOME] = K_HOME,
    [SDL_SCANCODE_END] = K_END,
    [SDL_SCANCODE_PAGEUP] = K_PGUP,
    [SDL_SCANCODE_PAGEDOWN] = K_PGDN,
    [SDL_SCANCODE_INSERT] = K_INS,
    [SDL_SCANCODE_DELETE] = K_DEL,
    [SDL_SCANCODE_F1] = K_F1,
    [SDL_SCANCODE_F2] = K_F2,
    [SDL_SCANCODE_F3] = K_F3,
    [SDL_SCANCODE_F4] = K_F4,
    [SDL_SCANCODE_F5] = K_F5,
    [SDL_SCANCODE_F6] = K_F6,
    [SDL_SCANCODE_F7] = K_F7,
    [SDL_SCANCODE_F8] = K_F8,
    [SDL_SCANCODE_F9] = K_F9,
    [SDL_SCANCODE_F10] = K_F10,
    [SDL_SCANCODE_F11] = K_F11,
    [SDL_SCANCODE_F12] = K_F12,
    [SDL_SCANCODE_CAPSLOCK] = K_CAPSLOCK,
    [SDL_SCANCODE_KP_1] = K_KP_1,
    [SDL_SCANCODE_KP_2] = K_KP_2,
    [SDL_SCANCODE_KP_3] = K_KP_3,
    [SDL_SCANCODE_KP_4] = K_KP_4,
    [SDL_SCANCODE_KP_5] = K_KP_5,
    [SDL_SCANCODE_KP_6] = K_KP_6,
    [SDL_SCANCODE_KP_7] = K_KP_7,
    [SDL_SCANCODE_KP_8] = K_KP_8,
    [SDL_SCANCODE_KP_9] = K_KP_9,
    [SDL_SCANCODE_KP_0] = K_KP_0,
    [SDL_SCANCODE_KP_PERIOD] = K_KP_PERIOD,
    [SDL_SCANCODE_KP_ENTER] = K_KP_ENTER,
    [SDL_SCANCODE_KP_PLUS] = K_KP_PLUS,
    [SDL_SCANCODE_KP_MINUS] = K_KP_MINUS,
    [SDL_SCANCODE_KP_MULTIPLY] = K_KP_MULTIPLY,
    [SDL_SCANCODE_KP_DIVIDE] = K_KP_DIVIDE,
};

/*
=================
IN_MapKey - Преобразование кода клавиши SDL2 в код RTCW
=================
*/
void IN_MapKey( SDL_Keycode sym, int *key, int *down ) {
    SDL_Scancode scancode = SDL_GetScancodeFromKey( sym );
    
    if ( scancode < SDL_NUM_SCANCODES && sdl2rtcw_keys[scancode] != 0 ) {
        *key = sdl2rtcw_keys[scancode];
    } else {
        *key = 0;
    }
    
    *down = 1;
}

/*
=================
IN_MouseEvent - Обработка событий мыши
=================
*/
void IN_MouseEvent( int mstate ) {
    int i;
    
    // Относительное перемещение мыши
    if ( cl_mouseAccel->integer != 0 ) {
        // С ускорением
        int mx, my;
        SDL_GetRelativeMouseState( &mx, &my );
        
        mx *= cl_mouseAccel->integer;
        my *= cl_mouseAccel->integer;
        
        CL_MouseEvent( mx, my, 0 );
    } else {
        // Без ускорения
        int mx, my;
        SDL_GetRelativeMouseState( &mx, &my );
        CL_MouseEvent( mx, my, 0 );
    }
    
    // Кнопки мыши
    for ( i = 0; i < 3; i++ ) {
        if ( ( mstate & ( 1 << i ) ) && !( oldButtonState & ( 1 << i ) ) ) {
            CL_KeyEvent( K_MOUSE1 + i, qtrue, 0 );
        }
        
        if ( !( mstate & ( 1 << i ) ) && ( oldButtonState & ( 1 << i ) ) ) {
            CL_KeyEvent( K_MOUSE1 + i, qfalse, 0 );
        }
    }
    
    oldButtonState = mstate;
}

/*
=================
IN_Init - Инициализация системы ввода
=================
*/
void IN_Init( void ) {
    // Захват мыши
    if ( cl_mouseGrab->integer ) {
        SDL_SetRelativeMouseMode( SDL_TRUE );
        mouseActive = qtrue;
    }
    
    Com_Printf( "SDL2 input system initialized\n" );
}

/*
=================
IN_Frame - Обработка ввода каждый кадр
=================
*/
void IN_Frame( void ) {
    SDL_Event event;
    int key = 0, down = 0;
    
    // Обрабатываем события
    while ( SDL_PollEvent( &event ) ) {
        switch ( event.type ) {
            case SDL_KEYDOWN:
                IN_MapKey( event.key.keysym.sym, &key, &down );
                if ( key ) {
                    CL_KeyEvent( key, qtrue, 0 );
                }
                break;
                
            case SDL_KEYUP:
                IN_MapKey( event.key.keysym.sym, &key, &down );
                if ( key ) {
                    CL_KeyEvent( key, qfalse, 0 );
                }
                break;
                
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
                IN_MouseEvent( SDL_GetMouseState( NULL, NULL ) );
                break;
                
            case SDL_MOUSEMOTION:
                if ( mouseActive ) {
                    IN_MouseEvent( oldButtonState );
                }
                break;
                
            case SDL_WINDOWEVENT:
                if ( event.window.event == SDL_WINDOWEVENT_FOCUS_LOST ) {
                    // Потеря фокуса - отпускаем все клавиши
                    memset( cls.downkeys, 0, sizeof( cls.downkeys ) );
                }
                break;
                
            default:
                break;
        }
    }
    
    // Обновляем состояние захвата мыши
    if ( cl_mouseGrab->integer && !mouseActive ) {
        SDL_SetRelativeMouseMode( SDL_TRUE );
        mouseActive = qtrue;
    } else if ( !cl_mouseGrab->integer && mouseActive ) {
        SDL_SetRelativeMouseMode( SDL_FALSE );
        mouseActive = qfalse;
    }
}

/*
=================
IN_Shutdown - Завершение работы системы ввода
=================
*/
void IN_Shutdown( void ) {
    SDL_SetRelativeMouseMode( SDL_FALSE );
    mouseActive = qfalse;
    
    Com_Printf( "SDL2 input shutdown\n" );
}

/*
=================
Sys_SendKeyEvents - Отправка событий клавиатуры
=================
*/
void Sys_SendKeyEvents( void ) {
    IN_Frame();
}
