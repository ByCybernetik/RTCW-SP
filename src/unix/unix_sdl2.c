/*
===========================================================================

Return to Castle Wolfenstein single player GPL Source Code
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company. 

This file is part of the Return to Castle Wolfenstein single player GPL Source Code.

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

===========================================================================
*/

// unix_sdl2.c - SDL2 implementation for video, sound and input

#include <SDL2/SDL.h>
#ifdef VULKAN_BACKEND
#include <SDL2/SDL_vulkan.h>
#endif

#include "../renderer/tr_local.h"
#ifdef VULKAN_BACKEND
#include "../vulkan/vk_local.h"
#endif
#include "../client/client.h"
#include "../client/snd_local.h"
#include "unix_glw.h"

// Define glw_state for SDL2 backend
glwstate_t glw_state;

// SDL2 QGL function
extern qboolean QGL_SDL_Init( void );

// Forward declaration
qboolean GLimp_SetMode( int mode, qboolean fullscreenFlag, qboolean noborder );

// Forward declarations
void IN_Init( void );

/*
===============
Video mode table

Must stay in sync with r_vidModes[] in src/renderer/tr_init.c.
Mode -1 is custom (r_customwidth/r_customheight).
===============
*/
static const int s_modeTable[][2] = {
    { 320, 240 },   // mode 0
    { 400, 300 },   // mode 1
    { 512, 384 },   // mode 2
    { 640, 480 },   // mode 3
    { 800, 600 },   // mode 4
    { 960, 720 },   // mode 5
    { 1024, 768 },  // mode 6
    { 1152, 864 },  // mode 7
    { 1280, 1024 }, // mode 8
    { 1600, 1200 }, // mode 9
    { 2048, 1536 }, // mode 10
    { 856, 480 },   // mode 11 (wide)
    { 1920, 1200 }  // mode 12 (wide)
};
static const int s_numModes = sizeof( s_modeTable ) / sizeof( s_modeTable[0] );
void IN_Shutdown( void );
void IN_ProcessEvents( void );
void IN_StartupJoystick( void );
void IN_ShutdownJoystick( void );
void IN_ActivateMouse( void );
void IN_DeactivateMouse( void );
void IN_MouseMove( void );
void IN_JoystickMove( void );

// Video
static SDL_Window *window = NULL;
static SDL_GLContext glContext = NULL;
static qboolean fullscreen_active = qfalse;

// Input
static qboolean mouseActive = qfalse;
static qboolean mouseAvailable = qfalse;
static int mouseX = 0, mouseY = 0;

// Joystick
static SDL_Joystick *stick = NULL;

// Sound
static SDL_AudioDeviceID audioDevice = 0;

