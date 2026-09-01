#include "../client_recovered.h"
#include "../globals.h"

#include <stdarg.h>
#include <stdint.h>

// Source: uo_cgame_mp_x86.dll 0x3002b420..0x3002b46e
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002b420_3002b46e.mcode
//
// Com_Printf: the unconditional variadic diagnostic printer. It formats the
// caller's (format, ...) message into a fixed 1024-byte stack buffer, then emits
// the result verbatim through the print backend Com_PrintMessage("%s", buffer)
// (0x3002b2b0 -> cgame_syscall(CG_PRINT, buffer), trap id 0). Unlike its
// developer-gated twin Com_DPrintf (0x3002b470), which is identical except for a
// leading `developer` cvar gate, Com_Printf always formats and prints.
//
// The .mcode's mechanical name script_func_isplayernumber is a pure size match
// (win 0x4e == PPC 0x4e) and is REJECTED: there is no player-number/script
// behavior here; the body is a vsprintf + print. Size is not evidence. The name
// Com_Printf is proven by the call graph: it is the ungated twin of Com_DPrintf
// (0x3002b470) and shares the same print backend Com_PrintMessage (0x3002b2b0),
// and is already woven through the reconstructed callers as Com_Printf.
//
// Argument boundary (proven, and the discriminant vs the error sibling):
// after SUB ESP,0x404 the incoming args are at [ESP+0x408]=arg0, [ESP+0x40c]=arg1.
// This function reads the format from [ESP+0x408] (arg0) and takes va_start from
// [ESP+0x40c] (arg1, the first vararg) — so its ABI is (const char *format, ...)
// with NO leading level argument. Contrast the error sibling Com_Error
// (0x3002b3d0), whose otherwise-identical body reads the format one slot further
// in ([ESP+0x40c]) and va_starts from [ESP+0x410], i.e. (int level, format, ...).
//
// Structure (proven instruction-by-instruction):
//
//   0x3002b420 SUB ESP,0x404               reserve 0x400-byte buffer + canary slot
//   0x3002b426 MOV EAX,[0x30081650]        \  MSVC /GS prologue: snapshot the
//   0x3002b432 MOV [ESP+0x400],EAX         /  __security_cookie into the canary slot
//   0x3002b42b MOV ECX,[ESP+0x408]         ECX = format   (arg0, 1st stack arg)
//   0x3002b439 LEA EAX,[ESP+0x40c]         EAX = &args    (va_start: 1st vararg)
//   0x3002b440 PUSH EAX                    vsprintf arg3 = args (va_list)
//   0x3002b441 PUSH ECX                    vsprintf arg2 = format
//   0x3002b442 LEA EDX,[ESP+0x8]           EDX = &buffer  (0x400 buf at frame+0)
//   0x3002b446 PUSH EDX                    vsprintf arg1 = buffer
//   0x3002b447 CALL 0x3005b538             vsprintf(buffer, format, args)
//   0x3002b44c LEA EAX,[ESP+0xc]           EAX = &buffer   (frame+0, args on stack)
//   0x3002b450 PUSH EAX                    Com_PrintMessage vararg = buffer
//   0x3002b451 PUSH 0x30076cd8             Com_PrintMessage format = "%s"
//   0x3002b456 CALL 0x3002b2b0             Com_PrintMessage("%s", buffer)
//   0x3002b45b MOV ECX,[ESP+0x414]         \  MSVC /GS epilogue: reload the canary
//   0x3002b462 CALL 0x30061639             /  (5 pushed dwords still on stack) and
//                                             verify it via __security_check_cookie
//   0x3002b467 ADD ESP,0x418               release frame + clean the 5 pushed args
//   0x3002b46d RET                         void, caller-cleaned varargs
//
// The buffer size is exactly 0x400 (1024): SUB ESP,0x404 reserves the 0x400 buffer
// at frame offset 0 plus the 4-byte canary slot at frame+0x400. The print goes
// through the fixed format "%s" (0x30076cd8) so the already-formatted, potentially
// %-bearing buffer is emitted verbatim rather than re-interpreted.
//
// The SUB/MOV cookie snapshot (0x3002b426/0x3002b432) and the reload+check epilogue
// (0x3002b45b/0x3002b462) are compiler-generated MSVC /GS stack-protector code, not
// source statements: the original C body is just the vsprintf + print below. They
// are emitted because this function has a large stack buffer.
void Com_Printf(const char *format, ...)
{
    // 0x3002b42b..0x3002b447: format the message into a fixed 1024-byte buffer.
    // va_start yields &format+1, i.e. the address of the first vararg slot
    // ([ESP+0x40c]), which the machine code passes straight to vsprintf as
    // the va_list argument.
    char buffer[MAX_STRING_CHARS];
    va_list args;

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    va_start(args, format);
    (void)vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    // 0x3002b44c..0x3002b456: emit the formatted text through the print backend
    // with the literal format "%s", so buffer is printed verbatim.
    Com_PrintMessage("%s", buffer);
}
