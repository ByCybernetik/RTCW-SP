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
** GLIMP.C - OpenGL implementation с использованием GLFW
** 
** Адаптировано для современных Linux систем
** - Использует GLFW для создания окна и обработки ввода
** - Поддержка OpenGL 3.3+ Core Profile
** - Удалена зависимость от X11/GLX
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "../renderer/tr_local.h"
#include "../client/client.h"
#include "linux_local.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

// Глобальные переменные
static GLFWwindow* window = NULL;
static qboolean glfw_initialized = qfalse;
static qboolean mouse_grabbed = qfalse;
static int window_width = 640;
static int window_height = 480;
static qboolean fullscreen = qfalse;

// Внешние переменные из движка
extern refimport_t ri;

// Прототипы функций
static void GLFWAPI errorCallback(int error, const char* description);
static void GLFWAPI cursorPosCallback(GLFWwindow* window, double xpos, double ypos);
static void GLFWAPI mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
static void GLFWAPI scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
static void GLFWAPI keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
static void GLFWAPI framebufferSizeCallback(GLFWwindow* window, int width, int height);

// Таблица клавиш GLFW -> Quake
static int keymap[GLFW_KEY_LAST];

//===========================================================================
// Инициализация таблицы клавиш
//===========================================================================
static void InitKeymap(void) {
    int i;
    
    for (i = 0; i < GLFW_KEY_LAST; i++) {
        keymap[i] = K_NONE;
    }
    
    // Буквы
    keymap[GLFW_KEY_A] = K_A;
    keymap[GLFW_KEY_B] = K_B;
    keymap[GLFW_KEY_C] = K_C;
    keymap[GLFW_KEY_D] = K_D;
    keymap[GLFW_KEY_E] = K_E;
    keymap[GLFW_KEY_F] = K_F;
    keymap[GLFW_KEY_G] = K_G;
    keymap[GLFW_KEY_H] = K_H;
    keymap[GLFW_KEY_I] = K_I;
    keymap[GLFW_KEY_J] = K_J;
    keymap[GLFW_KEY_K] = K_K;
    keymap[GLFW_KEY_L] = K_L;
    keymap[GLFW_KEY_M] = K_M;
    keymap[GLFW_KEY_N] = K_N;
    keymap[GLFW_KEY_O] = K_O;
    keymap[GLFW_KEY_P] = K_P;
    keymap[GLFW_KEY_Q] = K_Q;
    keymap[GLFW_KEY_R] = K_R;
    keymap[GLFW_KEY_S] = K_S;
    keymap[GLFW_KEY_T] = K_T;
    keymap[GLFW_KEY_U] = K_U;
    keymap[GLFW_KEY_V] = K_V;
    keymap[GLFW_KEY_W] = K_W;
    keymap[GLFW_KEY_X] = K_X;
    keymap[GLFW_KEY_Y] = K_Y;
    keymap[GLFW_KEY_Z] = K_Z;
    
    // Цифры
    keymap[GLFW_KEY_0] = K_0;
    keymap[GLFW_KEY_1] = K_1;
    keymap[GLFW_KEY_2] = K_2;
    keymap[GLFW_KEY_3] = K_3;
    keymap[GLFW_KEY_4] = K_4;
    keymap[GLFW_KEY_5] = K_5;
    keymap[GLFW_KEY_6] = K_6;
    keymap[GLFW_KEY_7] = K_7;
    keymap[GLFW_KEY_8] = K_8;
    keymap[GLFW_KEY_9] = K_9;
    
    // Функциональные клавиши
    keymap[GLFW_KEY_F1] = K_F1;
    keymap[GLFW_KEY_F2] = K_F2;
    keymap[GLFW_KEY_F3] = K_F3;
    keymap[GLFW_KEY_F4] = K_F4;
    keymap[GLFW_KEY_F5] = K_F5;
    keymap[GLFW_KEY_F6] = K_F6;
    keymap[GLFW_KEY_F7] = K_F7;
    keymap[GLFW_KEY_F8] = K_F8;
    keymap[GLFW_KEY_F9] = K_F9;
    keymap[GLFW_KEY_F10] = K_F10;
    keymap[GLFW_KEY_F11] = K_F11;
    keymap[GLFW_KEY_F12] = K_F12;
    keymap[GLFW_KEY_F13] = K_F13;
    keymap[GLFW_KEY_F14] = K_F14;
    keymap[GLFW_KEY_F15] = K_F15;
    
    // Специальные клавиши
    keymap[GLFW_KEY_ESCAPE] = K_ESCAPE;
    keymap[GLFW_KEY_ENTER] = K_ENTER;
    keymap[GLFW_KEY_TAB] = K_TAB;
    keymap[GLFW_KEY_BACKSPACE] = K_BACKSPACE;
    keymap[GLFW_KEY_INSERT] = K_INS;
    keymap[GLFW_KEY_DELETE] = K_DEL;
    keymap[GLFW_KEY_PAGE_UP] = K_PGUP;
    keymap[GLFW_KEY_PAGE_DOWN] = K_PGDN;
    keymap[GLFW_KEY_HOME] = K_HOME;
    keymap[GLFW_KEY_END] = K_END;
    keymap[GLFW_KEY_UP] = K_UPARROW;
    keymap[GLFW_KEY_DOWN] = K_DOWNARROW;
    keymap[GLFW_KEY_LEFT] = K_LEFTARROW;
    keymap[GLFW_KEY_RIGHT] = K_RIGHTARROW;
    keymap[GLFW_KEY_SPACE] = K_SPACE;
    keymap[GLFW_KEY_MINUS] = K_MINUS;
    keymap[GLFW_KEY_EQUAL] = K_EQUALS;
    keymap[GLFW_KEY_LEFT_BRACKET] = K_LEFTBRACKET;
    keymap[GLFW_KEY_RIGHT_BRACKET] = K_RIGHTBRACKET;
    keymap[GLFW_KEY_BACKSLASH] = K_BACKSLASH;
    keymap[GLFW_KEY_SEMICOLON] = K_SEMICOLON;
    keymap[GLFW_KEY_APOSTROPHE] = K_APOSTROPHE;
    keymap[GLFW_KEY_COMMA] = K_COMMA;
    keymap[GLFW_KEY_PERIOD] = K_PERIOD;
    keymap[GLFW_KEY_SLASH] = K_SLASH;
    keymap[GLFW_KEY_GRAVE_ACCENT] = K_TILDE;
    keymap[GLFW_KEY_CAPS_LOCK] = K_CAPSLOCK;
    keymap[GLFW_KEY_SCROLL_LOCK] = K_SCROLLLOCK;
    keymap[GLFW_KEY_NUM_LOCK] = K_KP_NUMLOCK;
    keymap[GLFW_KEY_PRINT_SCREEN] = K_PRINTSCREEN;
    keymap[GLFW_KEY_PAUSE] = K_PAUSE;
    
    // Numpad
    keymap[GLFW_KEY_KP_0] = K_KP_0;
    keymap[GLFW_KEY_KP_1] = K_KP_1;
    keymap[GLFW_KEY_KP_2] = K_KP_2;
    keymap[GLFW_KEY_KP_3] = K_KP_3;
    keymap[GLFW_KEY_KP_4] = K_KP_4;
    keymap[GLFW_KEY_KP_5] = K_KP_5;
    keymap[GLFW_KEY_KP_6] = K_KP_6;
    keymap[GLFW_KEY_KP_7] = K_KP_7;
    keymap[GLFW_KEY_KP_8] = K_KP_8;
    keymap[GLFW_KEY_KP_9] = K_KP_9;
    keymap[GLFW_KEY_KP_DECIMAL] = K_KP_PERIOD;
    keymap[GLFW_KEY_KP_DIVIDE] = K_KP_SLASH;
    keymap[GLFW_KEY_KP_MULTIPLY] = K_KP_STAR;
    keymap[GLFW_KEY_KP_SUBTRACT] = K_KP_MINUS;
    keymap[GLFW_KEY_KP_ADD] = K_KP_PLUS;
    keymap[GLFW_KEY_KP_ENTER] = K_KP_ENTER;
    
    // Модификаторы
    keymap[GLFW_KEY_LEFT_SHIFT] = K_LSHIFT;
    keymap[GLFW_KEY_RIGHT_SHIFT] = K_RSHIFT;
    keymap[GLFW_KEY_LEFT_CONTROL] = K_LCTRL;
    keymap[GLFW_KEY_RIGHT_CONTROL] = K_RCTRL;
    keymap[GLFW_KEY_LEFT_ALT] = K_LALT;
    keymap[GLFW_KEY_RIGHT_ALT] = K_RALT;
    keymap[GLFW_KEY_LEFT_SUPER] = K_LWIN;
    keymap[GLFW_KEY_RIGHT_SUPER] = K_RWIN;
    keymap[GLFW_KEY_MENU] = K_MENU;
}

