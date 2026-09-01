#include "../client_recovered.h"
#include "../globals.h"

#include <stdarg.h>
#include <stdint.h>

// Source: uo_cgame_mp_x86.dll 0x3002b3d0..0x3002b41e
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002b3d0_3002b41e.mcode
//
// Com_Error: the cgame variadic fatal-error reporter with the canonical
// Quake3/CoD signature void Com_Error(errorParm_t level, const char *format, ...).
// It formats (format, ...) into a fixed 1024-byte stack buffer via vsprintf,
// then re-emits the finished text through Com_ErrorMessage("%s", buffer)
// (0x3002b300), which issues the engine CG_ERROR trap (id 1). The `level`
// argument is the Q3 error code (ERR_DROP == 1); it is consumed by the
// caller-cleaned ABI but not otherwise touched by this body (the low-level
// emitter always issues CG_ERROR regardless of level).
//
// The .mcode's mechanical name PM_ContinueWeaponAnim is a pure win-size match
// (0x4e == 0x4e) and is REJECTED: there is no weapon/anim state access here, only
// a vararg format followed by a "%s"-relay into the CG_ERROR emitter. Size is not
// evidence. Com_Error is proven by the PPC symbol, call graph (vsprintf +
// Com_ErrorMessage) and its consumers (e.g. Com_ScriptError passing level 1).
//
// Arg shape (proven; a leading int `level` precedes the format, unlike the
// byte-sibling Com_ErrorMessage which has none):
//   after SUB ESP,0x404 the frame is: buffer[0x400] at frame+0, cookie at +0x400.
//   [ESP+0x408] == original [ESP+4] == 1st stack arg == level   (NOT read here).
//   [ESP+0x40c] == original [ESP+8] == 2nd stack arg == format.
//   [ESP+0x410] == original [ESP+0xc] == 1st vararg slot == va_start(args, format).
//
// Structure (proven instruction-by-instruction):
//
//   3002b3d0 SUB ESP,0x404              reserve 0x400-byte buffer + canary slot
//   3002b3d6 MOV EAX,[0x30081650]       \  MSVC /GS prologue: snapshot the
//   3002b3e2 MOV [ESP+0x400],EAX        /  __security_cookie into the canary slot
//   3002b3db MOV ECX,[ESP+0x40c]        ECX = format   (2nd stack arg; skips level)
//   3002b3e9 LEA EAX,[ESP+0x410]        EAX = &args    (va_start: 1st vararg slot)
//   3002b3f0 PUSH EAX                   vsprintf arg3 = args (va_list)
//   3002b3f1 PUSH ECX                   vsprintf arg2 = format
//   3002b3f2 LEA EDX,[ESP+0x8]          EDX = &buffer  (0x400 buf at frame+0)
//   3002b3f6 PUSH EDX                   vsprintf arg1 = buffer
//   3002b3f7 CALL 0x3005b538            vsprintf(buffer, format, args)
//   3002b3fc LEA EAX,[ESP+0xc]          EAX = &buffer  (frame+0, 3 args on stack)
//   3002b400 PUSH EAX                   Com_ErrorMessage arg2 = buffer
//   3002b401 PUSH 0x30076cd8            Com_ErrorMessage arg1 = "%s" (.rdata)
//   3002b406 CALL 0x3002b300            Com_ErrorMessage("%s", buffer)
//   3002b40b MOV ECX,[ESP+0x414]        \  MSVC /GS epilogue: reload canary and
//   3002b412 CALL 0x30061639           /  verify it via __security_check_cookie
//   3002b417 ADD ESP,0x418             release frame + both calls' pushed dwords
//   3002b41d RET                        void, caller-cleaned varargs
//
// The final ADD ESP,0x418 releases the 0x404 frame plus the two calls' 5 pushed
// dwords (3 for vsprintf + 2 for Com_ErrorMessage) that were never explicitly
// popped, so this is caller-cleaned throughout (plain RET, cdecl varargs).
//
// The SUB/MOV cookie snapshot (0x3002b3d6/0x3002b3e2) and the reload+check epilogue
// (0x3002b40b/0x3002b412) are compiler-generated MSVC /GS stack-protector code, not
// source statements: they are emitted because this function has a large stack
// buffer. The "%s" relay through Com_ErrorMessage (rather than a direct CG_ERROR
// syscall) is why this wrapper exists distinct from the emitter: it lets callers
// pass a printf-style (level, format, ...) while the already-formatted text is then
// safely re-emitted as a literal via "%s". The original C body is below.
void Com_Error(errorParm_t level, const char *format, ...)
{
    // 0x3002b3db..0x3002b3f7: format the message into a fixed 1024-byte buffer.
    // va_start yields the address of the first vararg slot ([ESP+0x410]), which
    // the machine code passes straight to vsprintf as the va_list argument.
    // `level` is not referenced by this body; the low-level emitter always issues
    // CG_ERROR.
    char buffer[MAX_STRING_CHARS];
    va_list args;

    (void)level;

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    va_start(args, format);
    (void)vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    // 0x3002b3fc..0x3002b406: re-emit the finished text as a literal string through
    // the low-level error emitter, which issues the CG_ERROR engine trap (id 1).
    // The "%s" format ensures the already-expanded text is not re-interpreted.
    Com_ErrorMessage("%s", buffer);
}
