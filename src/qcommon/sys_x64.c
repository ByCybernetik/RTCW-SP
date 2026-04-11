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
 * @file sys_x64.c
 * @brief x64-specific system functions - replaces unix_main.c assembly code
 * 
 * This file provides x64-native implementations of system functions that
 * previously used x86 assembly. All pointer conversions use proper 64-bit
 * types (uintptr_t/intptr_t) to avoid truncation on x64 platforms.
 */

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

// Architecture check
#if defined(__x86_64__) || defined(_M_X64) || defined(__amd64__)
    #define ARCH_X64 1
#else
    #error "This file is for x64 architecture only"
#endif

/**
 * @brief Get high-resolution time in milliseconds
 * 
 * Uses clock_gettime() for accurate timing on modern Linux systems.
 * Replaces old gettimeoftaday() which had lower resolution.
 * 
 * @return Current time in milliseconds since epoch
 */
int64_t Sys_Milliseconds(void) {
    struct timespec ts;
    
    #ifdef CLOCK_MONOTONIC
        clock_gettime(CLOCK_MONOTONIC, &ts);
    #else
        clock_gettime(CLOCK_REALTIME, &ts);
    #endif
    
    return (int64_t)ts.tv_sec * 1000 + (int64_t)ts.tv_nsec / 1000000;
}

/**
 * @brief Get high-resolution time in microseconds
 * 
 * @return Current time in microseconds since epoch
 */
int64_t Sys_Microseconds(void) {
    struct timespec ts;
    
    #ifdef CLOCK_MONOTONIC
        clock_gettime(CLOCK_MONOTONIC, &ts);
    #else
        clock_gettime(CLOCK_REALTIME, &ts);
    #endif
    
    return (int64_t)ts.tv_sec * 1000000 + (int64_t)ts.tv_nsec / 1000;
}

/**
 * @brief Get CPU frequency in Hz
 * 
 * Attempts to read from /proc/cpuinfo on Linux.
 * Falls back to estimation if unavailable.
 * 
 * @return CPU frequency in Hz, or 0 if unknown
 */
uint64_t Sys_CPUFrequency(void) {
    static uint64_t cpuFreq = 0;
    
    if (cpuFreq != 0) {
        return cpuFreq;
    }
    
    #ifdef __linux__
    {
        FILE* f = fopen("/proc/cpuinfo", "r");
        if (f) {
            char line[256];
            while (fgets(line, sizeof(line), f)) {
                if (strncmp(line, "cpu MHz", 7) == 0) {
                    double mhz;
                    if (sscanf(line, "cpu MHz : %lf", &mhz) == 1) {
                        cpuFreq = (uint64_t)(mhz * 1000000.0);
                        fclose(f);
                        return cpuFreq;
                    }
                }
            }
            fclose(f);
        }
    }
    #endif
    
    // Fallback: assume 2 GHz
    cpuFreq = 2000000000ULL;
    return cpuFreq;
}

/**
 * @brief Sleep for specified milliseconds
 * 
 * Uses nanosleep for precise sleep on modern Linux.
 * 
 * @param msec Milliseconds to sleep
 */
void Sys_Sleep(int msec) {
    struct timespec req, rem;
    
    req.tv_sec = msec / 1000;
    req.tv_nsec = (msec % 1000) * 1000000L;
    
    while (nanosleep(&req, &rem) == -1 && errno == EINTR) {
        req = rem;
    }
}

/**
 * @brief Yield CPU to other threads
 * 
 * Uses sched_yield on POSIX systems.
 */
void Sys_Yield(void) {
    #ifdef _POSIX_PRIORITY_SCHEDULING
        sched_yield();
    #else
        usleep(1000); // Fallback: sleep for 1ms
    #endif
}

/**
 * @brief Get current thread ID
 * 
 * Returns platform-specific thread identifier.
 * 
 * @return Thread ID
 */
uintptr_t Sys_CurrentThreadID(void) {
    #ifdef __linux__
        return (uintptr_t)syscall(SYS_gettid);
    #elif defined(_WIN32)
        return (uintptr_t)GetCurrentThreadId();
    #else
        return (uintptr_t)pthread_self();
    #endif
}

/**
 * @brief Memory barrier implementation for x64
 * 
 * Ensures all memory operations before this point complete
 * before any after this point begin. Uses compiler barriers
 * and hardware memory fence on x64.
 */
void Sys_MemoryBarrier(void) {
    #if defined(__GNUC__) || defined(__clang__)
        __sync_synchronize();
    #elif defined(_MSC_VER)
        _mm_mfence();
    #else
        // Portable fallback
        volatile int barrier = 0;
        (void)barrier;
    #endif
}

/**
 * @brief Atomic increment for 64-bit integers
 * 
 * Thread-safe increment operation.
 * 
 * @param ptr Pointer to value to increment
 * @return Old value before increment
 */
int64_t Sys_AtomicInc64(volatile int64_t* ptr) {
    #if defined(__GNUC__) || defined(__clang__)
        return __sync_fetch_and_add(ptr, 1);
    #elif defined(_MSC_VER)
        return _InterlockedExchangeAdd64((volatile long long*)ptr, 1);
    #else
        // Non-atomic fallback (not thread-safe!)
        int64_t old = *ptr;
        (*ptr)++;
        return old;
    #endif
}