//===========================================================================
// Callback функции GLFW
//===========================================================================

static void GLFWAPI errorCallback(int error, const char* description) {
    Com_Printf("GLFW Error %d: %s\n", error, description);
}

static void GLFWAPI framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    window_width = width;
    window_height = height;
    glViewport(0, 0, width, height);
}

static void GLFWAPI cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    if (mouse_grabbed) {
        // В режиме захвата мыши используем относительное движение
        static double last_x = 0, last_y = 0;
        
        float mx = (float)(xpos - last_x);
        float my = (float)(ypos - last_y);
        
        last_x = xpos;
        last_y = ypos;
        
        // Отправляем события движения мыши в движок
        CL_MouseEvent((int)mx, (int)my, 0);
    }
}

static void GLFWAPI mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    int key;
    
    switch (button) {
        case GLFW_MOUSE_BUTTON_LEFT:
            key = K_MOUSE1;
            break;
        case GLFW_MOUSE_BUTTON_RIGHT:
            key = K_MOUSE2;
            break;
        case GLFW_MOUSE_BUTTON_MIDDLE:
            key = K_MOUSE3;
            break;
        case GLFW_MOUSE_BUTTON_4:
            key = K_MOUSE4;
            break;
        case GLFW_MOUSE_BUTTON_5:
            key = K_MOUSE5;
            break;
        default:
            return;
    }
    
    if (action == GLFW_PRESS) {
        CL_KeyEvent(key, qtrue, Sys_Milliseconds());
    } else if (action == GLFW_RELEASE) {
        CL_KeyEvent(key, qfalse, Sys_Milliseconds());
    }
}

