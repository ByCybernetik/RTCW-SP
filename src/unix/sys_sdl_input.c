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

// sys_sdl_input.c - SDL2-based input system for RTCW SP

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include "../renderer/tr_public.h"

#include <SDL2/SDL.h>

// External functions from sys_sdl_main.c
extern SDL_Window *Sys_GetWindowHandle( void );

// Key mapping table (SDL scancode to RTCW keynum)
static int sdl_keymap[SDL_NUM_SCANCODES];
static qboolean keydown[256];

// Mouse state
static int mouse_x = 0;
static int mouse_y = 0;
static int mouse_z = 0;

/*
=================
IN_InitKeymap
=================
*/
static void IN_InitKeymap( void ) {
	int i;
	
	// Initialize all keys to KEY_NONE
	for ( i = 0; i < SDL_NUM_SCANCODES; i++ ) {
		sdl_keymap[i] = K_NONE;
	}
	
	// Alpha keys
	sdl_keymap[SDL_SCANCODE_A] = K_A;
	sdl_keymap[SDL_SCANCODE_B] = K_B;
	sdl_keymap[SDL_SCANCODE_C] = K_C;
	sdl_keymap[SDL_SCANCODE_D] = K_D;
	sdl_keymap[SDL_SCANCODE_E] = K_E;
	sdl_keymap[SDL_SCANCODE_F] = K_F;
	sdl_keymap[SDL_SCANCODE_G] = K_G;
	sdl_keymap[SDL_SCANCODE_H] = K_H;
	sdl_keymap[SDL_SCANCODE_I] = K_I;
	sdl_keymap[SDL_SCANCODE_J] = K_J;
	sdl_keymap[SDL_SCANCODE_K] = K_K;
	sdl_keymap[SDL_SCANCODE_L] = K_L;
	sdl_keymap[SDL_SCANCODE_M] = K_M;
	sdl_keymap[SDL_SCANCODE_N] = K_N;
	sdl_keymap[SDL_SCANCODE_O] = K_O;
	sdl_keymap[SDL_SCANCODE_P] = K_P;
	sdl_keymap[SDL_SCANCODE_Q] = K_Q;
	sdl_keymap[SDL_SCANCODE_R] = K_R;
	sdl_keymap[SDL_SCANCODE_S] = K_S;
	sdl_keymap[SDL_SCANCODE_T] = K_T;
	sdl_keymap[SDL_SCANCODE_U] = K_U;
	sdl_keymap[SDL_SCANCODE_V] = K_V;
	sdl_keymap[SDL_SCANCODE_W] = K_W;
	sdl_keymap[SDL_SCANCODE_X] = K_X;
	sdl_keymap[SDL_SCANCODE_Y] = K_Y;
	sdl_keymap[SDL_SCANCODE_Z] = K_Z;
	
	// Number keys
	sdl_keymap[SDL_SCANCODE_0] = K_0;
	sdl_keymap[SDL_SCANCODE_1] = K_1;
	sdl_keymap[SDL_SCANCODE_2] = K_2;
	sdl_keymap[SDL_SCANCODE_3] = K_3;
	sdl_keymap[SDL_SCANCODE_4] = K_4;
	sdl_keymap[SDL_SCANCODE_5] = K_5;
	sdl_keymap[SDL_SCANCODE_6] = K_6;
	sdl_keymap[SDL_SCANCODE_7] = K_7;
	sdl_keymap[SDL_SCANCODE_8] = K_8;
	sdl_keymap[SDL_SCANCODE_9] = K_9;
	
	// Function keys
	sdl_keymap[SDL_SCANCODE_F1] = K_F1;
	sdl_keymap[SDL_SCANCODE_F2] = K_F2;
	sdl_keymap[SDL_SCANCODE_F3] = K_F3;
	sdl_keymap[SDL_SCANCODE_F4] = K_F4;
	sdl_keymap[SDL_SCANCODE_F5] = K_F5;
	sdl_keymap[SDL_SCANCODE_F6] = K_F6;
	sdl_keymap[SDL_SCANCODE_F7] = K_F7;
	sdl_keymap[SDL_SCANCODE_F8] = K_F8;
	sdl_keymap[SDL_SCANCODE_F9] = K_F9;
	sdl_keymap[SDL_SCANCODE_F10] = K_F10;
	sdl_keymap[SDL_SCANCODE_F11] = K_F11;
	sdl_keymap[SDL_SCANCODE_F12] = K_F12;
	sdl_keymap[SDL_SCANCODE_F13] = K_F13;
	sdl_keymap[SDL_SCANCODE_F14] = K_F14;
	sdl_keymap[SDL_SCANCODE_F15] = K_F15;
	
	// Special keys
	sdl_keymap[SDL_SCANCODE_ESCAPE] = K_ESCAPE;
	sdl_keymap[SDL_SCANCODE_RETURN] = K_ENTER;
	sdl_keymap[SDL_SCANCODE_BACKSPACE] = K_BACKSPACE;
	sdl_keymap[SDL_SCANCODE_TAB] = K_TAB;
	sdl_keymap[SDL_SCANCODE_SPACE] = K_SPACE;
	sdl_keymap[SDL_SCANCODE_CAPSLOCK] = K_CAPSLOCK;
	
	sdl_keymap[SDL_SCANCODE_LEFTCTRL] = K_LCTRL;
	sdl_keymap[SDL_SCANCODE_RIGHTCTRL] = K_RCTRL;
	sdl_keymap[SDL_SCANCODE_LEFTSHIFT] = K_LSHIFT;
	sdl_keymap[SDL_SCANCODE_RIGHTSHIFT] = K_RSHIFT;
	sdl_keymap[SDL_SCANCODE_LALT] = K_LALT;
	sdl_keymap[SDL_SCANCODE_RALT] = K_RALT;
	
	sdl_keymap[SDL_SCANCODE_UP] = K_UPARROW;
	sdl_keymap[SDL_SCANCODE_DOWN] = K_DOWNARROW;
	sdl_keymap[SDL_SCANCODE_LEFT] = K_LEFTARROW;
	sdl_keymap[SDL_SCANCODE_RIGHT] = K_RIGHTARROW;
	
	sdl_keymap[SDL_SCANCODE_INSERT] = K_INS;
	sdl_keymap[SDL_SCANCODE_DELETE] = K_DEL;
	sdl_keymap[SDL_SCANCODE_HOME] = K_HOME;
	sdl_keymap[SDL_SCANCODE_END] = K_END;
	sdl_keymap[SDL_SCANCODE_PAGEUP] = K_PGUP;
	sdl_keymap[SDL_SCANCODE_PAGEDOWN] = K_PGDN;
	
	sdl_keymap[SDL_SCANCODE_MINUS] = K_MINUS;
	sdl_keymap[SDL_SCANCODE_EQUALS] = K_EQUALS;
	sdl_keymap[SDL_SCANCODE_LEFTBRACKET] = K_LEFTBRACKET;
	sdl_keymap[SDL_SCANCODE_RIGHTBRACKET] = K_RIGHTBRACKET;
	sdl_keymap[SDL_SCANCODE_BACKSLASH] = K_BACKSLASH;
	sdl_keymap[SDL_SCANCODE_SEMICOLON] = K_SEMICOLON;
	sdl_keymap[SDL_SCANCODE_APOSTROPHE] = K_APOSTROPHE;
	sdl_keymap[SDL_SCANCODE_COMMA] = K_COMMA;
	sdl_keymap[SDL_SCANCODE_PERIOD] = K_PERIOD;
	sdl_keymap[SDL_SCANCODE_SLASH] = K_SLASH;
	sdl_keymap[SDL_SCANCODE_GRAVE] = K_GRAVE;
	
	sdl_keymap[SDL_SCANCODE_KP_0] = K_KP_0;
	sdl_keymap[SDL_SCANCODE_KP_1] = K_KP_1;
	sdl_keymap[SDL_SCANCODE_KP_2] = K_KP_2;
	sdl_keymap[SDL_SCANCODE_KP_3] = K_KP_3;
	sdl_keymap[SDL_SCANCODE_KP_4] = K_KP_4;
	sdl_keymap[SDL_SCANCODE_KP_5] = K_KP_5;
	sdl_keymap[SDL_SCANCODE_KP_6] = K_KP_6;
	sdl_keymap[SDL_SCANCODE_KP_7] = K_KP_7;
	sdl_keymap[SDL_SCANCODE_KP_8] = K_KP_8;
	sdl_keymap[SDL_SCANCODE_KP_9] = K_KP_9;
	sdl_keymap[SDL_SCANCODE_KP_PERIOD] = K_KP_PERIOD;
	sdl_keymap[SDL_SCANCODE_KP_DIVIDE] = K_KP_DIVIDE;
	sdl_keymap[SDL_SCANCODE_KP_MULTIPLY] = K_KP_MULTIPLY;
	sdl_keymap[SDL_SCANCODE_KP_MINUS] = K_KP_MINUS;
	sdl_keymap[SDL_SCANCODE_KP_PLUS] = K_KP_PLUS;
	sdl_keymap[SDL_SCANCODE_KP_ENTER] = K_KP_ENTER;
	sdl_keymap[SDL_SCANCODE_KP_EQUALS] = K_KP_EQUALS;
	
	sdl_keymap[SDL_SCANCODE_HELP] = K_HELP;
	sdl_keymap[SDL_SCANCODE_PRINTSCREEN] = K_PRINT_SCREEN;
	sdl_keymap[SDL_SCANCODE_SCROLLLOCK] = K_SCROLLOCK;
	sdl_keymap[SDL_SCANCODE_PAUSE] = K_PAUSE;
}

