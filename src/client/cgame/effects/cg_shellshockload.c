// Source: uo_cgame_mp_x86.dll 0x3003b950..0x3003ba04
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003b950_3003ba04.mcode
//
// CG_ShellShockLoad — load and apply a shellshock (.shock) parameter definition
// file named `name`. Strict translation of the i386 machine code.
//
// Naming: the .mcode carries the size-guessed bank name CG_DrawSelectedPlayerName
// (matched only by win size 0xb4). REJECTED: this function opens
// "scripts/%s.shock" (global 0x3007a404) and parses it against the 27 cg_shock_*
// cvar names (cg_shockParamNames, 0x30085dc0) — it is the shellshock parameter
// loader, not a player-name drawer. Callees va (0x3004e8a0) and Com_PrintMessage
// (0x3002b2b0) are already reconstructed; the engine services are reached through
// the cgame syscall pointer *0x30085e9c (cgame_syscall).
//
// ABI: the `name` pointer arrives in EAX (the caller does LEA EAX,[name] then a
// plain CALL; the callee's first PUSH EAX forwards it to va with no stack arg).
// The entry `PUSH ECX` reserves one local dword (the FS file-handle out slot);
// EBX/EDI/ESI are the callee-saved registers. Expressed here as plain C.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

int CG_ShellShockLoad(const char *name)
{
    int fileHandle;        // 0x3003b950 PUSH ECX: local out slot at [ESP+0x14]/[ESP+0x24]
    int length;            // EDI: byte length returned by CG_FS_FOPEN_FILE
    char *fileText;        // ESI: temp buffer holding the file text
    int result;            // EDI (reused): CG_COM_LOAD_CVARS_FROM_BUFFER parse result / return value

    // 0x3003b953..0x3003b96a
    //   path = va("scripts/%s.shock", name);
    //   length = cgame_syscall(CG_FS_FOPEN_FILE, path, &fileHandle, FS_READ);
    // EDI = length (MOV EDI,EAX). FS_READ mode is the literal 0 pushed at 0x3003b95e.
    const char *path = va("scripts/%s.shock", name);
    length = (int32_t)cgame_syscall(CG_FS_FOPEN_FILE, (intptr_t)path, (intptr_t)&fileHandle, FS_READ);

    // 0x3003b975 TEST EDI,EDI ; 0x3003b977 JGE 0x3003b98d
    // Signed length < 0 (JGE not taken) => open failed.
    if (length < 0) {
        // 0x3003b979..0x3003b98c
        Com_PrintMessage("^1couldn't open '%s'\n", path);
        return 0; // XOR EAX,EAX ; RET
    }

    // 0x3003b98e..0x3003b997
    //   fileText = cgame_syscall(CG_Z_MALLOC_INTERNAL, length + 1);   // temp alloc
    fileText = (char *)(intptr_t)cgame_syscall(CG_Z_MALLOC_INTERNAL, coduo_int32_from_bits((uint32_t)length + 1u));

    // 0x3003b99f..0x3003b9a8
    //   cgame_syscall(CG_FS_READ, fileText, length, fileHandle);
    // Note the machine code pushes fileHandle from [ESP+0x14] (the out slot),
    // length from EDI, and fileText from ESI.
    cgame_syscall(CG_FS_READ, (intptr_t)fileText, length, fileHandle);

    // 0x3003b9ae MOV byte ptr [ESI + EDI],0x0
    fileText[length] = '\0';

    // 0x3003b9b2..0x3003b9b9
    //   cgame_syscall(CG_FS_FCLOSE_FILE, fileHandle);   // fileHandle from [ESP+0x24]
    cgame_syscall(CG_FS_FCLOSE_FILE, fileHandle);

    // 0x3003b9bf..0x3003b9ca
    //   result = cgame_syscall(CG_COM_LOAD_CVARS_FROM_BUFFER, cg_shockParamNames, 27, fileText, path);
    // EBX still holds `path` here (set at 0x3003b965). EDI = result (MOV EDI,EAX).
    result = (int32_t)cgame_syscall(CG_COM_LOAD_CVARS_FROM_BUFFER, (intptr_t)cg_shockParamNames, CG_SHOCK_PARAM_COUNT, (intptr_t)fileText,
                                    (intptr_t)path);

    // 0x3003b9d0..0x3003b9d8
    //   cgame_syscall(CG_Z_FREE_INTERNAL, fileText);   // free the temp buffer
    cgame_syscall(CG_Z_FREE_INTERNAL, (intptr_t)fileText);

    // 0x3003b9e1..0x3003b9fb
    //   for (i = 0; i < 27; i++) cgame_syscall(CG_CVAR_UPDATE, cg_shockParamTargets[i]);
    // Loop uses a byte index in ESI stepping by 4 up to 0x6c (unsigned JC),
    // i.e. 27 word-sized entries of the target table.
    for (int i = 0; i < CG_SHOCK_PARAM_COUNT; i++) {
        cgame_syscall(CG_CVAR_UPDATE, (intptr_t)cg_shockParamTargets[i]);
    }

    // 0x3003b9fe MOV EAX,EDI ; RET  => return the CG_COM_LOAD_CVARS_FROM_BUFFER parse result.
    return result;
}
