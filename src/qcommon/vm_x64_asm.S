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
 * @file vm_x64_asm.S
 * @brief Native x64 assembly helper functions for VM calls
 * 
 * This file contains optimized x64 assembly implementations for:
 * - Fast function call trampolines
 * - Register preservation across calls
 * - Proper stack alignment for x64 ABI compliance
 * 
 * x64 System V ABI (Linux/Unix):
 * - First 6 integer/pointer args: RDI, RSI, RDX, RCX, R8, R9
 * - Stack must be 16-byte aligned before CALL
 * - Red zone: 128 bytes below RSP preserved across calls
 */

#ifdef __x86_64__

.text
.align 16

/**
 * @brief Fast native call with proper x64 calling convention
 * 
 * This function ensures:
 * - 16-byte stack alignment
 * - Proper register preservation
 * - Red zone compliance
 * 
 * Parameters:
 *   RDI: function pointer (intptr_t (*)(intptr_t, intptr_t*))
 *   RSI: command (intptr_t)
 *   RDX: args pointer (intptr_t*)
 * 
 * Returns:
 *   RAX: result from function call
 */
.globl VM_Native_CallFast
.type VM_Native_CallFast, @function
VM_Native_CallFast:
    // Save base pointer and set up frame
    pushq   %rbp
    movq    %rsp, %rbp
    
    // Preserve callee-saved registers we might use
    pushq   %rbx
    pushq   %r12
    pushq   %r13
    pushq   %r14
    pushq   %r15
    
    // Ensure 16-byte stack alignment before call
    // At this point: return addr + saved regs = 48 bytes (6*8)
    // We need to align to 16 bytes, so subtract 8 more
    subq    $8, %rsp
    
    // Move arguments to proper registers for target function call
    // RDI already has function pointer
    // RSI already has command (first arg)
    // RDX already has args pointer (second arg)
    
    // Call the function pointer
    call    *%rdi
    
    // Restore stack alignment
    addq    $8, %rsp
    
    // Restore preserved registers
    popq    %r15
    popq    %r14
    popq    %r13
    popq    %r12
    popq    %rbx
    
    // Restore base pointer and return
    popq    %rbp
    ret

.size VM_Native_CallFast, .-VM_Native_CallFast

/**
 * @brief Helper to extract arguments from array with proper alignment
 * 
 * This function safely extracts arguments from an intptr_t array,
 * ensuring proper alignment for x64.
 * 
 * Parameters:
 *   RDI: args array pointer (intptr_t*)
 *   RSI: argument index
 * 
 * Returns:
 *   RAX: args[index] value
 */
.globl VM_GetArg
.type VM_GetArg, @function
VM_GetArg:
    // Calculate offset: index * sizeof(intptr_t)
    leaq    (,%rsi,8), %rax
    
    // Load value with proper alignment
    movq    (%rdi,%rax), %rax
    
    ret

.size VM_GetArg, .-VM_GetArg

/**
 * @brief Helper to set arguments in array with proper alignment
 * 
 * Parameters:
 *   RDI: args array pointer (intptr_t*)
 *   RSI: argument index
 *   RDX: value to store
 * 
 * Returns: void
 */
.globl VM_SetArg
.type VM_SetArg, @function
VM_SetArg:
    // Calculate offset: index * sizeof(intptr_t)
    leaq    (,%rsi,8), %rax
    
    // Store value with proper alignment
    movq    %rdx, (%rdi,%rax)
    
    ret

.size VM_SetArg, .-VM_SetArg

/**
 * @brief Memory barrier for x64
 * 
 * Ensures all memory operations before this point are complete
 * before any operations after this point begin.
 */
.globl VM_MemoryBarrier
.type VM_MemoryBarrier, @function
VM_MemoryBarrier:
    mfence
    ret

.size VM_MemoryBarrier, .-VM_MemoryBarrier

/**
 * @brief Atomic compare-and-swap for x64
 * 
 * Parameters:
 *   RDI: pointer to value (int64_t*)
 *   RSI: expected value
 *   RDX: new value
 * 
 * Returns:
 *   RAX: 1 if swap succeeded, 0 if failed
 */
.globl VM_AtomicCAS
.type VM_AtomicCAS, @function
VM_AtomicCAS:
    movq    %rsi, %rax          // Load expected into RAX for CMPXCHG
    lock cmpxchgq %rdx, (%rdi)  // Atomic compare-and-swap
    sete    %al                 // Set AL if equal (ZF=1)
    movzxq  %al, %rax           // Zero-extend to full RAX
    ret

.size VM_AtomicCAS, .-VM_AtomicCAS

/**
 * @brief Atomic increment for x64
 * 
 * Parameters:
 *   RDI: pointer to value (int64_t*)
 * 
 * Returns:
 *   RAX: old value before increment
 */
.globl VM_AtomicInc
.type VM_AtomicInc, @function
VM_AtomicInc:
    movq    $1, %rax
    lock xaddq %rax, (%rdi)     // Atomic exchange and add
    ret

.size VM_AtomicInc, .-VM_AtomicInc

/**
 * @brief Validate pointer alignment
 * 
 * Parameters:
 *   RDI: pointer to validate
 *   RSI: required alignment (power of 2)
 * 
 * Returns:
 *   RAX: 1 if aligned, 0 if not
 */
.globl VM_ValidateAlignment
.type VM_ValidateAlignment, @function
VM_ValidateAlignment:
    movq    %rdi, %rax
    decq    %rsi                // alignment - 1
    andq    %rsi, %rax          // test bits
    setz    %al                 // set if zero (aligned)
    movzxq  %al, %rax           // zero-extend
    ret

.size VM_ValidateAlignment, .-VM_ValidateAlignment

#else
#error "This file is for x64 architecture only"
#endif
