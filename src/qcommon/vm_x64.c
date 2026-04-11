/*
===========================================================================
Copyright (C) 2024 Return to Castle Wolfenstein SDL2/x64 Project

This file is part of the RTCW SDL2/x64 adaptation.

RTCW SDL2/x64 adaptation is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

RTCW SDL2/x64 adaptation is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with RTCW SDL2/x64 adaptation; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

/**
 * @file vm_x64.c
 * @brief Native x64 VM implementation - replaces bytecode interpreter
 * 
 * This module provides direct native function calls for game modules,
 * eliminating the need for bytecode interpretation on x64 platforms.
 * All function pointers use proper 64-bit types (uintptr_t) to avoid
 * pointer truncation issues.
 */

#include "../game/q_shared.h"
#include "qcommon.h"
#include "vm_local.h"
#include "vm_x64.h"
#include <stdint.h>
#include <string.h>
#include <dlfcn.h>

// Define for x64 architecture
#if defined(__x86_64__) || defined(_M_X64) || defined(__amd64__)
    #define ARCH_X64 1
#else
    #error "This file is for x64 architecture only"
#endif

/**
 * @brief Structure to hold native module data
 */
typedef struct {
    void* libraryHandle;        ///< Dynamic library handle
    intptr_t (*entryPoint)(intptr_t, intptr_t*);  ///< Main entry point with proper 64-bit types
    char libraryPath[MAX_QPATH]; ///< Path to loaded library
    qboolean isLoaded;          ///< Library load status
} vmNativeData_t;

static vmNativeData_t vmNativeModules[MAX_VM];

/**
 * @brief Initialize native VM system
 */
void VM_Native_Init(void) {
    memset(vmNativeModules, 0, sizeof(vmNativeModules));
    Com_Printf("Native x64 VM system initialized\n");
}

/**
 * @brief Shutdown native VM system
 */
void VM_Native_Shutdown(void) {
    int i;
    for (i = 0; i < MAX_VM; i++) {
        if (vmNativeModules[i].isLoaded && vmNativeModules[i].libraryHandle) {
            dlclose(vmNativeModules[i].libraryHandle);
            vmNativeModules[i].libraryHandle = NULL;
            vmNativeModules[i].isLoaded = qfalse;
            Com_Printf("Unloaded native module %d\n", i);
        }
    }
}

/**
 * @brief Load a native module from shared library
 * 
 * @param vmNumber VM slot number
 * @param name Module name (game, cgame, ui)
 * @return qtrue on success, qfalse on failure
 */
qboolean VM_Native_LoadModule(int vmNumber, const char* name) {
    char libraryPath[MAX_OSPATH];
    char entryName[64];
    void* handle;
    intptr_t (*entryPoint)(intptr_t, intptr_t*);
    
    if (vmNumber < 0 || vmNumber >= MAX_VM) {
        Com_Printf(S_COLOR_RED "ERROR: Invalid VM number %d\n", vmNumber);
        return qfalse;
    }
    
    // Construct library path based on platform
    #ifdef __linux__
        snprintf(libraryPath, sizeof(libraryPath), "./%s.so", name);
    #elif defined(_WIN32)
        snprintf(libraryPath, sizeof(libraryPath), "%s.dll", name);
    #elif defined(__APPLE__)
        snprintf(libraryPath, sizeof(libraryPath), "./%s.dylib", name);
    #else
        #error "Unsupported platform for native modules"
    #endif
    
    // Check if already loaded
    if (vmNativeModules[vmNumber].isLoaded) {
        Com_Printf("Module %s already loaded in slot %d\n", name, vmNumber);
        return qtrue;
    }
    
    // Load dynamic library
    handle = dlopen(libraryPath, RTLD_LAZY | RTLD_LOCAL);
    if (!handle) {
        Com_Printf(S_COLOR_RED "ERROR: Failed to load %s: %s\n", libraryPath, dlerror());
        return qfalse;
    }
    
    // Get entry point symbol - using proper 64-bit function pointer type
    snprintf(entryName, sizeof(entryName), "vmMain");
    entryPoint = (intptr_t (*)(intptr_t, intptr_t*))dlsym(handle, entryName);
    
    if (!entryPoint) {
        Com_Printf(S_COLOR_RED "ERROR: Failed to find vmMain in %s: %s\n", libraryPath, dlerror());
        dlclose(handle);
        return qfalse;
    }
    
    // Store module data with proper 64-bit pointer handling
    vmNativeModules[vmNumber].libraryHandle = handle;
    vmNativeModules[vmNumber].entryPoint = entryPoint;
    Q_strncpyz(vmNativeModules[vmNumber].libraryPath, libraryPath, sizeof(vmNativeModules[vmNumber].libraryPath));
    vmNativeModules[vmNumber].isLoaded = qtrue;
    
    Com_Printf("Loaded native module %s from %s (entry point: %p)\n", 
               name, libraryPath, (void*)entryPoint);
    
    return qtrue;
}

