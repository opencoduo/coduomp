#include "../client_recovered.h"
#include "../globals.h"

#include <stddef.h>

// Source: uo_cgame_mp_x86.dll 0x30051ac0..0x30051aee
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30051ac0_30051aee.mcode
// This cgame body calls Menu_FadeItemByName; UI's same-named body contains the
// fade loop inline, so this function intentionally remains module-local.
//
// Script_FadeIn — the menu-script command handler bound to the "fadein" keyword.
// The command/handler pairing is proven from the {char *name; void *fn} menu-script
// dispatch table in .data (cited by the reconstructed siblings Script_Show/
// Script_Hide): the block runs {"show",0x30051a60}, {"fadein",0x30051ac0},
// {"fadeout",0x30051af0}, {"hide",0x30051a90}, {"open",0x30051b20}. This function is
// the fade sibling of Script_Show/Script_Hide: it is byte-identical to the
// "fadeout" handler at 0x30051af0 except for the flag pushed to Menu_FadeItemByName
// — this function pushes 0x0 at 0x30051ae1 (fadeOut = qfalse => start a FADE-IN),
// while 0x30051af0 pushes 0x1 (fadeOut = qtrue => fade-out). It calls
// Menu_FadeItemByName (0x30051790) where Script_Show/Hide call Menu_ShowItemByName
// (0x30051710) — the fade variant animates the transition instead of snapping the
// WINDOW_VISIBLE bit.
//
// The .mcode header's "# name SP_info_camp" is a pure size-only corpus match
// (win size 0x2e == matched size 0x2e) and is REJECTED per the no-size-matching
// rule: SP_info_camp is a Quake3 entity spawn function, whereas this function
// parses a token, dereferences an itemDef_t through item->parent (+0xf0), and calls
// Menu_FadeItemByName — a ui_shared.c menu-script handler, not an entity spawner.
// This is exactly the mislabel-by-size the AGENTS.md warns about.
//
// Menu-script command signature is the ui_shared.c __cdecl
// (itemDef_t *item, char **args). Both arguments are used: `args` (arg2) is the
// parse source handed to String_Parse, and `item` (arg1) supplies the owning menu
// through item->parent (+0xf0). String_Parse takes the parse source in EAX and the
// out char** in EDI; Menu_FadeItemByName takes the parsed name in EAX, the menu in
// EDI, and the fadeOut flag as its single stack arg.
//
// Register/stack ABI, proven from the machine code (note the item read differs from
// Script_Show/Hide, which POP EDI before reading item; here EDI stays pushed across
// the item read, so item resolves at [ESP+0xc] rather than [ESP+0x8]):
//   30051ac0 PUSH ECX                 reserve the 4-byte [ESP] local (name slot)
//   30051ac1 MOV EAX,[ESP+0xc]        EAX = args (arg2); read before PUSH EDI, so
//                                     [ESP+0xc] resolves to the entry [ESP+0x8]
//   30051ac5 PUSH EDI
//   30051ac6 LEA EDI,[ESP+0x4]        EDI = &name (the reserved local slot)
//   30051aca CALL 0x300505a0          EAX = String_Parse(args, &name)
//   30051acf TEST EAX,EAX
//   30051ad1 JZ 0x30051aeb            missing token => do nothing, return
//   30051ad3 MOV ECX,[ESP+0xc]        ECX = item (arg1); EDI still pushed, so the
//                                     entry [ESP+0x4] now resolves to [ESP+0xc]
//   30051ad7 MOV EAX,[ESP+0x4]        EAX = name (parsed token from the local)
//   30051adb MOV EDI,[ECX+0xf0]       EDI = item->parent (owning menu)
//   30051ae1 PUSH 0x0                 fadeOut = qfalse (this is "fadein")
//   30051ae3 CALL 0x30051790          Menu_FadeItemByName(item->parent, name, qfalse)
//   30051ae8 ADD ESP,0x4              caller-cleans the one pushed stack arg
//   30051aeb POP EDI
//   30051aec POP ECX                  release the local slot
//   30051aed RET                      plain RET (caller cleans its own args)

void Script_FadeIn(itemDef_t *item, char **args)
{
    // [ESP] local reserved by the entry PUSH ECX; filled by String_Parse through
    // EDI (LEA EDI,[ESP+0x4]) and re-read at 0x30051ad7.
    const char *name;

    // 0x30051aca..0x30051ad1: read one string token (the item-group name) from the
    // parse source. args arrives in EAX, &name in EDI; on failure (EAX == 0) the JZ
    // at 0x30051ad1 skips to the epilogue and the command is a no-op.
    if (!String_Parse(args, &name)) {
        return;
    }

    // 0x30051ad3..0x30051ae8: start a fade-in on every item in the owning menu whose
    // group/name matches the parsed token. fadeOut is qfalse — the distinguishing
    // difference from the "fadeout" handler at 0x30051af0, which passes qtrue.
    Menu_FadeItemByName(item->parent, name, qfalse);
}
