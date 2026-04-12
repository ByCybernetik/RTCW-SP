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

// vm.c -- virtual machine (NATIVE DLL ONLY - QVM support removed for modern systems)
// This file provides stub functions for backward compatibility. All game modules
// now load as native DLLs via Sys_LoadDll instead of QVM bytecode.

#include "vm_local.h"

// VM система удалена - используются только нативные DLL
// Этот файл оставлен для совместимости с существующим кодом

vm_t    *currentVM = NULL;
vm_t    *lastVM    = NULL;
int vm_debugLevel;

#define MAX_VM      3
vm_t vmTable[MAX_VM];

void VM_VmInfo_f( void ) {}
void VM_VmProfile_f( void ) {}

void *VM_VM2C( vmptr_t p, int length ) {
return (void *)p;
}

void VM_Debug( int level ) {
vm_debugLevel = level;
}

/*
==============
VM_Init
==============
*/
void VM_Init( void ) {
// QVM переменные больше не используются, но оставляем для совместимости
Cvar_Get( "vm_cgame", "0", CVAR_ARCHIVE | CVAR_ROM );
Cvar_Get( "vm_game",  "0", CVAR_ARCHIVE | CVAR_ROM );
Cvar_Get( "vm_ui",    "0", CVAR_ARCHIVE | CVAR_ROM );

Com_Printf( "VM system initialized (NATIVE DLL only, QVM support removed)\n" );
}

/*
=================
VM_DllSyscall

Dlls will call this directly - now just passes through to currentVM
=================
*/
int QDECL VM_DllSyscall( int arg, ... ) {
#if ( ( defined __linux__ ) && ( defined __powerpc__ ) )
int args[16];
int i;
va_list ap;

args[0] = arg;

va_start( ap, arg );
for ( i = 1; i < sizeof( args ) / sizeof( args[i] ); i++ )
args[i] = va_arg( ap, int );
va_end( ap );

return currentVM->systemCall( args );
#else // original id code
return currentVM->systemCall( &arg );
#endif
}

/*
=================
VM_Restart

Reload the data, but leave everything else in place
For native DLLs, this means unload and reload the DLL
=================
*/
vm_t *VM_Restart( vm_t *vm ) {
char name[MAX_QPATH];
int ( *systemCall )( int *parms );

if ( !vm || !vm->name[0] ) {
return NULL;
}

systemCall = vm->systemCall;
Q_strncpyz( name, vm->name, sizeof( name ) );

VM_Free( vm );

// Reload as native DLL
vm = VM_Create( name, systemCall, VMI_NATIVE );
return vm;
}

/*
================
VM_Create

Load module as native DLL only (QVM support removed)
================
*/
vm_t *VM_Create( const char *module, int ( *systemCalls )(int *), vmInterpret_t interpret ) {
vm_t        *vm;
int i;

if ( !module || !module[0] || !systemCalls ) {
Com_Error( ERR_FATAL, "VM_Create: bad parms" );
}

// Find existing VM
for ( i = 0 ; i < MAX_VM ; i++ ) {
if ( !Q_stricmp( vmTable[i].name, module ) ) {
vm = &vmTable[i];
return vm;
}
}

// Find free slot
for ( i = 0 ; i < MAX_VM ; i++ ) {
if ( !vmTable[i].name[0] ) {
break;
}
}

if ( i == MAX_VM ) {
Com_Error( ERR_FATAL, "VM_Create: no free vm_t" );
}

vm = &vmTable[i];

Q_strncpyz( vm->name, module, sizeof( vm->name ) );
vm->systemCall = systemCalls;

// Always load as native DLL (QVM support removed)
vm->dllHandle = Sys_LoadDll( module, &vm->entryPoint, VM_DllSyscall );
if ( vm->dllHandle ) {
Com_Printf( "Loaded native DLL: %s\n", module );
return vm;
}

Com_Error( ERR_FATAL, "Failed to load native DLL: %s", module );
return NULL;
}

/*
==============
VM_Free
==============
*/
void VM_Free( vm_t *vm ) {
if ( !vm ) {
return;
}

if ( vm->dllHandle ) {
Sys_UnloadDll( vm->dllHandle );
}

Com_Memset( vm, 0, sizeof( *vm ) );

currentVM = NULL;
lastVM = NULL;
}

void VM_Clear( void ) {
int i;
for ( i = 0; i < MAX_VM; i++ ) {
if ( vmTable[i].dllHandle ) {
Sys_UnloadDll( vmTable[i].dllHandle );
}
Com_Memset( &vmTable[i], 0, sizeof( vm_t ) );
}
currentVM = NULL;
lastVM = NULL;
}

/*
==============
VM_Call
==============
*/
int QDECL VM_Call( vm_t *vm, int callNum, ... ) {
int args[12];
int i;
va_list ap;

if ( !vm || !vm->entryPoint ) {
Com_Error( ERR_FATAL, "VM_Call: NULL vm or entryPoint" );
}

if ( lastVM != vm ) {
lastVM = vm;
}

currentVM = vm;

va_start( ap, callNum );
for ( i = 0; i < 12; i++ ) {
args[i] = va_arg( ap, int );
}
va_end( ap );

args[0] = callNum;
return vm->entryPoint( 12, args );
}

/*
==============
VM_ArgPtr
==============
*/
void *VM_ArgPtr( int intValue ) {
return (void *)intValue;
}

/*
==============
VM_ExplicitArgPtr
==============
*/
void *VM_ExplicitArgPtr( vm_t *vm, int intValue ) {
return (void *)intValue;
}
