#include "../client_recovered.h"
#include "../globals.h"

#include <stdarg.h>
#include <stdint.h>

// Source: uo_cgame_mp_x86.dll 0x3002b300..0x3002b34c
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002b300_3002b34c.mcode
//
// Com_ErrorMessage: the low-level variadic fatal-error backend. It formats
// (format, ...) into a fixed 1024-byte stack buffer and emits the result to the
// engine via the cgame error trap (CG_ERROR, id 1). It is the byte-for-byte
// sibling of Com_PrintMessage (0x3002b2b0) — the ONLY difference is the pushed
// trap id: Com_PrintMessage pushes 0 (CG_PRINT), this one pushes 1 (CG_ERROR).
// Com_Error(level, ...) is the outer wrapper that routes its formatted
// text through this emitter.
//
// The .mcode's mechanical name MatrixTranspose is a pure size match
// (win 0x4c == corpus 0x4c) and is REJECTED: there is no matrix, no vec3 read,
// and no transpose arithmetic here — the body is a vsprintf + cgame_syscall(1)
// error emitter. Size is not evidence. The name Com_ErrorMessage is the
// established corpus name for this address (client_recovered.h) and is proven by
// the call graph and the CG_ERROR trap id.
//
// Arg shape (proven, NO leading int/level arg before the format):
//   after SUB ESP,0x404 the frame is: buffer[0x400] at frame+0, cookie at +0x400.
//   [ESP+0x408] == original [ESP+4] == 1st stack arg == format.
//   [ESP+0x40c] == original [ESP+8] == 1st vararg slot == va_start(args, format).
// So the signature is exactly void Com_ErrorMessage(const char *format, ...) —
// the integer error level lives one frame up in Com_Error, not here.
//
// Structure (proven instruction-by-instruction):
//
//   3002b300 SUB ESP,0x404              reserve 0x400-byte buffer + canary slot
//   3002b306 MOV EAX,[0x30081650]       \  MSVC /GS prologue: snapshot the
//   3002b312 MOV [ESP+0x400],EAX        /  __security_cookie into the canary slot
//   3002b30b MOV ECX,[ESP+0x408]        ECX = format   (1st stack arg)
//   3002b319 LEA EAX,[ESP+0x40c]        EAX = &args    (va_start: 1st vararg slot)
//   3002b320 PUSH EAX                   vsprintf arg3 = args (va_list)
//   3002b321 PUSH ECX                   vsprintf arg2 = format
//   3002b322 LEA EDX,[ESP+0x8]          EDX = &buffer  (0x400 buf at frame+0)
//   3002b326 PUSH EDX                   vsprintf arg1 = buffer
//   3002b327 CALL 0x3005b538            vsprintf(buffer, format, args)
//   3002b32c LEA EAX,[ESP+0xc]          EAX = &buffer  (frame+0, 3 args on stack)
//   3002b330 PUSH EAX                   cgame_syscall arg1 = buffer
//   3002b331 PUSH 0x1                   cgame_syscall command = CG_ERROR (id 1)
//   3002b333 CALL [0x30085e9c]          cgame_syscall(CG_ERROR, buffer)
//   3002b339 MOV ECX,[ESP+0x414]        \  MSVC /GS epilogue: reload canary and
//   3002b340 CALL 0x30061639           /  verify it via __security_check_cookie
//   3002b345 ADD ESP,0x418             release frame + both calls' pushed dwords
//   3002b34b RET                        void, caller-cleaned varargs
//
// The final ADD ESP,0x418 releases the 0x404 frame plus the two calls' 5 pushed
// dwords (3 for vsprintf + 2 for cgame_syscall) that were never explicitly
// popped, so this is caller-cleaned throughout (plain RET, cdecl varargs).
//
// The SUB/MOV cookie snapshot (0x3002b306/0x3002b312) and the reload+check epilogue
// (0x3002b339/0x3002b340) are compiler-generated MSVC /GS stack-protector code, not
// source statements: they are emitted because this function has a large stack
// buffer. The original C body is just the vsprintf + error trap below.
void Com_ErrorMessage(const char *format, ...)
{
    // 0x3002b30b..0x3002b327: format the message into a fixed 1024-byte buffer.
    // va_start yields &format+1, i.e. the address of the first vararg slot
    // ([ESP+0x40c]), which the machine code passes straight to vsprintf as
    // the va_list argument.
    char buffer[MAX_STRING_CHARS];
    va_list args;

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    va_start(args, format);
    (void)vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    // 0x3002b32c..0x3002b333: emit the formatted text through the engine's error
    // trap. The engine's system-call function pointer is installed by dllEntry;
    // PUSH 0x1 is the CG_ERROR command id, PUSH &buffer its single arg.
    cgame_syscall(CG_ERROR, buffer);
}