static void GLFWAPI scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    if (yoffset > 0) {
        CL_KeyEvent(K_MWHEELUP, qtrue, Sys_Milliseconds());
        CL_KeyEvent(K_MWHEELUP, qfalse, Sys_Milliseconds());
    } else if (yoffset < 0) {
        CL_KeyEvent(K_MWHEELDOWN, qtrue, Sys_Milliseconds());
        CL_KeyEvent(K_MWHEELDOWN, qfalse, Sys_Milliseconds());
    }
}

static void GLFWAPI keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key >= 0 && key < GLFW_KEY_LAST) {
        int quake_key = keymap[key];
        
        if (quake_key != K_NONE) {
            if (action == GLFW_PRESS) {
                CL_KeyEvent(quake_key, qtrue, Sys_Milliseconds());
            } else if (action == GLFW_RELEASE) {
                CL_KeyEvent(quake_key, qfalse, Sys_Milliseconds());
            }
        }
    }
}

//===========================================================================
// Функции GLimp
//===========================================================================

/*
=================
GLimp_Init
=================
*/
rserr_t GLimp_Init(void) {
    int major, minor;
    const char* version;
    
    Com_Printf("------- GLimp Initialization -------\n");
    
    if (glfw_initialized) {
        Com_Printf("GLFW уже инициализирован\n");
        return RSERR_OK;
    }
    
    // Инициализация GLFW
    glfwSetErrorCallback(errorCallback);
    
    if (!glfwInit()) {
        Com_Printf("Не удалось инициализировать GLFW\n");
        return RSERR_UNKNOWN;
    }
    
    glfw_initialized = qtrue;
    
    // Настройка версии OpenGL
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_RED_BITS, 8);
    glfwWindowHint(GLFW_GREEN_BITS, 8);
    glfwWindowHint(GLFW_BLUE_BITS, 8);
    glfwWindowHint(GLFW_ALPHA_BITS, 8);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);
    glfwWindowHint(GLFW_STENCIL_BITS, 8);
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
    
    // Получение текущего режима
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    
    window_width = r_mode->integer < 0 ? 640 : mode->width;
    window_height = r_mode->integer < 0 ? 480 : mode->height;
    fullscreen = r_fullscreen->integer ? qtrue : qfalse;
    
    // Создание окна
    if (fullscreen) {
        window = glfwCreateWindow(mode->width, mode->height, 
                                  WINDOW_CLASS_NAME, monitor, NULL);
    } else {
        window = glfwCreateWindow(window_width, window_height, 
                                  WINDOW_CLASS_NAME, NULL, NULL);
    }
    
    if (!window) {
        Com_Printf("Не удалось создать окно GLFW\n");
        glfwTerminate();
        glfw_initialized = qfalse;
        return RSERR_INVALID_MODE;
    }
    
    glfwMakeContextCurrent(window);
    
    // Загрузка функций OpenGL
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        Com_Printf("Не удалось загрузить OpenGL функции\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        glfw_initialized = qfalse;
        return RSERR_UNKNOWN;
    }
    
    // Получение информации о контексте
    version = (const char*)glGetString(GL_VERSION);
    major = glfwGetWindowAttrib(window, GLFW_CONTEXT_VERSION_MAJOR);
    minor = glfwGetWindowAttrib(window, GLFW_CONTEXT_VERSION_MINOR);
    
    Com_Printf("OpenGL Version: %s\n", version);
    Com_Printf("OpenGL Context: %d.%d\n", major, minor);
    Com_Printf("GL_VENDOR: %s\n", glGetString(GL_VENDOR));
    Com_Printf("GL_RENDERER: %s\n", glGetString(GL_RENDERER));
    
    // Настройка callback функций
    InitKeymap();
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetKeyCallback(window, keyCallback);
    
    // Начальная настройка viewport
    glfwGetFramebufferSize(window, &window_width, &window_height);
    glViewport(0, 0, window_width, window_height);
    
    // Настройка VSync
    glfwSwapInterval(r_vsync->integer ? 1 : 0);
    
    Com_Printf("------------------------------------\n");
    
    return RSERR_OK;
}

