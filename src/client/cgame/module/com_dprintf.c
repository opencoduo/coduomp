#include "../client_recovered.h"
#include "../globals.h"

#include <stdarg.h>
#include <stdint.h>

// Source: uo_cgame_mp_x86.dll 0x3002b470..0x3002b4ca
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002b470_3002b4ca.mcode
//
// Com_DPrintf: the developer-gated variadic diagnostic printer. It is the
// standard Quake3/CoD "developer" print: formatted output that is produced and
// emitted only when the `developer` cvar is enabled, and is otherwise a no-op.
//
// The .mcode's mechanical name SP_script_model is a pure size match (win 0x5a ==
// PPC 0x5a) and is REJECTED: there is no entity/spawn/script-model behavior here;
// the body is a developer gate around a vsprintf + print. Size is not evidence.
// The name Com_DPrintf is proven by the call graph: it is the developer-gated
// twin of the print backend Com_PrintMessage (0x3002b2b0), differing from the
// unconditional Com_Printf (0x3002b420) only by the developer_vmCvar.integer gate.
//
// Structure (proven instruction-by-instruction):
//
//   0x3002b470 SUB ESP,0x404               reserve 0x400-byte buffer + canary slot
//   0x3002b476 MOV EAX,[0x30081650]        \  MSVC /GS prologue: snapshot the
//   0x3002b47b MOV [ESP+0x400],EAX         /  __security_cookie into the canary slot
//   0x3002b482 MOV EAX,[0x3052f8ec]        load developer_vmCvar.integer
//   0x3002b487 TEST EAX,EAX                \  gate: if developer_vmCvar.integer == 0,
//   0x3002b489 JZ  0x3002b4b7              /  skip the whole format+print block
//   0x3002b48b MOV ECX,[ESP+0x408]         ECX = format   (1st stack arg)
//   0x3002b492 LEA EAX,[ESP+0x40c]         EAX = &args    (va_start: 1st vararg)
//   0x3002b499 PUSH EAX                    vsprintf arg3 = args (va_list)
//   0x3002b49a PUSH ECX                    vsprintf arg2 = format
//   0x3002b49b LEA EDX,[ESP+0x8]           EDX = &buffer  (0x400 buf at frame+0)
//   0x3002b49f PUSH EDX                    vsprintf arg1 = buffer
//   0x3002b4a0 CALL 0x3005b538             vsprintf(buffer, format, args)
//   0x3002b4a5 LEA EAX,[ESP+0xc]           EAX = &buffer   (frame+0, args on stack)
//   0x3002b4a9 PUSH EAX                    Com_PrintMessage vararg = buffer
//   0x3002b4aa PUSH 0x30076cd8             Com_PrintMessage format = "%s"
//   0x3002b4af CALL 0x3002b2b0             Com_PrintMessage("%s", buffer)
//   0x3002b4b4 ADD ESP,0x14               caller-clean both calls' 5 pushed dwords
//   0x3002b4b7 MOV ECX,[ESP+0x400]         \  MSVC /GS epilogue: reload canary and
//   0x3002b4be CALL 0x30061639            /  verify it via __security_check_cookie
//   0x3002b4c3 ADD ESP,0x404              release the frame
//   0x3002b4c9 RET                        void, caller-cleaned varargs
//
// The buffer size is exactly 0x400 (1024): SUB ESP,0x404 reserves the 0x400 buffer
// at frame offset 0 plus the 4-byte canary slot at frame+0x400. The print goes
// through the fixed format "%s" (0x30076cd8) so the already-formatted, potentially
// %-bearing buffer is emitted verbatim rather than re-interpreted.
//
// The SUB/MOV cookie snapshot (0x3002b476/0x3002b47b) and the reload+check epilogue
// (0x3002b4b7/0x3002b4be) are compiler-generated MSVC /GS stack-protector code, not
// source statements: the original C body is just the developer gate plus the
// vsprintf + print below. They are emitted because this function has a large
// stack buffer.
void Com_DPrintf(const char *format, ...)
{
    // 0x3002b482..0x3002b489: developer gate. When the developer cvar's integer is
    // zero, Com_DPrintf produces no output at all (JZ jumps past the entire
    // format+print block straight to the /GS epilogue).
    if (developer_vmCvar.integer == 0) {
        return;
    }

    // 0x3002b48b..0x3002b4a0: format the message into a fixed 1024-byte buffer.
    // va_start yields &format+1, i.e. the address of the first vararg slot
    // ([ESP+0x40c]), which the machine code passes straight to vsprintf as
    // the va_list argument.
    char buffer[MAX_STRING_CHARS];
    va_list args;

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
    va_start(args, format);
    (void)vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    // 0x3002b4a5..0x3002b4af: emit the formatted text through the print backend
    // with the literal format "%s", so buffer is printed verbatim.
    Com_PrintMessage("%s", buffer);
}