/**
 * @brief Call a native module function
 * 
 * @param vmNumber VM slot number
 * @param command Command ID to execute
 * @param args Array of arguments (properly aligned for x64)
 * @return Result from module call
 */
intptr_t VM_Native_Call(int vmNumber, int command, intptr_t* args) {
    if (vmNumber < 0 || vmNumber >= MAX_VM) {
        Com_Printf(S_COLOR_RED "ERROR: Invalid VM number %d in call\n", vmNumber);
        return -1;
    }
    
    if (!vmNativeModules[vmNumber].isLoaded || !vmNativeModules[vmNumber].entryPoint) {
        Com_Printf(S_COLOR_RED "ERROR: Module %d not loaded\n", vmNumber);
        return -1;
    }
    
    // Direct native call with proper 64-bit calling convention
    // The first argument (command) is passed as intptr_t
    // The second argument (args) is a pointer to array of intptr_t
    return vmNativeModules[vmNumber].entryPoint((intptr_t)command, args);
}

/**
 * @brief Unload a specific native module
 * 
 * @param vmNumber VM slot number
 */
void VM_Native_UnloadModule(int vmNumber) {
    if (vmNumber < 0 || vmNumber >= MAX_VM) {
        return;
    }
    
    if (vmNativeModules[vmNumber].isLoaded && vmNativeModules[vmNumber].libraryHandle) {
        dlclose(vmNativeModules[vmNumber].libraryHandle);
        vmNativeModules[vmNumber].libraryHandle = NULL;
        vmNativeModules[vmNumber].entryPoint = NULL;
        vmNativeModules[vmNumber].isLoaded = qfalse;
        Com_Printf("Unloaded native module %d\n", vmNumber);
    }
}

/**
 * @brief Get module information for debugging
 * 
 * @param vmNumber VM slot number
 * @return Pointer to module data or NULL if not loaded
 */
const vmNativeData_t* VM_Native_GetInfo(int vmNumber) {
    if (vmNumber < 0 || vmNumber >= MAX_VM) {
        return NULL;
    }
    
    if (!vmNativeModules[vmNumber].isLoaded) {
        return NULL;
    }
    
    return &vmNativeModules[vmNumber];
}

/**
 * @brief Validate pointer alignment for x64
 * 
 * On x64, pointers should be properly aligned. This function helps
 * detect potential alignment issues that could cause crashes.
 * 
 * @param ptr Pointer to validate
 * @param size Size of data being accessed
 * @return qtrue if aligned, qfalse otherwise
 */
qboolean VM_ValidatePointerAlignment(const void* ptr, size_t size) {
    uintptr_t addr = (uintptr_t)ptr;
    
    // Check alignment based on size
    switch (size) {
        case 1:  return qtrue;  // Any address is valid for 1 byte
        case 2:  return (addr & 1) == 0;
        case 4:  return (addr & 3) == 0;
        case 8:  return (addr & 7) == 0;  // Critical for x64
        default: return (addr & 7) == 0;  // Assume 8-byte alignment for larger types
    }
}

/**
 * @brief Convert between pointer and integer safely for x64
 * 
 * These inline functions ensure proper conversion without warnings
 * and make the intent clear in the code.
 */

ID_INLINE uintptr_t VM_PtrToUint(const void* ptr) {
    return (uintptr_t)ptr;
}

ID_INLINE intptr_t VM_PtrToInt(const void* ptr) {
    return (intptr_t)ptr;
}

ID_INLINE void* VM_UintToPtr(uintptr_t num) {
    return (void*)num;
}

ID_INLINE void* VM_IntToPtr(intptr_t num) {
    return (void*)num;
}