/*
=================
IN_Init
=================
*/
void IN_Init( void ) {
	Com_Printf( "SDL Input initialized\n" );
	
	IN_InitKeymap();
	
	// Reset mouse state
	mouse_x = 0;
	mouse_y = 0;
	mouse_z = 0;
	
	// Clear key states
	Com_Memset( keydown, 0, sizeof( keydown ) );
	
	// Enable text input for console
	SDL_StartTextInput();
}

/*
=================
IN_Shutdown
=================
*/
void IN_Shutdown( void ) {
	SDL_StopTextInput();
	Com_Printf( "SDL Input shutdown\n" );
}

/*
=================
IN_Frame
=================
*/
void IN_Frame( void ) {
	SDL_Event event;
	
	// Process all pending events
	while ( SDL_PollEvent( &event ) ) {
		switch ( event.type ) {
			case SDL_KEYDOWN:
				if ( event.key.keysym.scancode < SDL_NUM_SCANCODES ) {
					int keynum = sdl_keymap[event.key.keysym.scancode];
					if ( keynum != K_NONE && !keydown[keynum] ) {
						keydown[keynum] = qtrue;
						Key_Event( keynum, qtrue );
					}
				}
				break;
				
			case SDL_KEYUP:
				if ( event.key.keysym.scancode < SDL_NUM_SCANCODES ) {
					int keynum = sdl_keymap[event.key.keysym.scancode];
					if ( keynum != K_NONE ) {
						keydown[keynum] = qfalse;
						Key_Event( keynum, qfalse );
					}
				}
				break;
				
			case SDL_MOUSEMOTION:
				if ( cls.state == CA_ACTIVE && cl_paused->integer == 0 ) {
					mouse_x += event.motion.xrel;
					mouse_y += event.motion.yrel;
				}
				break;
				
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
				{
					int keynum = K_MOUSE1 + ( event.button.button - 1 );
					qboolean down = ( event.button.state == SDL_PRESSED );
					Key_Event( keynum, down );
				}
				break;
				
			case SDL_MOUSEWHEEL:
				if ( event.wheel.y > 0 ) {
					Key_Event( K_MWHEELUP, qtrue );
					Key_Event( K_MWHEELUP, qfalse );
				} else if ( event.wheel.y < 0 ) {
					Key_Event( K_MWHEELDOWN, qtrue );
					Key_Event( K_MWHEELDOWN, qfalse );
				}
				break;
				
			case SDL_TEXTINPUT:
				// Send character input to console/chat
				{
					const char *text = event.text.text;
					while ( *text ) {
						int ch = (unsigned char)*text++;
						if ( ch >= 32 && ch != 127 ) {
							Key_Event( ch, qtrue );
							Key_Event( ch, qfalse );
						}
					}
				}
				break;
				
			case SDL_QUIT:
				Cbuf_AddText( "quit\n" );
				break;
				
			default:
				break;
		}
	}
	
	// Accumulate mouse movement
	if ( m_filter->integer ) {
		mouse_x = ( mouse_x + mx_accum ) / 2;
		mouse_y = ( mouse_y + my_accum ) / 2;
	} else {
		mouse_x = mx_accum;
		mouse_y = my_accum;
	}
	
	mx_accum = 0;
	my_accum = 0;
}

