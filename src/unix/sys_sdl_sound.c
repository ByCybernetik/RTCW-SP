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

// sys_sdl_sound.c - SDL2/OpenAL-based sound system for RTCW SP

#include "q_shared.h"
#include "../qcommon/qcommon.h"
#include "snd_local.h"

#include <SDL2/SDL.h>
#include <AL/al.h>
#include <AL/alc.h>

static ALCdevice *alc_device = NULL;
static ALCcontext *alc_context = NULL;
static qboolean al_initialized = qfalse;

// Audio buffer for SDL callback
static short *audio_buffer = NULL;
static int audio_buffer_samples = 0;
static qboolean sdl_audio_initialized = qfalse;

/*
=================
SND_Init
=================
*/
void SND_Init( void ) {
	ALCenum alc_error;
	ALenum al_error;
	
	Com_Printf( "------- Sound Initialization -------\n" );
	
	// Initialize OpenAL context
	alc_device = alcOpenDevice( NULL );
	if ( !alc_device ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: Could not open OpenAL device\n" );
		goto fail;
	}
	
	alc_error = alcGetError( alc_device );
	if ( alc_error != ALC_NO_ERROR ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: OpenAL device error: %d\n", alc_error );
		alcCloseDevice( alc_device );
		alc_device = NULL;
		goto fail;
	}
	
	alc_context = alcCreateContext( alc_device, NULL );
	if ( !alc_context ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: Could not create OpenAL context\n" );
		alcCloseDevice( alc_device );
		alc_device = NULL;
		goto fail;
	}
	
	alcMakeContextCurrent( alc_context );
	
	al_error = alGetError();
	if ( al_error != AL_NO_ERROR ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: OpenAL context error: %d\n", al_error );
		alcDestroyContext( alc_context );
		alcCloseDevice( alc_device );
		alc_device = NULL;
		alc_context = NULL;
		goto fail;
	}
	
	// Set listener parameters
	alListener3f( AL_POSITION, 0.0f, 0.0f, 0.0f );
	alListener3f( AL_VELOCITY, 0.0f, 0.0f, 0.0f );
	// AL_ORIENTATION is a vector pair (forward, up) - use alListenerfv for 6 values
	{
		ALfloat orientation[6] = { 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f };
		alListenerfv( AL_ORIENTATION, orientation );
	}
	alListenerf( AL_GAIN, 1.0f );
	
	al_error = alGetError();
	if ( al_error != AL_NO_ERROR ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: OpenAL listener setup error: %d\n", al_error );
	}
	
	al_initialized = qtrue;
	
	Com_Printf( "OpenAL initialized successfully\n" );
	Com_Printf( "Vendor: %s\n", alGetString( AL_VENDOR ) );
	Com_Printf( "Renderer: %s\n", alGetString( AL_RENDERER ) );
	Com_Printf( "Version: %s\n", alGetString( AL_VERSION ) );
	Com_Printf( "Extensions: %s\n", alGetString( AL_EXTENSIONS ) );
	
	Com_Printf( "------------------------------------\n" );
	return;
	
fail:
	Com_Printf( "Sound initialization failed\n" );
	Com_Printf( "------------------------------------\n" );
}

/*
=================
SND_Shutdown
=================
*/
void SND_Shutdown( void ) {
	Com_Printf( "Shutting down sound system\n" );
	
	if ( alc_context ) {
		alcMakeContextCurrent( NULL );
		alcDestroyContext( alc_context );
		alc_context = NULL;
	}
	
	if ( alc_device ) {
		alcCloseDevice( alc_device );
		alc_device = NULL;
	}
	
	al_initialized = qfalse;
	
	if ( sdl_audio_initialized ) {
		SDL_CloseAudio();
		sdl_audio_initialized = qfalse;
	}
	
	if ( audio_buffer ) {
		SDL_free( audio_buffer );
		audio_buffer = NULL;
	}
}

/*
=================
SND_StartSound
=================
*/
void SND_StartSound( vec3_t origin, int entnum, int entchannel, sfxHandle_t sfx ) {
	if ( !al_initialized ) {
		return;
	}
	
	// TODO: Implement proper sound playback with sources
	// For now, just validate the system is working
}

/*
=================
SND_StartLocalSound
=================
*/
void SND_StartLocalSound( sfxHandle_t sfx ) {
	if ( !al_initialized ) {
		return;
	}
	
	// TODO: Implement local sound playback
}

