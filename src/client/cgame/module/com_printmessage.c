#include "../client_recovered.h"
#include "../globals.h"

#include <stdarg.h>
#include <stdint.h>

// Source: uo_cgame_mp_x86.dll 0x3002b2b0..0x3002b2fc
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002b2b0_3002b2fc.mcode
//
// Com_PrintMessage: the low-level variadic print backend that both Com_Printf
// (0x3002b420) and Com_DPrintf (0x3002b470) route their formatted text through.
// It formats (format, ...) into a fixed 1024-byte stack buffer and emits the
// result to the engine console via the cgame print trap (CG_PRINT, id 0).
//
// The .mcode's mechanical name BG_GetVehiclePosOffset is a pure size match
// (win 0x4c == corpus 0x4c) and is REJECTED: there is no vehicle table, no vec3
// read, and no offset arithmetic here. The body is a vsprintf + cgame_syscall(0)
// print emitter. Size is not evidence. The name Com_PrintMessage is proven by the
// call graph — it is the shared backend Com_DPrintf (0x3002b470) invokes as
// Com_PrintMessage("%s", buffer) — and matches its established use across the
// codebase (client_recovered.h).
//
// Structure (proven instruction-by-instruction):
//
//   3002b2b0 SUB ESP,0x404              reserve 0x400-byte buffer + canary slot
//   3002b2b6 MOV EAX,[0x30081650]       \  MSVC /GS prologue: snapshot the
//   3002b2c2 MOV [ESP+0x400],EAX        /  __security_cookie into the canary slot
//   3002b2bb MOV ECX,[ESP+0x408]        ECX = format   (1st stack arg)
//   3002b2c9 LEA EAX,[ESP+0x40c]        EAX = &args    (va_start: 1st vararg slot)
//   3002b2d0 PUSH EAX                   vsprintf arg3 = args (va_list)
//   3002b2d1 PUSH ECX                   vsprintf arg2 = format
//   3002b2d2 LEA EDX,[ESP+0x8]          EDX = &buffer  (0x400 buf at frame+0)
//   3002b2d6 PUSH EDX                   vsprintf arg1 = buffer
//   3002b2d7 CALL 0x3005b538            vsprintf(buffer, format, args)
//   3002b2dc LEA EAX,[ESP+0xc]          EAX = &buffer  (frame+0, 3 args on stack)
//   3002b2e0 PUSH EAX                   cgame_syscall arg1 = buffer
//   3002b2e1 PUSH 0x0                   cgame_syscall command = CG_PRINT (id 0)
//   3002b2e3 CALL [0x30085e9c]          cgame_syscall(CG_PRINT, buffer)
//   3002b2e9 MOV ECX,[ESP+0x414]        \  MSVC /GS epilogue: reload canary and
//   3002b2f0 CALL 0x30061639           /  verify it via __security_check_cookie
//   3002b2f5 ADD ESP,0x418             release frame + both calls' pushed dwords
//   3002b2fb RET                        void, caller-cleaned varargs
//
// The buffer size is exactly 0x400 (1024): SUB ESP,0x404 reserves the 0x400 buffer
// at frame offset 0 plus the 4-byte canary slot at frame+0x400. Note the final
// ADD ESP,0x418 releases the 0x404 frame together with the two calls' 5 pushed
// dwords (3 for vsprintf + 2 for cgame_syscall) that were never explicitly
// popped, so this is caller-cleaned throughout (plain RET, cdecl varargs).
//
// The SUB/MOV cookie snapshot (0x3002b2b6/0x3002b2c2) and the reload+check epilogue
// (0x3002b2e9/0x3002b2f0) are compiler-generated MSVC /GS stack-protector code, not
// source statements: they are emitted because this function has a large stack
// buffer. The original C body is just the vsprintf + print below.
void Com_PrintMessage(const char *format, ...)
{
    // 0x3002b2bb..0x3002b2d7: format the message into a fixed 1024-byte buffer.
    // va_start yields &format+1, i.e. the address of the first vararg slot
    // ([ESP+0x40c]), which the machine code passes straight to vsprintf as
    // the va_list argument.
    char buffer[MAX_STRING_CHARS];
    va_list args;

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    va_start(args, format);
    (void)vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    // 0x3002b2dc..0x3002b2e3: emit the formatted text to the engine console via the
    // cgame print trap. The engine's system-call function pointer is installed by
    // dllEntry; PUSH 0x0 is the CG_PRINT command id, PUSH &buffer its single arg.
    cgame_syscall(CG_PRINT, buffer);
}
