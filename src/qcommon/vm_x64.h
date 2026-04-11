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
 * @file vm_x64.h
 * @brief Header file for native x64 VM implementation
 * 
 * This header provides declarations for the x64-native VM system that
 * replaces bytecode interpretation with direct native function calls.
 * All pointer conversions use proper 64-bit types to avoid truncation.
 */

#ifndef __VM_X64_H__
#define __VM_X64_H__

#include "../game/q_shared.h"
#include <stdint.h>
#include <stddef.h>

// Architecture detection
#if defined(__x86_64__) || defined(_M_X64) || defined(__amd64__)
    #define VM_ARCH_X64 1
    #define VM_NATIVE_SUPPORT 1
#else
    #error "vm_x64.h requires x64 architecture"
#endif

/**
 * @brief Maximum number of VM modules supported
 */
#ifndef MAX_VM
    #define MAX_VM 16
#endif

/**
 * @brief Forward declaration of VM structure
 */
typedef struct vm_s vm_t;

/**
 * @brief Initialize the native x64 VM system
 * 
 * Call this once at startup before loading any modules.
 */
void VM_Native_Init(void);

/**
 * @brief Shutdown the native x64 VM system
 * 
 * Unloads all loaded modules and cleans up resources.
 */
void VM_Native_Shutdown(void);

/**
 * @brief Load a native module from shared library
 * 
 * @param vmNumber VM slot number (0 to MAX_VM-1)
 * @param name Module name without extension (e.g., "game", "cgame", "ui")
 * @return qtrue on success, qfalse on failure
 */
qboolean VM_Native_LoadModule(int vmNumber, const char* name);

/**
 * @brief Call a function in a loaded native module
 * 
 * @param vmNumber VM slot number
 * @param command Command ID to execute
 * @param args Array of arguments (intptr_t for proper 64-bit handling)
 * @return Result from the module function
 */
intptr_t VM_Native_Call(int vmNumber, int command, intptr_t* args);

/**
 * @brief Unload a specific native module
 * 
 * @param vmNumber VM slot number
 */
void VM_Native_UnloadModule(int vmNumber);

/**
 * @brief Get information about a loaded module (for debugging)
 * 
 * @param vmNumber VM slot number
 * @return Pointer to module info or NULL if not loaded
 */
const void* VM_Native_GetInfo(int vmNumber);

/**
 * @brief Validate pointer alignment for x64
 * 
 * Ensures pointers are properly aligned for their data size to
 * prevent alignment faults on x64.
 * 
 * @param ptr Pointer to validate
 * @param size Size of data being accessed
 * @return qtrue if properly aligned, qfalse otherwise
 */
qboolean VM_ValidatePointerAlignment(const void* ptr, size_t size);

/**
 * @brief Safe pointer-to-integer conversion for x64
 * 
 * These inline functions ensure proper conversion between pointers
 * and integers without compiler warnings or data loss.
 */

static ID_INLINE uintptr_t VM_PtrToUint(const void* ptr) {
    return (uintptr_t)ptr;
}

static ID_INLINE intptr_t VM_PtrToInt(const void* ptr) {
    return (intptr_t)ptr;
}

static ID_INLINE void* VM_UintToPtr(uintptr_t num) {
    return (void*)num;
}

static ID_INLINE void* VM_IntToPtr(intptr_t num) {
    return (void*)num;
}

/**
 * @brief Field offset macro safe for x64
 * 
 * Replaces the old FOFS macro that cast to int (32-bit).
 * Uses intptr_t to preserve full 64-bit offsets.
 */
#define FOFS_X64(struct_type, field) \
    ((intptr_t)&(((struct_type *)0)->field))

/**
 * @brief Assembly helper functions (defined in vm_x64_asm.S)
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Fast native call with optimized x64 calling convention
 * 
 * @param func Function pointer to call
 * @param command First argument (command ID)
 * @param args Second argument (argument array)
 * @return Result from function call
 */
extern intptr_t VM_Native_CallFast(
    intptr_t (*func)(intptr_t, intptr_t*),
    intptr_t command,
    intptr_t* args
);

/**
 * @brief Extract argument from array with proper alignment
 * 
 * @param args Argument array
 * @param index Argument index
 * @return Argument value
 */
extern intptr_t VM_GetArg(intptr_t* args, int index);

/**
 * @brief Set argument in array with proper alignment
 * 
 * @param args Argument array
 * @param index Argument index
 * @param value Value to store
 */
extern void VM_SetArg(intptr_t* args, int index, intptr_t value);

/**
 * @brief Memory barrier for x64
 * 
 * Ensures all memory operations before this point complete
 * before any after this point begin.
 */
extern void VM_MemoryBarrier(void);

/**
 * @brief Atomic compare-and-swap
 * 
 * @param ptr Pointer to value
 * @param expected Expected value
 * @param new_value New value to store
 * @return 1 if swap succeeded, 0 if failed
 */
extern int VM_AtomicCAS(int64_t* ptr, int64_t expected, int64_t new_value);

/**
 * @brief Atomic increment
 * 
 * @param ptr Pointer to value
 * @return Old value before increment
 */
extern int64_t VM_AtomicInc(int64_t* ptr);

/**
 * @brief Validate pointer alignment
 * 
 * @param ptr Pointer to validate
 * @param alignment Required alignment (power of 2)
 * @return 1 if aligned, 0 if not
 */
extern int VM_ValidateAlignment(const void* ptr, size_t alignment);

#ifdef __cplusplus
}
#endif

/**
 * @brief Debug macros for pointer validation
 */
#ifdef _DEBUG
    #define VM_CHECK_PTR_ALIGNMENT(ptr, size) \
        if (!VM_ValidatePointerAlignment(ptr, size)) { \
            Com_Printf(S_COLOR_YELLOW "WARNING: Misaligned pointer %p for size %zu\n", ptr, size); \
        }
#else
    #define VM_CHECK_PTR_ALIGNMENT(ptr, size) ((void)0)
#endif

/**
 * @brief Helper macro for safe field offset calculation
 * 
 * Usage: 
 *   intptr_t offset = VM_FOFS(gentity_t, s);
 */
#define VM_FOFS(type, field) FOFS_X64(type, field)

#endif // __VM_X64_H__