// Key conversion table (simplified - maps SDL scancodes to Quake keys)
static int sdl_to_quake_key(SDL_Scancode scancode) {
    switch (scancode) {
        case SDL_SCANCODE_ESCAPE: return K_ESCAPE;
        case SDL_SCANCODE_1: return '1';
        case SDL_SCANCODE_2: return '2';
        case SDL_SCANCODE_3: return '3';
        case SDL_SCANCODE_4: return '4';
        case SDL_SCANCODE_5: return '5';
        case SDL_SCANCODE_6: return '6';
        case SDL_SCANCODE_7: return '7';
        case SDL_SCANCODE_8: return '8';
        case SDL_SCANCODE_9: return '9';
        case SDL_SCANCODE_0: return '0';
        case SDL_SCANCODE_MINUS: return '-';
        case SDL_SCANCODE_EQUALS: return '=';
        case SDL_SCANCODE_BACKSPACE: return K_BACKSPACE;
        case SDL_SCANCODE_TAB: return K_TAB;
        case SDL_SCANCODE_Q: return 'q';
        case SDL_SCANCODE_W: return 'w';
        case SDL_SCANCODE_E: return 'e';
        case SDL_SCANCODE_R: return 'r';
        case SDL_SCANCODE_T: return 't';
        case SDL_SCANCODE_Y: return 'y';
        case SDL_SCANCODE_U: return 'u';
        case SDL_SCANCODE_I: return 'i';
        case SDL_SCANCODE_O: return 'o';
        case SDL_SCANCODE_P: return 'p';
        case SDL_SCANCODE_LEFTBRACKET: return '[';
        case SDL_SCANCODE_RIGHTBRACKET: return ']';
        case SDL_SCANCODE_RETURN: return K_ENTER;
        case SDL_SCANCODE_LCTRL: return K_CTRL;
        case SDL_SCANCODE_A: return 'a';
        case SDL_SCANCODE_S: return 's';
        case SDL_SCANCODE_D: return 'd';
        case SDL_SCANCODE_F: return 'f';
        case SDL_SCANCODE_G: return 'g';
        case SDL_SCANCODE_H: return 'h';
        case SDL_SCANCODE_J: return 'j';
        case SDL_SCANCODE_K: return 'k';
        case SDL_SCANCODE_L: return 'l';
        case SDL_SCANCODE_SEMICOLON: return ';';
        case SDL_SCANCODE_APOSTROPHE: return '\'';
        case SDL_SCANCODE_GRAVE: return '`';
        case SDL_SCANCODE_LSHIFT: return K_SHIFT;
        case SDL_SCANCODE_BACKSLASH: return '\\';
        case SDL_SCANCODE_Z: return 'z';
        case SDL_SCANCODE_X: return 'x';
        case SDL_SCANCODE_C: return 'c';
        case SDL_SCANCODE_V: return 'v';
        case SDL_SCANCODE_B: return 'b';
        case SDL_SCANCODE_N: return 'n';
        case SDL_SCANCODE_M: return 'm';
        case SDL_SCANCODE_COMMA: return ',';
        case SDL_SCANCODE_PERIOD: return '.';
        case SDL_SCANCODE_SLASH: return '/';
        case SDL_SCANCODE_RSHIFT: return K_SHIFT;
        case SDL_SCANCODE_KP_MULTIPLY: return '*';
        case SDL_SCANCODE_LALT: return K_ALT;
        case SDL_SCANCODE_SPACE: return K_SPACE;
        case SDL_SCANCODE_CAPSLOCK: return K_CAPSLOCK;
        case SDL_SCANCODE_F1: return K_F1;
        case SDL_SCANCODE_F2: return K_F2;
        case SDL_SCANCODE_F3: return K_F3;
        case SDL_SCANCODE_F4: return K_F4;
        case SDL_SCANCODE_F5: return K_F5;
        case SDL_SCANCODE_F6: return K_F6;
        case SDL_SCANCODE_F7: return K_F7;
        case SDL_SCANCODE_F8: return K_F8;
        case SDL_SCANCODE_F9: return K_F9;
        case SDL_SCANCODE_F10: return K_F10;
        case SDL_SCANCODE_NUMLOCKCLEAR: return K_KP_NUMLOCK;
        case SDL_SCANCODE_SCROLLLOCK: return 0;
        case SDL_SCANCODE_KP_7: return K_KP_HOME;
        case SDL_SCANCODE_KP_8: return K_KP_UPARROW;
        case SDL_SCANCODE_KP_9: return K_KP_PGUP;
        case SDL_SCANCODE_KP_MINUS: return K_KP_MINUS;
        case SDL_SCANCODE_KP_4: return K_KP_LEFTARROW;
        case SDL_SCANCODE_KP_5: return K_KP_5;
        case SDL_SCANCODE_KP_6: return K_KP_RIGHTARROW;
        case SDL_SCANCODE_KP_PLUS: return K_KP_PLUS;
        case SDL_SCANCODE_KP_1: return K_KP_END;
        case SDL_SCANCODE_KP_2: return K_KP_DOWNARROW;
        case SDL_SCANCODE_KP_3: return K_KP_PGDN;
        case SDL_SCANCODE_KP_0: return K_KP_INS;
        case SDL_SCANCODE_KP_PERIOD: return K_KP_DEL;
        case SDL_SCANCODE_F11: return K_F11;
        case SDL_SCANCODE_F12: return K_F12;
        case SDL_SCANCODE_F13: return K_F13;
        case SDL_SCANCODE_F14: return K_F14;
        case SDL_SCANCODE_F15: return K_F15;
        case SDL_SCANCODE_KP_ENTER: return K_KP_ENTER;
        case SDL_SCANCODE_RCTRL: return K_CTRL;
        case SDL_SCANCODE_KP_DIVIDE: return K_KP_SLASH;
        case SDL_SCANCODE_SYSREQ: return 0;
        case SDL_SCANCODE_RALT: return K_ALT;
        case SDL_SCANCODE_HOME: return K_HOME;
        case SDL_SCANCODE_UP: return K_UPARROW;
        case SDL_SCANCODE_PAGEUP: return K_PGUP;
        case SDL_SCANCODE_LEFT: return K_LEFTARROW;
        case SDL_SCANCODE_RIGHT: return K_RIGHTARROW;
        case SDL_SCANCODE_END: return K_END;
        case SDL_SCANCODE_DOWN: return K_DOWNARROW;
        case SDL_SCANCODE_PAGEDOWN: return K_PGDN;
        case SDL_SCANCODE_INSERT: return K_INS;
        case SDL_SCANCODE_DELETE: return K_DEL;
        case SDL_SCANCODE_LGUI: return 0;
        case SDL_SCANCODE_RGUI: return 0;
        case SDL_SCANCODE_PAUSE: return K_PAUSE;
        default: return 0;
    }
}

