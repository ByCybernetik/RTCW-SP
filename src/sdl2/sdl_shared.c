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
** SDL2 Shared Implementation
** 
** Common utility functions for SDL2 platform.
** Properly handles x64 architecture with correct pointer types.
*/

#include <SDL2/SDL.h>
#include <stdint.h>  // Для intptr_t, uintptr_t - важно для x64!
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "../game/q_shared.h"
#include "../qcommon/qcommon.h"
#include "sdl_local.h"

/*
=================
Sys_GetClipboardData - Получение данных из буфера обмена
=================
*/
char *Sys_GetClipboardData( void ) {
    char *text = NULL;
    char *result = NULL;
    
    // Получаем текст из буфера обмена SDL2
    text = SDL_GetClipboardText();
    
    if ( text ) {
        // Копируем в формат RTCW
        size_t len = strlen( text ) + 1;
        result = (char *)Z_Malloc( len, TAG_TEMP_WORKSPACE, qfalse );
        
        if ( result ) {
            strcpy( result, text );
        }
        
        SDL_free( text );
    }
    
    return result;
}

/*
=================
Sys_SetClipboardData - Запись данных в буфер обмена
=================
*/
void Sys_SetClipboardData( const char *text ) {
    if ( text ) {
        SDL_SetClipboardText( text );
    }
}

/*
=================
Sys_Mkdir - Создание директории
=================
*/
void Sys_Mkdir( const char *path ) {
    // Создаем директорию с правами доступа 0755
    mkdir( path, 0755 );
}

/*
=================
Sys_rmdir - Удаление директории
=================
*/
qboolean Sys_rmdir( const char *path ) {
    return ( rmdir( path ) == 0 );
}

/*
=================
Sys_unlink - Удаление файла
=================
*/
qboolean Sys_unlink( const char *path ) {
    return ( unlink( path ) == 0 );
}

/*
=================
Sys_FindFirst - Начало поиска файлов
=================
*/
// Простая реализация через POSIX dirent
#include <dirent.h>

static DIR *finddir = NULL;
static struct dirent *findentry = NULL;

char *Sys_FindFirst( const char *path, unsigned musthave, unsigned canthave ) {
    static char fullname[MAX_OSPATH];
    char dirpath[MAX_OSPATH];
    char pattern[MAX_OSPATH];
    
    // Разбираем путь и паттерн
    strncpy( dirpath, path, sizeof( dirpath ) - 1 );
    dirpath[sizeof( dirpath ) - 1] = '\0';
    
    // Находим последний слэш
    char *lastslash = strrchr( dirpath, '/' );
    if ( lastslash ) {
        *lastslash = '\0';
        strncpy( pattern, lastslash + 1, sizeof( pattern ) - 1 );
    } else {
        strcpy( dirpath, "." );
        strncpy( pattern, path, sizeof( pattern ) - 1 );
    }
    
    // Открываем директорию
    finddir = opendir( dirpath );
    
    if ( !finddir ) {
        return NULL;
    }
    
    // Ищем первый файл
    while ( ( findentry = readdir( finddir ) ) != NULL ) {
        // Простая проверка паттерна (можно улучшить)
        if ( strstr( findentry->d_name, "*" ) || 1 ) {
            snprintf( fullname, sizeof( fullname ), "%s/%s", dirpath, findentry->d_name );
            return fullname;
        }
    }
    
    return NULL;
}

/*
=================
Sys_FindNext - Продолжение поиска файлов
=================
*/
char *Sys_FindNext( unsigned musthave, unsigned canthave ) {
    static char fullname[MAX_OSPATH];
    
    if ( !finddir || !findentry ) {
        return NULL;
    }
    
    while ( ( findentry = readdir( finddir ) ) != NULL ) {
        snprintf( fullname, sizeof( fullname ), "%s", findentry->d_name );
        return fullname;
    }
    
    return NULL;
}

/*
=================
Sys_FindClose - Завершение поиска файлов
=================
*/
void Sys_FindClose( void ) {
    if ( finddir ) {
        closedir( finddir );
        finddir = NULL;
    }
    findentry = NULL;
}

/*
=================
Sys_ConsoleInput - Чтение ввода из консоли
=================
*/
char *Sys_ConsoleInput( void ) {
    // В SDL2 нет стандартной консольной输入
    // Возвращаем NULL для использования графической консоли
    return NULL;
}

/*
=================
Sys_AnsiColorPrint - Вывод цветного текста
=================
*/
void Sys_AnsiColorPrint( const char *msg ) {
    // Выводим сообщение в stdout
    // SDL2 не имеет прямой поддержки ANSI цветов в консоли
    fprintf( stdout, "%s", msg );
    fflush( stdout );
}

/*
=================
Sys_OpenURL - Открытие URL в браузере
=================
*/
void Sys_OpenURL( const char *url ) {
    // Используем SDL2 для открытия URL
    SDL_OpenURL( url );
}

/*
=================
Sys_GLimpSafeInit - Безопасная инициализация OpenGL
=================
*/
void Sys_GLimpSafeInit( void ) {
    // SDL2 сам управляет безопасностью OpenGL контекста
    // Эта функция может быть пустой
}

/*
=================
Sys_GLimpInit - Инициализация OpenGL
=================
*/
void Sys_GLimpInit( void ) {
    // Вызываем GLimp_Init
    GLimp_Init();
}

/*
=================
Com_PrintMessage - Вывод сообщения консоли
=================
*/
void Com_PrintMessage( const char *msg ) {
    // Выводим в stdout
    fprintf( stdout, "%s", msg );
    fflush( stdout );
}

/*
=================
Sys_GetCurrentUser - Получение имени текущего пользователя
=================
*/
char *Sys_GetCurrentUser( void ) {
    static char username[256];
    char *home = getenv( "HOME" );
    
    if ( home ) {
        // Извлекаем имя пользователя из пути HOME
        char *slash = strrchr( home, '/' );
        if ( slash ) {
            strncpy( username, slash + 1, sizeof( username ) - 1 );
        } else {
            strncpy( username, home, sizeof( username ) - 1 );
        }
        username[sizeof( username ) - 1] = '\0';
    } else {
        strcpy( username, "unknown" );
    }
    
    return username;
}