/*
=================
SND_StopAllSounds
=================
*/
void SND_StopAllSounds( void ) {
	if ( !al_initialized ) {
		return;
	}
	
	// Stop all active sources - simple implementation
	// In a full implementation, we would track all created sources
	ALint source;
	alGenSources( 1, &source );
	if ( alGetError() == AL_NO_ERROR ) {
		alSourceStop( source );
		alDeleteSources( 1, &source );
	}
}

/*
=================
SND_Update
=================
*/
void SND_Update( refdef_t *refdef ) {
	if ( !al_initialized || !refdef ) {
		return;
	}
	
	// Update listener position based on client view
	vec3_t origin;
	vec3_t velocity;
	vec3_t forward, right, up;
	
	VectorCopy( refdef->vieworg, origin );
	VectorClear( velocity ); // No velocity info in refdef
	
	// Calculate orientation from view angles
	AngleVectors( refdef->viewangles, forward, right, up );
	
	alListener3f( AL_POSITION, origin[0], origin[1], origin[2] );
	alListener3f( AL_VELOCITY, velocity[0], velocity[1], velocity[2] );
	alListenerfv( AL_ORIENTATION, (ALfloat[]){ 
		forward[0], forward[1], forward[2], 
		up[0], up[1], up[2] 
	} );
	
	// Check for errors
	ALenum error = alGetError();
	if ( error != AL_NO_ERROR ) {
		Com_DPrintf( S_COLOR_YELLOW "OpenAL update error: %d\n", error );
	}
}

/*
=================
SND_DisableMusic
=================
*/
void SND_DisableMusic( void ) {
	// TODO: Implement music disabling
}

/*
=================
SND_RawSamples
=================
*/
void SND_RawSamples( int samples, int rate, int width, int channels, 
                     const byte *data, float volume ) {
	// TODO: Implement raw sample playback for cinematics
}

/*
=================
SND_Respatialize
=================
*/
void SND_Respatialize( int entityNum, const vec3_t origin, vec3_t hearingOrg, 
                       qboolean inwater ) {
	// Spatialization handled in SND_Update
}

/*
=================
SND_PositionEntity
=================
*/
void SND_PositionEntity( int entityNum, const vec3_t origin, const vec3_t velocity ) {
	// Entity positioning handled per-source
}

/*
=================
SND_Register
=================
*/
void SND_Register( void ) {
	// Called during level load to precache sounds
}

/*
=================
SND_BeginRegistration
=================
*/
sfxHandle_t SND_BeginRegistration( const char *name ) {
	// TODO: Implement sound registration
	return 0;
}

/*
=================
SND_EndRegistration
=================
*/
void SND_EndRegistration( void ) {
	// TODO: Implement end of registration
}

/*
=================
SND_ClearLoopingSounds
=================
*/
void SND_ClearLoopingSounds( void ) {
	// TODO: Clear looping sounds
}

/*
=================
SND_AddLoopingSound
=================
*/
void SND_AddLoopingSound( int entityNum, const vec3_t origin, const vec3_t velocity, 
                          sfxHandle_t sfx ) {
	// TODO: Add looping sound source
}

/*
=================
SND_StopLoopingSound
=================
*/
void SND_StopLoopingSound( int entityNum ) {
	// TODO: Stop looping sound for entity
}

/*
=================
SND_GetSoundVelocity
=================
*/
void SND_GetSoundVelocity( int entityNum, vec3_t velocity ) {
	VectorClear( velocity );
}

/*
=================
SND_UpdateEntityVelocity
=================
*/
void SND_UpdateEntityVelocity( int entityNum, const vec3_t velocity ) {
	// TODO: Update entity velocity for Doppler effect
}

/*
=================
SND_SetEntitySoundVolume
=================
*/
void SND_SetEntitySoundVolume( int entityNum, float volume ) {
	// TODO: Set entity sound volume
}

/*
=================
SND_IsSoundPlaying
=================
*/
qboolean SND_IsSoundPlaying( sfxHandle_t sfx ) {
	return qfalse;
}

/*
=================
SND_PlayBackgroundTrack
=================
*/
void SND_PlayBackgroundTrack( const char *track ) {
	// TODO: Implement background music playback
}

/*
=================
SND_StopBackgroundTrack
=================
*/
void SND_StopBackgroundTrack( void ) {
	// TODO: Stop background music
}

/*
=================
SND_LoopBackgroundTrack
=================
*/
void SND_LoopBackgroundTrack( const char *track ) {
	SND_PlayBackgroundTrack( track );
}