/*
===============
GLimp_Init
===============
*/
void GLimp_Init( void ) {
    cvar_t *r_mode = Cvar_Get("r_mode", "3", CVAR_ARCHIVE | CVAR_LATCH);
    cvar_t *r_fullscreen = Cvar_Get("r_fullscreen", "0", CVAR_ARCHIVE | CVAR_LATCH);
    
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0) {
        Com_Error(ERR_FATAL, "SDL_Init failed: %s\n", SDL_GetError());
    }
    
    Com_Printf("SDL2 video driver: %s\n", SDL_GetCurrentVideoDriver());
    
    // Create window and GL context
    if (!GLimp_SetMode(r_mode->integer, r_fullscreen->integer, qfalse)) {
        Com_Error(ERR_FATAL, "GLimp_Init() - could not create OpenGL context\n");
    }
}

#ifdef VULKAN_BACKEND
/*
===============
VKimp_Init

Initialize Vulkan: create SDL window with VULKAN flag, set up instance and
surface, then initialize the Vulkan device and swapchain.
===============
*/
qboolean VKimp_Init(void)
{
    cvar_t *r_mode_cv = Cvar_Get("r_mode", "3", CVAR_ARCHIVE | CVAR_LATCH);
    cvar_t *r_fullscreen_cv = Cvar_Get("r_fullscreen", "0", CVAR_ARCHIVE | CVAR_LATCH);
    int width = 640, height = 480;
    int mode;
    unsigned int extCount;
    const char **extensions;
    VkInstance instance;
    VkSurfaceKHR surface;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0) {
        Com_Error(ERR_FATAL, "VKimp_Init: SDL_Init failed: %s\n", SDL_GetError());
        return qfalse;
    }

    Com_Printf("SDL2 video driver: %s\n", SDL_GetCurrentVideoDriver());

    /* Determine resolution from mode CVar */
    mode = r_mode_cv->integer;
    if (mode == -1) {
        width = Cvar_Get("r_customwidth", "1600", CVAR_ARCHIVE | CVAR_LATCH)->integer;
        height = Cvar_Get("r_customheight", "1024", CVAR_ARCHIVE | CVAR_LATCH)->integer;
    } else {
        if ( mode >= 0 && mode < s_numModes ) {
            width = s_modeTable[mode][0];
            height = s_modeTable[mode][1];
        }
    }

    Com_Printf("VKimp_Init: %dx%d (fullscreen: %d)\n", width, height, r_fullscreen_cv->integer);

    /* Destroy existing window if any */
    if (window) {
        SDL_DestroyWindow(window);
        window = NULL;
    }

    /* Create window with Vulkan support */
    Uint32 winFlags = SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN;
    if (!r_fullscreen_cv->integer) {
        winFlags |= SDL_WINDOW_RESIZABLE;
    } else {
        winFlags |= SDL_WINDOW_FULLSCREEN;
    }

    window = SDL_CreateWindow("Return to Castle Wolfenstein (Vulkan)",
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              width, height, winFlags);
    if (!window) {
        Com_Printf("VKimp_Init: SDL_CreateWindow failed: %s\n", SDL_GetError());
        return qfalse;
    }

    /* Get required Vulkan instance extensions from SDL */
    SDL_Vulkan_GetInstanceExtensions(window, &extCount, NULL);
    extensions = malloc(extCount * sizeof(const char *));
    if (!extensions) {
        SDL_DestroyWindow(window);
        window = NULL;
        return qfalse;
    }
    SDL_Vulkan_GetInstanceExtensions(window, &extCount, extensions);

    /* Create Vulkan instance with SDL extensions (validation layer opt-in) */
    {
        const char *validationLayer = "VK_LAYER_KHRONOS_validation";
        const char *layers[1];
        uint32_t layerCount = 0;

        if (getenv("RTCW_VK_VALIDATION")) {
            layers[layerCount++] = validationLayer;
            Com_Printf("VKimp_Init: enabling %s\n", validationLayer);
        }

        if (!VK_CreateInstance(extensions, extCount, layers, layerCount)) {
            Com_Printf("VKimp_Init: VK_CreateInstance failed\n");
            free(extensions);
            SDL_DestroyWindow(window);
            window = NULL;
            return qfalse;
        }
    }
    instance = vk_state.instance;
    free(extensions);

    /* Create Vulkan surface from SDL window */
    if (!SDL_Vulkan_CreateSurface(window, instance, &surface)) {
        Com_Printf("VKimp_Init: SDL_Vulkan_CreateSurface failed: %s\n", SDL_GetError());
        VK_DestroyInstance(); /* destroys vk_state.instance and debug messenger */
        SDL_DestroyWindow(window);
        window = NULL;
        return qfalse;
    }

    /* Initialize Vulkan device, swapchain, etc. */
    if (!VK_InitFromPlatform(width, height, instance, surface)) {
        /* VK_Shutdown (called from VK_InitFromPlatform on failure) does NOT
         * destroy the instance — platform owns it. Clean up surface + instance. */
        vkDestroySurfaceKHR(instance, surface, NULL);
        vkDestroyInstance(instance, NULL);
        SDL_DestroyWindow(window);
        window = NULL;
        return qfalse;
    }

    /* Populate glConfig for the rest of the engine */
    glConfig.vidWidth    = width;
    glConfig.vidHeight   = height;
    glConfig.windowAspect = (float)width / height;
    glConfig.colorBits   = 32;
    glConfig.depthBits   = 24;
    glConfig.stencilBits = 8;

    vk_active = qtrue;
    RB_RenderView = VK_RenderView;

    /* Show window */
    SDL_ShowWindow(window);
    SDL_RaiseWindow(window);

    if (vk_state.physFeatures.samplerAnisotropy) {
        Com_Printf("VKimp_Init: anisotropic filtering supported (max %.1fx)\n", vk_state.maxSamplerAnisotropy);
    } else {
        Com_Printf("VKimp_Init: anisotropic filtering not supported\n");
    }

    Com_Printf("VKimp_Init: Vulkan initialized successfully\n");
    return qtrue;
}