/*
=================
IN_MouseEvent
=================
*/
void IN_MouseEvent( int mstate ) {
	// Handled in IN_Frame via SDL events
}

/*
=================
IN_Move
=================
*/
void IN_Move( usercmd_t *cmd ) {
	static int old_mouse_x = 0;
	static int old_mouse_y = 0;
	
	if ( m_filter->integer ) {
		mouse_x = ( mouse_x + old_mouse_x ) / 2;
		mouse_y = ( mouse_y + old_mouse_y ) / 2;
	}
	
	old_mouse_x = mouse_x;
	old_mouse_y = mouse_y;
	
	// Add mouse movement to command
	cmd->angles[YAW] -= mouse_x * m_yaw->value;
	cmd->angles[PITCH] += mouse_y * m_pitch->value;
	
	// Reset mouse delta
	mouse_x = 0;
	mouse_y = 0;
}

/*
=================
IN_IsGamepadAvailable
=================
*/
qboolean IN_IsGamepadAvailable( void ) {
	return SDL_NumJoysticks() > 0 ? qtrue : qfalse;
}

/*
=================
IN_GamepadAxis
=================
*/
int IN_GamepadAxis( int device, int axis ) {
	// TODO: Implement gamepad support
	return 0;
}

/*
=================
IN_GamepadButton
=================
*/
int IN_GamepadButton( int device, int button ) {
	// TODO: Implement gamepad support
	return 0;
}
