#include "bg_animation.h"
#include "bg_weapon.h"

#include <stddef.h>
#include <string.h>

// Sources: uo_cgame_mp_x86.dll 0x300014f0..0x30001580 and
//          uo_game_mp_x86.dll  0x200014f0..0x20001580.
// Evidence: direct objdump comparison of both authoritative Windows modules.
//
// BG_InitWeaponStrings: (re)build the value table for the WEAPON
// animation-condition type. bgAnimConditionTypes[ANIM_COND_WEAPON].values points
// at a fixed 128-entry bg_indexed_string_t table (at 0x3053a040). This routine
// clears that whole table, installs "none" at index 0, then installs one entry
// per registered weapon (bg_weaponInfos[1..bg_numWeapons]), storing each
// weapon's name (weaponInfo_t::name, +0x04) together with its precomputed
// case-insensitive BG_StringHashValue. The stored table is later consumed by the
// generic bg_indexed_string_t lookup (BG_IndexForString/BG_StringHashValue) that
// maps an anim-script "weapon" condition token to its weapon index.
//
// The .mcode mechanical name "CMD_VEH_FreeVehicle" is a pure size match
// (win 0x91 == corpus 0x91) and is REJECTED: this body frees nothing and has no
// vehicle/command state. It clears a 0x100-dword (128 * 8-byte) table, seeds a
// "none" string entry, and fills a { name, hash } record per registered weapon
// from the weaponInfo_t pointer array (0x30134cd8 = bg_weaponInfos) counted by
// 0x30134cd4 (bg_numWeapons). Identity is proven by: the table base
// 0x3053a040 being bgAnimConditionTypes[0].values (globals.c: the 0x3008238c
// fragment stores 0x3053a040 as bgAnimConditionTypes[0].values); the {name,hash}
// 8-byte-stride layout matching bg_indexed_string_t; the "none" literal at
// 0x30072894; and the weapon-list iteration over bg_weaponInfos[i]->name (+0x04),
// the same name field BG_GetWeaponIndexForName (0x300110f0) reads.
//
// Standard cdecl (no args; RET with no immediate; touches only file-scope
// globals). EBP is reused as the loop index register after the prologue saves
// the real EBP-less frame (this function keeps no frame pointer).
//
// Instruction map (behavior-affecting):
//   0x30001502 XOR EAX,EAX               \  clear the 128-entry (0x100 dword)
//   0x30001504 MOV ECX,0x100             |  bg_indexed_string_t value table:
//   0x30001509 MOV EDI,0x3053a040        |  memset(table, 0, 0x100 * 4)
//   0x3000150e REP STOSD                 /
//   0x30001510 MOV EAX,0x30072894        \  table[0].name = "none"
//   0x30001515 MOV [0x3053a040],EAX      /
//   0x3000151a CALL 0x300011b0           \  table[0].hash = BG_StringHashValue("none")
//   0x3000151f MOV [0x3053a044],EAX      /  (EAX carries "none" ptr into the call)
//   0x30001524 MOV EAX,[0x30134cd4]      \  if (bg_numWeapons < 1) return
//   0x30001529 MOV EBP,1                 |  index = 1
//   0x3000152e CMP EAX,EBP               |
//   0x30001530 JL 0x3000158e            /
// loop (0x30001534):
//   0x30001534 MOV EAX,[0x30134cd8]      \  wi = bg_weaponInfos[index]
//   0x30001539 MOV ECX,[EAX+EBP*4]       |
//   0x3000153c MOV ESI,[ECX+4]           |  name = wi->name (+0x04)
//   0x3000153f MOV [EBP*8+0x3053a040],ESI/  table[index].name = name
//   -- inlined BG_StringHashValue(name) with the -1 -> 0 fixup: --
//   0x30001546 MOV AL,[ESI]              \  c = *name; hash = 0
//   0x30001548 XOR EBX,EBX               |
//   0x3000154a TEST AL,AL                |  if (*name == 0) hash stays 0
//   0x3000154c JZ 0x3000157b            /
//   0x3000154e MOV EDI,0x77              \  EDI = 0x77 - name  (so that
//   0x30001553 SUB EDI,ESI              /   EDI + &name[i] == 0x77 + i)
// hashloop (0x30001555):
//   0x30001555 MOVSX EDX,AL             \  push (int)(signed char)c
//   0x30001558 PUSH EDX                 |
//   0x30001559 CALL 0x3005b84a          |  Q_tolower(c)
//   0x3000155e MOVSX EAX,AL             |  folded = (signed char)result
//   0x30001561 LEA ECX,[EDI+ESI]        |  weight = 0x77 + i
//   0x30001564 IMUL EAX,ECX             |  hash += folded * weight
//   0x30001567 ADD EBX,EAX              |
//   0x30001569 MOV AL,[ESI+1]           |  c = name[i+1]
//   0x3000156c ADD ESP,4                |  (pop the pushed arg)
//   0x3000156f INC ESI                  |  ++i (advance name cursor)
//   0x30001570 TEST AL,AL               |
//   0x30001572 JNZ 0x30001555          /   while (c != 0)
//   0x30001574 CMP EBX,-1               \  if (hash == -1) hash = 0
//   0x30001577 JNZ 0x3000157b           |
//   0x30001579 XOR EBX,EBX             /
// store (0x3000157b):
//   0x3000157b MOV EAX,[0x30134cd4]      \  table[index].hash = hash
//   0x30001580 MOV [EBP*8+0x3053a044],EBX|
//   0x30001587 INC EBP                   |  ++index
//   0x30001588 CMP EBP,EAX               |  while (index <= bg_numWeapons)
//   0x3000158a JLE 0x30001534           /
//   0x3000158e RET

// The fixed WEAPON condition value table (weaponStrings, 128 entries)
// is declared in bg_animation.h; MAX_WEAPONS == 128 is
// the 0x100-dword REP STOSD zero count divided by 2 dwords per entry.

// The literal name for index 0 ("none"), stored in .rdata at 0x30072894, that
// BG_StringHashValue hashes.
static const char *const BG_WEAPON_CONDITION_NONE = "none";

typedef char bg_indexed_string_name_offset_check[(offsetof(bg_indexed_string_t, name) == 0x00) ? 1 : -1];
#if defined(__i386__) || (defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4)
typedef char bg_indexed_string_hash_offset_check[(offsetof(bg_indexed_string_t, hash) == 0x04) ? 1 : -1];
typedef char bg_indexed_string_size_check[(sizeof(bg_indexed_string_t) == 8) ? 1 : -1];
#endif

void BG_InitWeaponStrings(void)
{
    int32_t weapon;

    memset(weaponStrings, 0, sizeof(weaponStrings));

    // Index 0 is the "none" entry; its hash uses the real BG_StringHashValue call
    // (0x300011b0), which returns 0 rather than the -1 sentinel.
    weaponStrings[0].name = BG_WEAPON_CONDITION_NONE;
    weaponStrings[0].hash = BG_StringHashValue(BG_WEAPON_CONDITION_NONE);

    // Indices 1..bg_numWeapons mirror the registered weapon list. Both Windows
    // modules inline the same hash loop and reload the live weapon count at the
    // loop tail; the ordinary call below preserves that operation and the for
    // condition preserves the live reload without duplicating the hash body.
    for (weapon = 1; weapon <= bg_numWeapons; ++weapon) {
        weaponStrings[weapon].name = bg_weaponInfos[weapon]->pickupName;
        weaponStrings[weapon].hash = BG_StringHashValue(weaponStrings[weapon].name);
    }
}