void VKimp_Shutdown(void)
{
    VkInstance instance = vk_state.instance;
    VkSurfaceKHR surface = vk_state.surface;

    /* Vulkan objects (including the swapchain) must be destroyed BEFORE the
     * surface they were created from, otherwise the driver's wayland code
     * crashes tearing down the presentation engine. */
    VK_Shutdown();
    if (surface)
        vkDestroySurfaceKHR(instance, surface, NULL);
    /* Destroy instance (VK_Shutdown zeroes vk, so we use saved handle) */
    if (instance) {
        vkDestroyInstance(instance, NULL);
    }
    if (window) {
        SDL_DestroyWindow(window);
        window = NULL;
    }
    SDL_QuitSubSystem(SDL_INIT_VIDEO);

    /* Reset glConfig so InitOpenGL will recreate the Vulkan window/device on restart. */
    glConfig.vidWidth = 0;
    glConfig.vidHeight = 0;

    vk_active = qfalse;
}
#endif /* VULKAN_BACKEND */

/*
===============
GLimp_Shutdown
===============
*/
void GLimp_Shutdown( void ) {
    IN_Shutdown();
    
    if (glContext) {
        SDL_GL_DeleteContext(glContext);
        glContext = NULL;
    }
    if (window) {
        SDL_DestroyWindow(window);
        window = NULL;
    }
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    
    // x64: reset vidWidth to force window recreation on next init
    glConfig.vidWidth = 0;
    glConfig.vidHeight = 0;
}

/*
===============
GLimp_SetGamma
===============
*/
void GLimp_SetGamma( unsigned char red[256], unsigned char green[256], unsigned char blue[256] ) {
    Uint16 ramp[256 * 3];
    int i;
    
    for (i = 0; i < 256; i++) {
        ramp[i] = red[i] << 8;
        ramp[i + 256] = green[i] << 8;
        ramp[i + 512] = blue[i] << 8;
    }
    
    SDL_SetWindowGammaRamp(window, ramp, ramp + 256, ramp + 512);
}

/*
===============
GLimp_SwapBuffers
===============
*/
void GLimp_SwapBuffers( void ) {
    if (!window) {
        Com_Printf("GLimp_SwapBuffers: no window!\n");
        return;
    }
    // Force clear to red to debug rendering
    // qglClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    // qglClear(GL_COLOR_BUFFER_BIT);
    SDL_GL_SwapWindow(window);
}