/*
=================
GLimp_Shutdown
=================
*/
void GLimp_Shutdown(void) {
    Com_Printf("GLimp_Shutdown\n");
    
    if (window) {
        glfwDestroyWindow(window);
        window = NULL;
    }
    
    if (glfw_initialized) {
        glfwTerminate();
        glfw_initialized = qfalse;
    }
}

/*
=================
GLimp_EndFrame
=================
*/
void GLimp_EndFrame(void) {
    if (window) {
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
}

/*
=================
GLimp_SetGamma
=================
*/
void GLimp_SetGamma(unsigned char red[256], unsigned char green[256], unsigned char blue[256]) {
    // В современном OpenGL гамма-коррекция делается через шейдеры или framebuffer
    // Эта функция может быть реализована через lookup texture
}

/*
=================
GLimp_SwitchFullscreen
=================
*/
qboolean GLimp_SwitchFullscreen(int width, int height) {
    if (!window) {
        return qfalse;
    }
    
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    
    if (fullscreen) {
        // Переход в оконный режим
        glfwSetWindowMonitor(window, NULL, 
                            (mode->width - width) / 2, 
                            (mode->height - height) / 2,
                            width, height, 0);
        fullscreen = qfalse;
    } else {
        // Переход в полноэкранный режим
        glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        fullscreen = qtrue;
    }
    
    return qtrue;
}

/*
=================
GLimp_GetClipboardData
=================
*/
char* GLimp_GetClipboardData(void) {
    if (!window) {
        return NULL;
    }
    
    const char* text = glfwGetClipboardString(window);
    if (text) {
        return strdup(text);
    }
    return NULL;
}

/*
=================
GLimp_SetClipboardData
=================
*/
void GLimp_SetClipboardData(const char* text) {
    if (window) {
        glfwSetClipboardString(window, text);
    }
}

/*
=================
Sys_ShowConsole
=================
*/
void Sys_ShowConsole(int level, qboolean show) {
    // Консоль отображается внутри игры
}

/*
=================
Conbuf_InterceptPrint
=================
*/
void Conbuf_InterceptPrint(const char* msg) {
    // Перехват вывода консоли (опционально)
}
