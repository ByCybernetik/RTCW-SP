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
** SDL2 Sound Implementation
** 
** Modern replacement for linux_snd.c using SDL2 audio API.
** Properly handles x64 architecture with correct pointer types.
*/

#include <SDL2/SDL.h>
#include <stdint.h>  // Для intptr_t, uintptr_t - важно для x64!
#include <string.h>

#include "../game/q_shared.h"
#include "../client/snd_local.h"
#include "sdl_local.h"

// Глобальное состояние звука
static SDL_AudioDeviceID sdl_audio_device = 0;
static SDL_AudioSpec sdl_audio_spec;
static qboolean snd_sdl_initialized = qfalse;

// Буфер звука
static portable_samplepair_t *sdl_sound_buffer = NULL;
static int sdl_sound_samples = 0;

/*
=================
SDL_AudioCallback - Callback функция SDL2 для заполнения аудио буфера
=================
*/
static void SDL_AudioCallback( void *userdata, Uint8 *stream, int len ) {
    paintbuffer_t *paintbuffer;
    int samplecount;
    int i;
    
    if ( !snd_sdl_initialized || !sdl_sound_buffer ) {
        // Тишина если звук не инициализирован
        memset( stream, 0, len );
        return;
    }
    
    // Количество сэмплов для обработки
    samplecount = len / ( sizeof( short ) * 2 );  // стерео 16-bit
    
    // Заполняем буфер микшированными данными
    paintbuffer = (paintbuffer_t *)stream;
    
    // Микшируем звук из основного буфера
    for ( i = 0; i < samplecount; i++ ) {
        int sample_index = ( snd_paintedpos + i ) % sdl_sound_samples;
        
        paintbuffer[i].left = sdl_sound_buffer[sample_index].left;
        paintbuffer[i].right = sdl_sound_buffer[sample_index].right;
    }
}

/*
=================
SNDDMA_Init - Инициализация звуковой подсистемы через SDL2
=================
*/
qboolean SNDDMA_Init( void ) {
    SDL_AudioSpec wanted;
    int i;
    
    if ( snd_sdl_initialized ) {
        return qtrue;
    }
    
    Com_Printf( "------- Sound Initialization -------\n" );
    
    // Инициализируем аудиоподсистему SDL2
    if ( SDL_InitSubSystem( SDL_INIT_AUDIO ) < 0 ) {
        Com_Printf( S_COLOR_RED "SDL_InitAudio failed: %s\n", SDL_GetError() );
        return qfalse;
    }
    
    // Настраиваем желаемые параметры аудио
    memset( &wanted, 0, sizeof( wanted ) );
    wanted.freq = s_khz->integer == 22 ? 22050 : 44100;
    wanted.format = AUDIO_S16SYS;  // 16-bit signed, система-dependent endian
    wanted.channels = 2;            // стерео
    wanted.samples = 2048;          // размер буфера
    wanted.callback = SDL_AudioCallback;
    wanted.userdata = NULL;
    
    // Открываем аудио устройство
    sdl_audio_device = SDL_OpenAudioDevice( NULL, 0, &wanted, &sdl_audio_spec, 0 );
    
    if ( !sdl_audio_device ) {
        Com_Printf( S_COLOR_RED "SDL_OpenAudioDevice failed: %s\n", SDL_GetError() );
        SDL_QuitSubSystem( SDL_INIT_AUDIO );
        return qfalse;
    }
    
    // Устанавливаем параметры для движка
    dma.samplebits = 16;
    dma.channels = 2;
    dma.speed = sdl_audio_spec.freq;
    dma.samples = sdl_audio_spec.samples;
    dma.submission_chunk = 1;
    
    // Выделяем буфер звука
    sdl_sound_samples = dma.samples;
    sdl_sound_buffer = (portable_samplepair_t *)Z_Malloc( 
        sdl_sound_samples * sizeof( portable_samplepair_t ), 
        TAG_SOUND, 
        qfalse 
    );
    
    if ( !sdl_sound_buffer ) {
        Com_Printf( S_COLOR_RED "Failed to allocate sound buffer\n" );
        SDL_CloseAudioDevice( sdl_audio_device );
        SDL_QuitSubSystem( SDL_INIT_AUDIO );
        return qfalse;
    }
    
    dma.buffer = (portable_samplepair_t *)sdl_sound_buffer;
    
    // Запускаем воспроизведение
    SDL_PauseAudioDevice( sdl_audio_device, 0 );
    
    snd_sdl_initialized = qtrue;
    
    Com_Printf( "SDL2 audio initialized:\n" );
    Com_Printf( "  Frequency: %d Hz\n", dma.speed );
    Com_Printf( "  Format: %d-bit %s\n", dma.samplebits, dma.channels == 2 ? "stereo" : "mono" );
    Com_Printf( "  Samples: %d\n", dma.samples );
    Com_Printf( "------------------------------------\n" );
    
    return qtrue;
}

/*
=================
SNDDMA_Shutdown - Завершение работы звуковой подсистемы
=================
*/
void SNDDMA_Shutdown( void ) {
    if ( !snd_sdl_initialized ) {
        return;
    }
    
    // Останавливаем воспроизведение
    if ( sdl_audio_device ) {
        SDL_PauseAudioDevice( sdl_audio_device, 1 );
        SDL_CloseAudioDevice( sdl_audio_device );
        sdl_audio_device = 0;
    }
    
    // Освобождаем буфер
    if ( sdl_sound_buffer ) {
        Z_Free( sdl_sound_buffer );
        sdl_sound_buffer = NULL;
    }
    
    // Завершаем аудиоподсистему
    SDL_QuitSubSystem( SDL_INIT_AUDIO );
    
    snd_sdl_initialized = qfalse;
    
    Com_Printf( "SDL2 audio shutdown complete\n" );
}

/*
=================
SNDDMA_BeginPainting - Начало микширования кадра
=================
*/
void SNDDMA_BeginPainting( void ) {
    // SDL2 сам управляет буфером через callback
    // Эта функция может быть пустой
}

/*
=================
SNDDMA_Submit - Отправка микшированных данных
=================
*/
void SNDDMA_Submit( void ) {
    // SDL2 сам запрашивает данные через callback
    // Эта функция может быть пустой
}

/*
=================
SNDDMA_AdvancePaintpos - Продвижение позиции микширования
=================
*/
void SNDDMA_AdvancePaintpos( int samples ) {
    snd_paintedpos += samples;
}