/*
===============
GLimp_SetMode
===============
*/
qboolean GLimp_SetMode( int mode, qboolean fullscreenFlag, qboolean noborder ) {
    int width, height;
    const char *glstring;
    
    // Parse mode string or use defaults
    if (mode == -1) {
        width = Cvar_Get("r_customwidth", "1600", CVAR_ARCHIVE | CVAR_LATCH)->integer;
        height = Cvar_Get("r_customheight", "1024", CVAR_ARCHIVE | CVAR_LATCH)->integer;
    } else {
        if ( mode >= 0 && mode < s_numModes ) {
            width = s_modeTable[mode][0];
            height = s_modeTable[mode][1];
        } else {
            width = 640;
            height = 480;
        }
    }
    
    Com_Printf("Setting mode %d: %dx%d (fullscreen: %d)\n", mode, width, height, fullscreenFlag);
    
    // Destroy existing window
    if (window) {
        SDL_DestroyWindow(window);
        window = NULL;
    }
    
    // Setup GL attributes
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    
    // Create window
    Uint32 windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN;
    // SDL_WINDOW_HIDDEN to avoid early SDL_QUIT on some WMs
    // x64: only allow resize in windowed mode to prevent Wayland auto-scaling
    if (!fullscreenFlag) {
        windowFlags |= SDL_WINDOW_RESIZABLE;
    }
    if (fullscreenFlag) {
        windowFlags |= SDL_WINDOW_FULLSCREEN;
    }
    if (noborder) {
        windowFlags |= SDL_WINDOW_BORDERLESS;
    }
    
    window = SDL_CreateWindow("Return to Castle Wolfenstein",
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              width, height,
                              windowFlags);
    
    if (!window) {
        Com_Printf("SDL_CreateWindow failed: %s\n", SDL_GetError());
        return qfalse;
    }
    
    // Create GL context
    glContext = SDL_GL_CreateContext(window);
    if (!glContext) {
        Com_Printf("SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        window = NULL;
        return qfalse;
    }
    
    SDL_GL_MakeCurrent(window, glContext);
    
    // Show window (was created hidden to avoid early SDL_QUIT)
    SDL_ShowWindow(window);
    
    // x64: Force window size to ensure it matches requested resolution
    SDL_SetWindowSize(window, width, height);
    
    // Load OpenGL functions via SDL2
    if ( !QGL_SDL_Init() ) {
        Com_Printf( "QGL_Init failed\n" );
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        window = NULL;
        glContext = NULL;
        return qfalse;
    }
    
    // Enable vsync if requested
    SDL_GL_SetSwapInterval(Cvar_Get("r_swapInterval", "0", CVAR_ARCHIVE)->integer);
    
    // Get actual GL config
    int r, g, b, a, depth, stencil;
    SDL_GL_GetAttribute(SDL_GL_RED_SIZE, &r);
    SDL_GL_GetAttribute(SDL_GL_GREEN_SIZE, &g);
    SDL_GL_GetAttribute(SDL_GL_BLUE_SIZE, &b);
    SDL_GL_GetAttribute(SDL_GL_ALPHA_SIZE, &a);
    SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE, &depth);
    SDL_GL_GetAttribute(SDL_GL_STENCIL_SIZE, &stencil);
    
    glConfig.colorBits = r + g + b;
    glConfig.depthBits = depth;
    glConfig.stencilBits = stencil;
    
    // Get GL strings
    glstring = (const char *)qglGetString(GL_VENDOR);
    if (glstring) Q_strncpyz(glConfig.vendor_string, glstring, sizeof(glConfig.vendor_string));
    Com_Printf("GL_VENDOR: %s\n", glConfig.vendor_string);
    
    glstring = (const char *)qglGetString(GL_RENDERER);
    if (glstring) Q_strncpyz(glConfig.renderer_string, glstring, sizeof(glConfig.renderer_string));
    Com_Printf("GL_RENDERER: %s\n", glConfig.renderer_string);
    
    glstring = (const char *)qglGetString(GL_VERSION);
    if (glstring) Q_strncpyz(glConfig.version_string, glstring, sizeof(glConfig.version_string));
    Com_Printf("GL_VERSION: %s\n", glConfig.version_string);
    
    // Skip extensions string - too long for modern GPUs (can exceed 4096 buffer)
    
    glConfig.vidWidth = width;
    glConfig.vidHeight = height;
    glConfig.windowAspect = (float)width / height;
    fullscreen_active = fullscreenFlag;
    
    // Show window after all setup is done
    SDL_ShowWindow(window);
    SDL_RaiseWindow(window);
    
    // Debug window state
    Uint32 flags = SDL_GetWindowFlags(window);
    Com_Printf("SDL Window flags: 0x%x (shown=%d, mapped=%d)\n", 
               flags, 
               (flags & SDL_WINDOW_SHOWN) ? 1 : 0,
               (window != NULL) ? 1 : 0);
    
    // Make sure window is visible and not minimized
    SDL_SetWindowGrab(window, SDL_FALSE);
    SDL_SetWindowMinimumSize(window, width, height);
    
    // Give the WM a moment to map the window
    SDL_Delay(100);
    
    IN_Init();
    
    return qtrue;
}

