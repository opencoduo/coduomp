// Source: uo_cgame_mp_x86.dll 0x3003b880..0x3003b943
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003b880_3003b943.mcode
//
// CG_ShellShockSave — serialize the current cg_shock_* cvar values to a
// shellshock (.shock) parameter file named `name`. This is the write-side
// counterpart to CG_ShellShockLoad (0x3003b950), which lives immediately after
// it and parses such a file back into the same cvars.
//
// Behavior (proven instruction-by-instruction from the .mcode):
//   1. Serialize the 27 cg_shockParamNames cvars into a 0x10000-byte text buffer:
//        result = cgame_syscall(CG_COM_SAVE_CVARS_TO_BUFFER, cg_shockParamNames, 27, buf, 0x10000);
//      If that returns 0, bail returning 0 (0x3003b8b4 JZ fail).
//   2. Open "scripts/<name>.shock" for write:
//        len = cgame_syscall(CG_FS_FOPEN_FILE, va("scripts/%s.shock", name),
//                            &fileHandle, 1 /* FS_WRITE */);
//      If len < 0 (open failed), bail returning 0 (0x3003b8dd JGE success).
//   3. Write strlen(buf) bytes of the serialized text and close:
//        cgame_syscall(CG_FS_WRITE, buf, strlen(buf), fileHandle);
//        cgame_syscall(CG_FS_FCLOSE_FILE, fileHandle);
//      Return 1.
//
// Name adjudication: the .mcode header guesses Script_ExecOnCvarStringValue by a
// 0xc3/0xc4 size match. REJECTED — size matching is disallowed and this function
// does not walk a script or evaluate a cvar-string expression. It serializes the
// shellshock cvar table (cg_shockParamNames, 0x30085dc0) via CG_COM_SAVE_CVARS_TO_BUFFER and writes
// the text to "scripts/%s.shock" (0x3007a404) — the exact inverse of
// CG_ShellShockLoad. The broad-corpus/id-Tech name for that role is
// CG_ShellShockSave. CG_COM_SAVE_CVARS_TO_BUFFER (0x14) is the write-side counterpart to the parse
// trap CG_COM_LOAD_CVARS_FROM_BUFFER (0x15) that CG_ShellShockLoad uses; both take
// (cg_shockParamNames, 27, buffer, ...). CG_FS_WRITE (0x11) matches the server
// bank trap_FS_Write(buffer, length, handle); CG_FS_FOPEN_FILE (0xf) and
// CG_FS_FCLOSE_FILE (0x12) are the same FS traps CG_ShellShockLoad proves.
//
// ABI: int CG_ShellShockSave(const char *name). MOV EAX,0x10008 / CALL __chkstk
// (0x30060a30) reserves the 0x10008-byte frame (a 0x10000 text buffer plus the
// fileHandle out slot and the /GS canary). MOV EAX,[__security_cookie] (0x30081650)
// / MOV [ESP+0x10008],EAX snapshots the cookie; both exits reload it and tail-check
// via __security_check_cookie (0x30061639) before ADD ESP,0x10008 / RET. Those
// chkstk / cookie save+check instructions are compiler-generated MSVC /GS +
// stack-probe code, not source statements, and are omitted from the body (same
// convention as CG_RegisterCvars / CG_ShellShockLoad).
//
// Stack map (relative to ESP after __chkstk, "SP_base"):
//   SP_base+0x0     fileHandle out slot (&fileHandle passed to CG_FS_FOPEN_FILE)
//   SP_base+0x4     buf[0x10000] text buffer (LEA [ESP+8]/[ESP+4]/[ESP+0xc])
//   SP_base+0x10004 /GS canary
//   SP_base+0x10008 return address
//   SP_base+0x1000c name (first argument; read by MOV ECX,[ESP+0x1000c])

#include "../client_recovered.h"
#include "../globals.h"

#include <stdint.h>
#include <string.h>

int CG_ShellShockSave(const char *name)
{
    char buf[65536];   // SP_base+0x4: serialized-text buffer, size 0x10000
    int32_t fileHandle;  // SP_base+0x0: FS_FOpenFileByMode out slot

    // 0x3003b88f..0x3003b8b4: serialize the 27 cg_shock_* cvars into `buf`.
    //   PUSH 0x10000 (bufferSize); LEA &buf; PUSH 27; PUSH cg_shockParamNames; PUSH 0x14
    //   ADD ESP,0x14 (5 dwords); TEST EAX,EAX; JZ fail.
    if (cgame_syscall(CG_COM_SAVE_CVARS_TO_BUFFER,
                      (intptr_t)cg_shockParamNames,
                      CG_SHOCK_PARAM_COUNT,
                      (intptr_t)buf,
                      sizeof(buf)) == 0) {
        return 0; // 0x3003b8df XOR EAX,EAX ; RET
    }

    // 0x3003b8b6..0x3003b8dd: open "scripts/<name>.shock" for write (mode 1).
    //   ECX = name (arg0); va("scripts/%s.shock", name); then
    //   cgame_syscall(0xf, va_result, &fileHandle, 1); ADD ESP,0x18 (2 va + 4 syscall
    //   dwords); TEST EAX,EAX; JGE success (i.e. fail when the signed length < 0).
    const char *path = va("scripts/%s.shock", name);
    if (cgame_syscall(CG_FS_FOPEN_FILE,
                      (intptr_t)path,
                      (intptr_t)&fileHandle,
                      FS_WRITE) < 0) {
        return 0; // 0x3003b8df XOR EAX,EAX ; RET
    }

    // 0x3003b8f4..0x3003b90d: length = strlen(buf).
    // The machine code walks buf byte-by-byte (MOV CL,[EAX]; INC EAX; TEST CL,CL;
    // JNZ) from EAX=&buf, with EDX=&buf+1, then SUB EAX,EDX = (NUL+1) - (buf+1) =
    // index of the NUL = strlen(buf).
    // 0x3003b907..0x3003b91b: write strlen(buf) bytes of buf to the open handle.
    //   PUSH fileHandle; PUSH strlen; PUSH &buf; PUSH 0x11; then ADD ESP,0x18.
    cgame_syscall(CG_FS_WRITE,
                  (intptr_t)buf,
                  (int32_t)strlen(buf),
                  fileHandle);

    // 0x3003b91b..0x3003b928: close the handle.
    cgame_syscall(CG_FS_FCLOSE_FILE, fileHandle);

    // 0x3003b932 MOV EAX,0x1 ; RET  => success.
    return 1;
}
