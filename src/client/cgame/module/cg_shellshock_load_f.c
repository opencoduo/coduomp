#include "../client_recovered.h"
#include "../globals.h"

#include <stdint.h>

// Source: uo_cgame_mp_x86.dll 0x30017590..0x300175ef
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30017590_300175ef.mcode
//
// CG_ShellShock_Load_f: the console-command handler for "cg_shellshock_load".
// It is one of the command callbacks dispatched by CG_ConsoleCommand
// (0x300178c0) and follows the id-Tech "_f" command-callback convention (takes
// no arguments; reads the command tokens through the trap_Arg* syscalls).
//
// Behavior (proven instruction-by-instruction):
//   - trap_Argc() must be exactly 2 (the command word plus one <name> token);
//     otherwise it prints the usage string and returns.
//   - Otherwise it copies token 1 into a 64-byte buffer via trap_Argv(1, ...)
//     and hands it to CG_ShellShockLoad, which builds "scripts/%s.shock" and
//     loads/parses that shellshock definition file.
//
// The .mcode's mechanical name `script_func_animhasnotetrack` is a pure size
// match (win 0x5f == server 0x5f) and is REJECTED: there is no notetrack/anim
// behavior here. The name is proven by the emitted usage string
// "USAGE: cg_shellshock_load <name>\n" (0x30076954) and the CG_ShellShockLoad
// callee, which formats "scripts/%s.shock" (0x3007a404) and prints
// "^1couldn't open '%s'\n" (0x3007a3ec) on failure.
//
// Structure (from the machine code):
//
//   0x30017590 SUB ESP,0x44                reserve name[64] + /GS canary slot
//   0x30017593 MOV EAX,[0x30081650]        \  MSVC /GS prologue: snapshot the
//   0x3001759a MOV [ESP+0x44],EAX          /  __security_cookie into frame+0x40
//   0x30017598 PUSH 0xc                     trap id CG_ARGC
//   0x3001759e CALL *0x30085e9c            EAX = trap_Argc()
//   0x300175a4 ADD ESP,0x4                 caller-clean the 1 pushed dword
//   0x300175a7 CMP EAX,0x2                 \  if argc != 2, take the usage branch
//   0x300175aa JZ  0x300175c6             /
//   0x300175ac PUSH 0x30076954            "USAGE: cg_shellshock_load <name>\n"
//   0x300175b1 CALL 0x3002b2b0            Com_PrintMessage(usage)
//   0x300175b6 ADD ESP,0x4                caller-clean the 1 pushed dword
//   0x300175b9 MOV ECX,[ESP+0x40]         \  /GS epilogue: reload canary and
//   0x300175bd CALL 0x30061639           /  verify via __security_check_cookie
//   0x300175c2 ADD ESP,0x44 / RET         release frame, return (void)
//
//   0x300175c6 PUSH 0x40                   trap_Argv arg3 = bufferLength (64)
//   0x300175c8 LEA EAX,[ESP+0x4]           EAX = &name[0]  (frame+0)
//   0x300175cc PUSH EAX                    trap_Argv arg2 = buffer
//   0x300175cd PUSH 0x1                     trap_Argv arg1 = n (token 1)
//   0x300175cf PUSH 0xd                     trap id CG_ARGV
//   0x300175d1 CALL *0x30085e9c            trap_Argv(1, name, 64)
//   0x300175d7 ADD ESP,0x10               caller-clean the 4 pushed dwords
//   0x300175da LEA EAX,[ESP]               EAX = &name[0]
//   0x300175dd CALL 0x3003b950            CG_ShellShockLoad(name) (name in EAX)
//   0x300175e2 MOV ECX,[ESP+0x40]         \  /GS epilogue (same as above)
//   0x300175e6 CALL 0x30061639           /
//   0x300175eb ADD ESP,0x44 / RET         release frame, return (void)
//
// The SUB/MOV cookie snapshot (0x30017593/0x3001759a) and the reload+check
// epilogues (0x300175b9/0x300175bd and 0x300175e2/0x300175e6) are compiler-
// generated MSVC /GS stack-protector code, not source statements; they are
// emitted because this function owns the 64-byte `name` stack buffer. The
// original C body is just the argc gate plus the trap_Argv + CG_ShellShockLoad
// below.
void CG_ShellShock_Load_f(void)
{
    // 0x300175a7..0x300175aa: require exactly two tokens ("cg_shellshock_load"
    // and one <name>). Anything else prints the usage line and returns.
    if (trap_Argc() != 2) {
        Com_PrintMessage("USAGE: cg_shellshock_load <name>\n");
        return;
    }

    // 0x300175c6..0x300175dd: copy token 1 (the <name>) into a 64-byte buffer and
    // load the corresponding "scripts/<name>.shock" definition.
    char name[64];

    trap_Argv(1, name, (int32_t)sizeof(name));
    CG_ShellShockLoad(name);
}