/*
===============
GLimp_HaveExtension
===============
*/
qboolean GLimp_HaveExtension( const char *ext ) {
    const char *extensions = (const char *)glGetString(GL_EXTENSIONS);
    return (extensions && strstr(extensions, ext)) ? qtrue : qfalse;
}

/*
===============
GLimp_LogComment
===============
*/
void GLimp_LogComment( char *comment ) {
    // Stub
}

/* ============================================================================
                                INPUT
   ============================================================================ */

/*
===============
IN_Init
===============
*/
void IN_Init( void ) {
    Com_Printf("SDL2 input initialized\n");
    mouseAvailable = qtrue;
    IN_StartupJoystick();
    
    // Hide system cursor - game draws its own
    SDL_ShowCursor(SDL_DISABLE);
    
    // Enable text input for console
    SDL_StartTextInput();
}

/*
===============
IN_Shutdown
===============
*/
void IN_Shutdown( void ) {
    IN_DeactivateMouse();
    mouseActive = qfalse;
    mouseAvailable = qfalse;
    IN_ShutdownJoystick();
}

/*
===============
IN_ActivateMouse
===============
*/
void IN_ActivateMouse( void ) {
    if (!mouseAvailable || !window) return;
    
    if (!mouseActive) {
        // Multiple grab methods for maximum compatibility
        SDL_SetWindowGrab(window, SDL_TRUE);
        SDL_SetWindowMouseGrab(window, SDL_TRUE);
        SDL_CaptureMouse(SDL_TRUE);
        SDL_SetRelativeMouseMode(SDL_TRUE);
        SDL_ShowCursor(SDL_DISABLE);
        SDL_WarpMouseInWindow(window, glConfig.vidWidth / 2, glConfig.vidHeight / 2);
        mouseActive = qtrue;
    }
}

/*
===============
IN_DeactivateMouse
===============
*/
void IN_DeactivateMouse( void ) {
    if (!mouseAvailable || !window) return;
    
    if (mouseActive) {
        SDL_SetRelativeMouseMode(SDL_FALSE);
        SDL_CaptureMouse(SDL_FALSE);
        SDL_SetWindowMouseGrab(window, SDL_FALSE);
        SDL_SetWindowGrab(window, SDL_FALSE);
        // Keep cursor hidden - game draws its own cursor
        mouseActive = qfalse;
    }
}

/*
===============
IN_MouseMove
===============
*/
static int lastMouseX = 0, lastMouseY = 0;

void IN_MouseMove( void ) {
    float mx, my;
    
    mx = (float)mouseX;
    my = (float)mouseY;
    
    if (mouseActive) {
        // Game mode: relative movement
        mouseX = 0;
        mouseY = 0;
        
        if (!mx && !my) return;
        
        CL_MouseEvent((int)mx, (int)my, Sys_Milliseconds());
    } else {
        // UI mode: absolute position, calculate delta
        int dx = mouseX - lastMouseX;
        int dy = mouseY - lastMouseY;
        
        lastMouseX = mouseX;
        lastMouseY = mouseY;
        
        if (dx || dy) {
            CL_MouseEvent(dx, dy, Sys_Milliseconds());
        }
    }
}

/*
===============
IN_Frame
===============
*/
void IN_Frame( void ) {
    // Auto-manage mouse capture based on game state
    // If no key catchers (UI/console) and game is active, capture mouse
    if (cls.state == CA_ACTIVE && !cls.keyCatchers) {
        if (!mouseActive) {
            IN_ActivateMouse();
        }
        // Aggressive mouse confinement - warp to center every frame when in game
        // This ensures the cursor never reaches window edges
        SDL_WarpMouseInWindow(window, glConfig.vidWidth / 2, glConfig.vidHeight / 2);
    } else {
        // UI mode - release mouse
        if (mouseActive) {
            IN_DeactivateMouse();
        }
    }
    
    IN_MouseMove();
}