/**
 * @brief Atomic compare-and-swap for 64-bit integers
 * 
 * Thread-safe CAS operation.
 * 
 * @param ptr Pointer to value
 * @param expected Expected value
 * @param desired New value to store
 * @return qtrue if swap succeeded, qfalse otherwise
 */
qboolean Sys_AtomicCAS64(volatile int64_t* ptr, int64_t expected, int64_t desired) {
    #if defined(__GNUC__) || defined(__clang__)
        return __sync_bool_compare_and_swap(ptr, expected, desired);
    #elif defined(_MSC_VER)
        return _InterlockedCompareExchange64(
            (volatile long long*)ptr, 
            desired, 
            expected
        ) == expected;
    #else
        // Non-atomic fallback (not thread-safe!)
        if (*ptr == expected) {
            *ptr = desired;
            return qtrue;
        }
        return qfalse;
    #endif
}

/**
 * @brief Safe pointer-to-integer conversion
 * 
 * Converts pointer to integer without truncation on x64.
 * 
 * @param ptr Pointer to convert
 * @return Integer representation of pointer
 */
QINLINE uintptr_t Sys_PtrToUint(const void* ptr) {
    return (uintptr_t)ptr;
}

/**
 * @brief Safe integer-to-pointer conversion
 * 
 * Converts integer to pointer without issues on x64.
 * 
 * @param num Integer to convert
 * @return Pointer representation
 */
QINLINE void* Sys_UintToPtr(uintptr_t num) {
    return (void*)num;
}

/**
 * @brief Validate stack alignment
 * 
 * Checks if current stack is properly aligned for x64 (16-byte).
 * 
 * @return qtrue if aligned, qfalse otherwise
 */
qboolean Sys_CheckStackAlignment(void) {
    uintptr_t stackAddr = (uintptr_t)&stackAddr;
    return (stackAddr & 15) == 0;
}

/**
 * @brief Get stack pointer value (for debugging)
 * 
 * @return Current stack pointer address
 */
uintptr_t Sys_GetStackPointer(void) {
    uintptr_t sp;
    #if defined(__GNUC__) || defined(__clang__)
        __asm__ volatile ("movq %%rsp, %0" : "=r"(sp));
    #else
        // Fallback: use local variable address
        volatile int local;
        sp = (uintptr_t)&local;
    #endif
    return sp;
}

/**
 * @brief Error handler for SIGSEGV/SIGBUS
 * 
 * Catches segmentation faults and bus errors, providing
 * useful debugging information.
 * 
 * @param sig Signal number
 */
static void Sys_SignalHandler(int sig) {
    const char* signame;
    
    switch (sig) {
        case SIGSEGV: signame = "SIGSEGV"; break;
        case SIGBUS:  signame = "SIGBUS";  break;
        case SIGFPE:  signame = "SIGFPE";  break;
        case SIGILL:  signame = "SIGILL";  break;
        default:      signame = "UNKNOWN"; break;
    }
    
    Com_Printf(S_COLOR_RED "\n=================================\n");
    Com_Printf(S_COLOR_RED "FATAL ERROR: %s (signal %d)\n", signame, sig);
    Com_Printf(S_COLOR_RED "Stack pointer: %p\n", (void*)Sys_GetStackPointer());
    Com_Printf(S_COLOR_RED "Stack aligned: %s\n", Sys_CheckStackAlignment() ? "YES" : "NO");
    Com_Printf(S_COLOR_RED "=================================\n\n");
    
    // Try to generate crash dump or exit
    exit(1);
}

/**
 * @brief Install signal handlers for crash detection
 */
void Sys_InstallSignalHandlers(void) {
    struct sigaction sa;
    
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = Sys_SignalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
}

/**
 * @brief Print CPU information
 * 
 * Displays CPU features and capabilities relevant to x64.
 */
void Sys_PrintCPUInfo(void) {
    uint64_t freq = Sys_CPUFrequency();
    
    Com_Printf("CPU Frequency: %.2f MHz\n", freq / 1000000.0);
    Com_Printf("Architecture: x86_64\n");
    Com_Printf("Pointer Size: %zu bytes\n", sizeof(void*));
    Com_Printf("intptr_t Size: %zu bytes\n", sizeof(intptr_t));
    
    #ifdef __SSE2__
        Com_Printf("SSE2: Enabled\n");
    #else
        Com_Printf("SSE2: Disabled\n");
    #endif
    
    #ifdef __SSE3__
        Com_Printf("SSE3: Enabled\n");
    #endif
    
    #ifdef __SSSE3__
        Com_Printf("SSSE3: Enabled\n");
    #endif
    
    #ifdef __SSE4_1__
        Com_Printf("SSE4.1: Enabled\n");
    #endif
    
    #ifdef __AVX__
        Com_Printf("AVX: Enabled\n");
    #endif
    
    #ifdef __AVX2__
        Com_Printf("AVX2: Enabled\n");
    #endif
}
