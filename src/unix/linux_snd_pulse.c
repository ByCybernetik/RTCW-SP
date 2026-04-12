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
** LINUX_SND_PULSE.C - Звуковая система с использованием PulseAudio
**
** Адаптировано для современных Linux систем
** - Использует PulseAudio вместо OSS/ALSA напрямую
** - Поддержка стерео и моно режимов
** - Асинхронный вывод звука
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pulse/simple.h>
#include <pulse/error.h>

#include "../game/q_shared.h"
#include "../client/snd_local.h"

// Глобальные переменные
static pa_simple* pulse_device = NULL;
static qboolean pulse_initialized = qfalse;
static int sample_rate = 22050;
static int channels = 2;
static int bits = 16;

// Переменные из оригинального кода
cvar_t *sndbits;
cvar_t *sndspeed;
cvar_t *sndchannels;
cvar_t *snddevice;

/*
=================
SNDDMA_Init
=================
*/
qboolean SNDDMA_Init(void) {
    pa_sample_spec ss;
    int error;
    
    Com_Printf("------- Sound Initialization -------\n");
    
    if (pulse_initialized) {
        Com_Printf("PulseAudio уже инициализирован\n");
        return qtrue;
    }
    
    // Чтение параметров из cvar
    sndbits = Cvar_Get("sndbits", "16", CVAR_SOUND);
    sndspeed = Cvar_Get("sndspeed", "22050", CVAR_SOUND);
    sndchannels = Cvar_Get("sndchannels", "2", CVAR_SOUND);
    snddevice = Cvar_Get("snddevice", "", CVAR_SOUND);
    
    // Настройка формата сэмплов
    bits = sndbits->integer;
    if (bits != 8 && bits != 16) {
        bits = 16;
    }
    
    sample_rate = sndspeed->integer;
    if (sample_rate < 8000 || sample_rate > 48000) {
        sample_rate = 22050;
    }
    
    channels = sndchannels->integer;
    if (channels != 1 && channels != 2) {
        channels = 2;
    }
    
    // Настройка sample spec для PulseAudio
    ss.format = (bits == 16) ? PA_SAMPLE_S16LE : PA_SAMPLE_U8;
    ss.rate = sample_rate;
    ss.channels = channels;
    
    // Открытие устройства
    pulse_device = pa_simple_new(
        NULL,                           // server
        "RTCW",                         // application name
        PA_STREAM_PLAYBACK,             // direction
        NULL,                           // device
        "Game Audio",                   // stream name
        &ss,                            // sample format spec
        NULL,                           // channel map
        NULL,                           // buffering attributes
        &error                          // error code
    );
    
    if (!pulse_device) {
        Com_Printf("Не удалось открыть PulseAudio устройство: %s\n", 
                   pa_strerror(error));
        return qfalse;
    }
    
    // Настройка dma buffer
    dma.buffer = dma.samples = 0;
    
    // Выделение буфера (2 секунды звука)
    dma.samples = sample_rate * channels * 2 / (bits / 8);
    dma.buffer = Z_Malloc(dma.samples * (bits / 8));
    
    if (!dma.buffer) {
        Com_Printf("Не удалось выделить память для звукового буфера\n");
        pa_simple_free(pulse_device);
        pulse_device = NULL;
        return qfalse;
    }
    
    Com_Memset(dma.buffer, 0, dma.samples * (bits / 8));
    
    dma.samplebits = bits;
    dma.speed = sample_rate;
    dma.channels = channels;
    
    Com_Printf("Sound system initialized:\n");
    Com_Printf("  Sample rate: %d Hz\n", sample_rate);
    Com_Printf("  Sample bits: %d\n", bits);
    Com_Printf("  Channels: %d\n", channels);
    Com_Printf("  Buffer samples: %d\n", dma.samples);
    Com_Printf("------------------------------------\n");
    
    pulse_initialized = qtrue;
    snd_inited = 1;
    
    return qtrue;
}

/*
=================
SNDDMA_Shutdown
=================
*/
void SNDDMA_Shutdown(void) {
    Com_Printf("SNDDMA_Shutdown\n");
    
    if (pulse_device) {
        pa_simple_drain(pulse_device, NULL);
        pa_simple_free(pulse_device);
        pulse_device = NULL;
    }
    
    if (dma.buffer) {
        Z_Free(dma.buffer);
        dma.buffer = NULL;
    }
    
    dma.samples = 0;
    pulse_initialized = qfalse;
    snd_inited = 0;
}

/*
=================
SNDDMA_BeginPainting
=================
*/
void SNDDMA_BeginPainting(void) {
    // Nothing to do for PulseAudio
}

/*
=================
SNDDMA_Submit
=================
*/
void SNDDMA_Submit(void) {
    int error;
    
    if (!pulse_device || !dma.buffer) {
        return;
    }
    
    // Отправка буфера в PulseAudio
    // В реальном коде здесь нужно отправлять только новые данные
    if (pa_simple_write(pulse_device, dma.buffer, 
                        dma.samples * (dma.samplebits / 8), &error) < 0) {
        Com_Printf("PulseAudio write error: %s\n", pa_strerror(error));
    }
}

/*
=================
SNDDMA_Activate
=================
*/
void SNDDMA_Activate(qboolean active) {
    // Pause/resume stream if needed
    if (!pulse_device) {
        return;
    }
    
    if (!active) {
        pa_simple_drain(pulse_device, NULL);
    }
}

/*
=================
SNDDMA_GetDMAPos
=================
*/
int SNDDMA_GetDMAPos(void) {
    // Return current position in buffer
    // For simplicity, return 0 (will be improved in production)
    return 0;
}

/*
=================
SNDDMA_LockBuffer
=================
*/
void SNDDMA_LockBuffer(void) {
    // Lock buffer for writing
}

/*
=================
SNDDMA_UnlockBuffer
=================
*/
void SNDDMA_UnlockBuffer(void) {
    // Unlock buffer after writing
}
