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
** UNIX_MAIN_GLFW.C - Основная точка входа с использованием GLFW
**
** Адаптировано для современных Linux систем
** - Использует GLFW для создания окна
** - Удалена зависимость от X11
** - Поддержка x64 архитектуры
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <dirent.h>

#include "../game/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../renderer/tr_public.h"
#include "linux_local.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

// Глобальные переменные
unsigned sys_frame_time;
uid_t saved_euid;
qboolean stdin_active = qtrue;

// Внешние функции из GLimp
extern void GLimp_EndFrame(void);

// Путь к домашней директории
static char home_path[MAX_OSPATH];

//===========================================================================
// Системные функции
//===========================================================================

/*
=================
Sys_Milliseconds
=================
*/
int Sys_Milliseconds(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

/*
=================
Sys_TickMilliseconds
=================
*/
long long Sys_TickMilliseconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

/*
=================
Sys_Sleep
=================
*/
void Sys_Sleep(int msec) {
    usleep(msec * 1000);
}

/*
=================
Sys_Error
=================
*/
void QDECL Sys_Error(const char* error, ...) {
    va_list argptr;
    char text[1024];
    
    va_start(argptr, error);
    vsprintf(text, error, argptr);
    va_end(argptr);
    
    fprintf(stderr, "Sys_Error: %s\n", text);
    
    // Закрытие GLFW если инициализирован
    glfwTerminate();
    
    exit(1);
}

/*
=================
Sys_Quit
=================
*/
void Sys_Quit(void) {
    Com_Shutdown();
    glfwTerminate();
    exit(0);
}

/*
=================
Sys_GetCurrentUser
=================
*/
char* Sys_GetCurrentUser(void) {
    struct passwd* p = getpwuid(getuid());
    return (p && p->pw_name) ? p->pw_name : "";
}

/*
=================
Sys_DefaultHomePath
=================
*/
char* Sys_DefaultHomePath(void) {
    if (!home_path[0]) {
        const char* homedir = getenv("HOME");
        
        if (!homedir) {
            struct passwd* pw = getpwuid(getuid());
            if (pw && pw->pw_dir) {
                homedir = pw->pw_dir;
            } else {
                homedir = ".";
            }
        }
        
        snprintf(home_path, sizeof(home_path), "%s/.rtcw", homedir);
    }
    
    return home_path;
}

/*
=================
Sys_GetClipboardData
=================
*/
char* Sys_GetClipboardData(void) {
    extern char* GLimp_GetClipboardData(void);
    return GLimp_GetClipboardData();
}

/*
=================
Sys_SetClipboardData
=================
*/
void Sys_SetClipboardData(const char* text) {
    extern void GLimp_SetClipboardData(const char* text);
    GLimp_SetClipboardData(text);
}

/*
=================
Sys_ConsoleInput
=================
*/
char* Sys_ConsoleInput(void) {
    // Консольный ввод обрабатывается через GLFW
    return NULL;
}

/*
=================
Sys_AnsicolorSupported
=================
*/
qboolean Sys_AnsicolorSupported(void) {
    // Проверяем поддержку ANSI цветов
    const char* term = getenv("TERM");
    return (term && strstr(term, "xterm")) ? qtrue : qfalse;
}

/*
=================
Sys_OpenURL
=================
*/
void Sys_OpenURL(const char* url) {
    char cmd[1024];
    
    snprintf(cmd, sizeof(cmd), "xdg-open '%s' &", url);
    system(cmd);
}

/*
=================
Sys_LowPhysicalMemory
=================
*/
qboolean Sys_LowPhysicalMemory(void) {
    // Возвращаем qfalse по умолчанию
    return qfalse;
}

/*
=================
Sys_ProcessorCount
=================
*/
int Sys_ProcessorCount(void) {
    return sysconf(_SC_NPROCESSORS_ONLN);
}

/*
=================
Sys_GetProcessorId
=================
*/
int Sys_GetProcessorId(void) {
    // Возвращаем CPUSTRING для x64
    return 0; // Generic x86_64
}

/*
=================
Sys_CPUString
=================
*/
const char* Sys_CPUString(void) {
#if defined(__x86_64__) || defined(_M_X64)
    return "Linux-x64";
#elif defined(__i386__) || defined(_M_IX86)
    return "Linux-x86";
#else
    return "Linux-other";
#endif
}

/*
=================
Sys_PlatformInit
=================
*/
void Sys_PlatformInit(void) {
    // Инициализация платформы
    saved_euid = geteuid();
    
    // Создание домашней директории если не существует
    char* home = Sys_DefaultHomePath();
    mkdir(home, 0755);
}

/*
=================
Sys_PlatformExit
=================
*/
void Sys_PlatformExit(void) {
    // Очистка платформы
}

/*
=================
Sys_Init
=================
*/
void Sys_Init(void) {
    Com_Printf("Sys_Init\n");
    
    sys_frame_time = Sys_Milliseconds();
    
    // Инициализация GLFW
    if (!glfwInit()) {
        Sys_Error("Не удалось инициализировать GLFW");
    }
    
    Com_Printf("CPU: %s\n", Sys_CPUString());
    Com_Printf("Processors: %d\n", Sys_ProcessorCount());
}

/*
=================
main
=================
*/
int main(int argc, char* argv[]) {
    // Инициализация
    Sys_PlatformInit();
    
    // Запуск движка
    Com_EventLoop();
    
    // Очистка
    Sys_PlatformExit();
    
    return 0;
}