/*
===============
IN_ProcessEvents
===============
*/
void IN_ProcessEvents( void ) {
    SDL_Event event;
    
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                Sys_Quit();
                break;
                
            case SDL_KEYDOWN:
            case SDL_KEYUP: {
                SDL_Keycode keycode = event.key.keysym.sym;
                int qkey;

                // Prefer scancode mapping (Backspace is SDLK_BACKSPACE=8 but K_BACKSPACE=127)
                qkey = sdl_to_quake_key( event.key.keysym.scancode );
                if ( !qkey && keycode > 0 && keycode < 128 ) {
                    qkey = (int)keycode;
                }

                qboolean down = (event.type == SDL_KEYDOWN);
                
                if (qkey) {
                    CL_KeyEvent(qkey, down, event.key.timestamp);
                }
                break;
            }
            
            case SDL_TEXTINPUT: {
                // Handle UTF-8 text input for console, chat and UI fields.
                CL_TextInput( event.text.text );
                break;
            }
            
            case SDL_MOUSEMOTION:
                // Always track mouse for UI, use relative for game when captured
                if (mouseActive) {
                    mouseX += event.motion.xrel;
                    mouseY += event.motion.yrel;
                } else {
                    // For UI: use absolute position
                    mouseX = event.motion.x;
                    mouseY = event.motion.y;
                }
                break;
                
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP: {
                int qkey = 0;
                qboolean down = (event.type == SDL_MOUSEBUTTONDOWN);
                
                switch (event.button.button) {
                    case SDL_BUTTON_LEFT: qkey = K_MOUSE1; break;
                    case SDL_BUTTON_MIDDLE: qkey = K_MOUSE3; break;
                    case SDL_BUTTON_RIGHT: qkey = K_MOUSE2; break;
                    case SDL_BUTTON_X1: qkey = K_MOUSE4; break;
                    case SDL_BUTTON_X2: qkey = K_MOUSE5; break;
                }
                
                if (qkey) {
                    CL_KeyEvent(qkey, down, 0);
                }
                break;
            }
            
            case SDL_MOUSEWHEEL:
                if (event.wheel.y > 0) {
                    CL_KeyEvent(K_MWHEELUP, qtrue, 0);
                    CL_KeyEvent(K_MWHEELUP, qfalse, 0);
                } else if (event.wheel.y < 0) {
                    CL_KeyEvent(K_MWHEELDOWN, qtrue, 0);
                    CL_KeyEvent(K_MWHEELDOWN, qfalse, 0);
                }
                break;
                
            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                    // Release all mouse grabs when focus lost
                    SDL_CaptureMouse(SDL_FALSE);
                    SDL_SetWindowMouseGrab(window, SDL_FALSE);
                    SDL_SetWindowGrab(window, SDL_FALSE);
                    SDL_SetRelativeMouseMode(SDL_FALSE);
                    SDL_ShowCursor(SDL_ENABLE);
                    mouseActive = qfalse;
                } else if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
                    SDL_ShowCursor(SDL_DISABLE);
                    if (cls.state == CA_ACTIVE && !cls.keyCatchers) {
                        IN_ActivateMouse();
                    }
                }
                break;
        }
    }
}

/* ============================================================================
                                SOUND
   ============================================================================ */

// SDL Audio callback - fills audio buffer from dma.buffer
// Uses a simple ring buffer approach
static volatile int sdl_audioPos = 0;

static void SDLCALL SNDDMA_AudioCallback(void *userdata, Uint8 *stream, int len) {
    int bytesPerSample;
    int bytesToCopy;
    int remaining;
    int pos;
    
    if (!dma.buffer) {
        memset(stream, 0, len);
        return;
    }
    
    bytesPerSample = dma.samplebits / 8;
    pos = sdl_audioPos;
    remaining = len;
    
    while (remaining > 0) {
        int bytesAvailable = (dma.samples - pos) * bytesPerSample;
        bytesToCopy = remaining;
        if (bytesToCopy > bytesAvailable) {
            bytesToCopy = bytesAvailable;
        }
        
        memcpy(stream, (byte *)dma.buffer + pos * bytesPerSample, bytesToCopy);
        
        stream += bytesToCopy;
        remaining -= bytesToCopy;
        pos = (pos + bytesToCopy / bytesPerSample) % dma.samples;
    }
    
    sdl_audioPos = pos;
}

