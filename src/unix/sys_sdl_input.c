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

#include "q_shared.h"
#include "../qcommon/qcommon.h"
#include "../ui/keycodes.h"
#include "client.h"
#include "tr_public.h"

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
	
	// Initialize all keys to 0 (unused)
	for ( i = 0; i < SDL_NUM_SCANCODES; i++ ) {
		sdl_keymap[i] = 0;
	}
	
	// Alpha keys (use ASCII values for lowercase letters)
	sdl_keymap[SDL_SCANCODE_A] = 'a';
	sdl_keymap[SDL_SCANCODE_B] = 'b';
	sdl_keymap[SDL_SCANCODE_C] = 'c';
	sdl_keymap[SDL_SCANCODE_D] = 'd';
	sdl_keymap[SDL_SCANCODE_E] = 'e';
	sdl_keymap[SDL_SCANCODE_F] = 'f';
	sdl_keymap[SDL_SCANCODE_G] = 'g';
	sdl_keymap[SDL_SCANCODE_H] = 'h';
	sdl_keymap[SDL_SCANCODE_I] = 'i';
	sdl_keymap[SDL_SCANCODE_J] = 'j';
	sdl_keymap[SDL_SCANCODE_K] = 'k';
	sdl_keymap[SDL_SCANCODE_L] = 'l';
	sdl_keymap[SDL_SCANCODE_M] = 'm';
	sdl_keymap[SDL_SCANCODE_N] = 'n';
	sdl_keymap[SDL_SCANCODE_O] = 'o';
	sdl_keymap[SDL_SCANCODE_P] = 'p';
	sdl_keymap[SDL_SCANCODE_Q] = 'q';
	sdl_keymap[SDL_SCANCODE_R] = 'r';
	sdl_keymap[SDL_SCANCODE_S] = 's';
	sdl_keymap[SDL_SCANCODE_T] = 't';
	sdl_keymap[SDL_SCANCODE_U] = 'u';
	sdl_keymap[SDL_SCANCODE_V] = 'v';
	sdl_keymap[SDL_SCANCODE_W] = 'w';
	sdl_keymap[SDL_SCANCODE_X] = 'x';
	sdl_keymap[SDL_SCANCODE_Y] = 'y';
	sdl_keymap[SDL_SCANCODE_Z] = 'z';
	
	// Number keys (use ASCII values)
	sdl_keymap[SDL_SCANCODE_0] = '0';
	sdl_keymap[SDL_SCANCODE_1] = '1';
	sdl_keymap[SDL_SCANCODE_2] = '2';
	sdl_keymap[SDL_SCANCODE_3] = '3';
	sdl_keymap[SDL_SCANCODE_4] = '4';
	sdl_keymap[SDL_SCANCODE_5] = '5';
	sdl_keymap[SDL_SCANCODE_6] = '6';
	sdl_keymap[SDL_SCANCODE_7] = '7';
	sdl_keymap[SDL_SCANCODE_8] = '8';
	sdl_keymap[SDL_SCANCODE_9] = '9';
	
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
	
	sdl_keymap[SDL_SCANCODE_LCTRL] = K_CTRL;
	sdl_keymap[SDL_SCANCODE_RCTRL] = K_CTRL;
	sdl_keymap[SDL_SCANCODE_LSHIFT] = K_SHIFT;
	sdl_keymap[SDL_SCANCODE_RSHIFT] = K_SHIFT;
	sdl_keymap[SDL_SCANCODE_LALT] = K_ALT;
	sdl_keymap[SDL_SCANCODE_RALT] = K_ALT;
	
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
	
	// Punctuation - use ASCII values
	sdl_keymap[SDL_SCANCODE_MINUS] = '-';
	sdl_keymap[SDL_SCANCODE_EQUALS] = '=';
	sdl_keymap[SDL_SCANCODE_LEFTBRACKET] = '[';
	sdl_keymap[SDL_SCANCODE_RIGHTBRACKET] = ']';
	sdl_keymap[SDL_SCANCODE_BACKSLASH] = '\\';
	sdl_keymap[SDL_SCANCODE_SEMICOLON] = ';';
	sdl_keymap[SDL_SCANCODE_APOSTROPHE] = '\'';
	sdl_keymap[SDL_SCANCODE_COMMA] = ',';
	sdl_keymap[SDL_SCANCODE_PERIOD] = '.';
	sdl_keymap[SDL_SCANCODE_SLASH] = '/';
	sdl_keymap[SDL_SCANCODE_GRAVE] = '`';
	
	// Keypad - use ASCII values for numbers, special codes for others
	sdl_keymap[SDL_SCANCODE_KP_0] = '0';
	sdl_keymap[SDL_SCANCODE_KP_1] = '1';
	sdl_keymap[SDL_SCANCODE_KP_2] = '2';
	sdl_keymap[SDL_SCANCODE_KP_3] = '3';
	sdl_keymap[SDL_SCANCODE_KP_4] = '4';
	sdl_keymap[SDL_SCANCODE_KP_5] = '5';
	sdl_keymap[SDL_SCANCODE_KP_6] = '6';
	sdl_keymap[SDL_SCANCODE_KP_7] = '7';
	sdl_keymap[SDL_SCANCODE_KP_8] = '8';
	sdl_keymap[SDL_SCANCODE_KP_9] = '9';
	sdl_keymap[SDL_SCANCODE_KP_PERIOD] = '.';
	sdl_keymap[SDL_SCANCODE_KP_DIVIDE] = '/';
	sdl_keymap[SDL_SCANCODE_KP_MULTIPLY] = '*';
	sdl_keymap[SDL_SCANCODE_KP_MINUS] = '-';
	sdl_keymap[SDL_SCANCODE_KP_PLUS] = '+';
	sdl_keymap[SDL_SCANCODE_KP_ENTER] = K_ENTER;
	sdl_keymap[SDL_SCANCODE_KP_EQUALS] = '=';
	
	// Special keys not in keycodes.h - skip or map to 0
	// sdl_keymap[SDL_SCANCODE_HELP] = 0;
	// sdl_keymap[SDL_SCANCODE_PRINTSCREEN] = 0;
	// sdl_keymap[SDL_SCANCODE_SCROLLLOCK] = 0;
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