/*
===============
SNDDMA_Init
===============
*/
qboolean SNDDMA_Init( void ) {
    SDL_AudioSpec desired, obtained;
    int freq;
    
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        Com_Printf("SDL audio init failed: %s\n", SDL_GetError());
        return qfalse;
    }
    
    memset(&desired, 0, sizeof(desired));
    
    freq = Cvar_Get("s_khz", "22", CVAR_ARCHIVE)->integer * 1000;
    if (freq <= 11025) {
        freq = 11025;
    } else if (freq <= 22050) {
        freq = 22050;
    } else {
        freq = 44100;
    }
    
    desired.freq = freq;
    desired.format = AUDIO_S16LSB;
    desired.channels = 2;
    desired.samples = 512;
    desired.callback = SNDDMA_AudioCallback;
    desired.userdata = NULL;
    
    audioDevice = SDL_OpenAudioDevice(NULL, 0, &desired, &obtained, 0);
    if (audioDevice == 0) {
        Com_Printf("SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
        return qfalse;
    }
    
    dma.channels = obtained.channels;
    dma.samplebits = 16;
    dma.speed = obtained.freq;
    // Buffer size: 4 times the SDL buffer size (gives us some headroom)
    // dma.samples is total samples across all channels
    dma.samples = obtained.samples * obtained.channels * 4;
    dma.submission_chunk = 1;
    dma.buffer = Z_Malloc(dma.samples * (dma.samplebits / 8));
    dma.samplepos = 0;
    sdl_audioPos = 0;
    
    if (!dma.buffer) {
        Com_Printf("Failed to allocate sound buffer\n");
        return qfalse;
    }
    
    memset(dma.buffer, 0, dma.samples * (dma.samplebits / 8));
    
    SDL_PauseAudioDevice(audioDevice, 0);
    
    Com_Printf("SDL2 audio initialized: %d Hz, %d bit, %d channels\n",
               dma.speed, dma.samplebits, dma.channels);
    
    return qtrue;
}

/*
===============
SNDDMA_Shutdown
===============
*/
void SNDDMA_Shutdown( void ) {
    if (audioDevice) {
        SDL_CloseAudioDevice(audioDevice);
        audioDevice = 0;
    }
    
    if (dma.buffer) {
        Z_Free(dma.buffer);
        dma.buffer = NULL;
    }
    
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

/*
===============
SNDDMA_BeginPainting
===============
*/
void SNDDMA_BeginPainting( void ) {
    SDL_LockAudioDevice(audioDevice);
}

/*
===============
SNDDMA_Submit
===============
*/
void SNDDMA_Submit( void ) {
    SDL_UnlockAudioDevice(audioDevice);
}

/* ============================================================================
                                JOYSTICK
   ============================================================================ */

/*
===============
IN_StartupJoystick
===============
*/
void IN_StartupJoystick( void ) {
    int i;
    
    if (!SDL_WasInit(SDL_INIT_JOYSTICK)) {
        if (SDL_Init(SDL_INIT_JOYSTICK) < 0) {
            Com_Printf("SDL joystick init failed: %s\n", SDL_GetError());
            return;
        }
    }
    
    int numJoysticks = SDL_NumJoysticks();
    
    if (numJoysticks == 0) {
        Com_Printf("No joysticks found\n");
        return;
    }
    
    for (i = 0; i < numJoysticks; i++) {
        stick = SDL_JoystickOpen(i);
        if (stick) {
            Com_Printf("Joystick %d opened: %s\n", i, SDL_JoystickName(stick));
            break;
        }
    }
}

/*
===============
IN_ShutdownJoystick
===============
*/
void IN_ShutdownJoystick( void ) {
    if (stick) {
        SDL_JoystickClose(stick);
        stick = NULL;
    }
}

/*
===============
IN_JoystickMove
===============
*/
void IN_JoystickMove( void ) {
    if (!stick) return;
}

/*
===============
Sys_SendKeyEvents
===============
*/
void Sys_SendKeyEvents( void ) {
    IN_ProcessEvents();
}

/*
===============
GLimp_EndFrame
===============
*/
void GLimp_EndFrame( void ) {
    GLimp_SwapBuffers();
}

/*
===============
SMP/Render thread stubs
===============
*/
qboolean GLimp_SpawnRenderThread( void (*function)(void) ) {
    return qfalse;
}

void *GLimp_RendererSleep( void ) {
    return NULL;
}

void GLimp_FrontEndSleep( void ) {
}

void GLimp_WakeRenderer( void *data ) {
}

/*
===============
SNDDMA_GetDMAPos
===============

Returns the current playback position in the DMA buffer.
For SDL audio, this is the position the callback has read up to.
*/
int SNDDMA_GetDMAPos( void ) {
    return sdl_audioPos;
}

/*
===============
Sys_SnapVector
===============
*/
void Sys_SnapVector( float *v ) {
    // No-op for x64
}
