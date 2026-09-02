// Sources: uo_cgame_mp_x86.dll 0x300015f0..0x3000169d and
//          uo_game_mp_x86.dll  0x200015e0..0x2000168d.
// Supporting Mac cgame/game expose the canonical BG_SetupAnimNoteTypes name
// with matching 0xbc-byte bodies.
//
// BG_SetupAnimNoteTypes(bg_static_animation_table_t *table)
//
// Recompute which static animation entries are actually referenced by the player
// animation scripts:
//   1. clear every entry's usedByScript flag (entries[i].usedByScript = 0);
//   2. for each player script, walk its command list and, for each of the two
//      body-part slots in a command, if the body-part word is nonzero, mark the
//      referenced animation entry as used (entries[animIndex].usedByScript = 1).
//
// Name resolution: the .mcode header's mechanical `# name script_func_map_restart`
// is a SIZE guess (win size 0xad == matched size 0xad) and is REJECTED — the body
// is not a script command (no VM/param access, no "map_restart" side effects); it
// zeroes a static-animation flag array and re-derives it from the anim scripts.
// The behavior and call graph match the Mac-retained source name and the
// corresponding game-module body.
//
// ABI: non-default register convention. The table pointer arrives in ECX (no stack
// arguments) and the function ends with a plain RET. Proven at the sole caller
// 0x30001905, which does `MOV ECX,[0x30134cc8]` (the BG animation-table pointer)
// immediately before `CALL 0x300015f0`. The leading `PUSH ECX` reserves the outer
// loop-counter local (later read/written via [ESP+0x4]/[ESP+0x10]); it is not a
// pushed argument. Modeled here as an ordinary C function taking the table pointer;
// the ECX register detail is a calling-convention fact, not source behavior.

#include "bg_animation.h"
#include "compat/coduo_int32_bits.h"

void BG_SetupAnimNoteTypes(bg_static_animation_table_t *table)
{
    /* Loop 1: clear usedByScript on every static animation entry.
     * 0x300015f1 MOV EDX,[ECX+0xb800] (entryCount); 0x300015fc JLE (signed <= 0);
     * body at 0x30001601 writes [ECX + i*0x5c + 0x58] = 0 with the count reloaded
     * from [ECX+0xb800] each iteration and the JL (signed) continuation. */
    int32_t entryCount = table->entryCount;
    for (int32_t i = 0; i < entryCount; i = coduo_int32_from_bits((uint32_t)i + 1u)) {
        table->entries[i].usedByScript = 0;
        entryCount = table->entryCount; /* 0x30001607 reloads [ECX+0xb800] each pass */
    }

    /* Loop 2: for each player script, mark the animation entries its commands use.
     * 0x30001615 MOV EAX,[ECX+0x20eac] (scriptList count); 0x30001625 JLE skips when
     * count <= 0. EDI walks the slot array at [ECX+0x20eb0] (4-byte stride). The
     * player scriptList is scriptLists[0] (+0x20eac). */
    bg_anim_script_list_t *scriptList = &table->scriptLists[0];
    int32_t scriptCount = scriptList->count;
    for (int32_t j = 0; j < scriptCount; j = coduo_int32_from_bits((uint32_t)j + 1u)) {
        /* 0x30001635 MOV ESI,[EDI]: the slot holds an absolute bg_anim_script_t*. */
        bg_anim_script_t *script = scriptList->scripts[j];

        /* 0x30001637 MOV EAX,[ESI+0x88] (commandCount); 0x30001641 JLE (signed). */
        int32_t commandCount = script->commandCount;
        for (int32_t k = 0; k < commandCount; k = coduo_int32_from_bits((uint32_t)k + 1u)) {
            bg_anim_script_command_t *cmd = &script->commands[k];

            /* 0x30001650 CMP word [cmd+0x00],0 (bodyPart[0]); if nonzero,
             * 0x30001657 MOVSX EBP,word [cmd+0x04] (animIndex[0], sign-extended),
             * 0x3000165b IMUL by 0x5c, 0x3000165e MOV [ECX+EBP+0x58],EBX(=1). */
            if (cmd->bodyPart[0] != 0) {
                table->entries[cmd->animIndex[0]].usedByScript = 1;
            }

            /* 0x30001662 CMP word [cmd+0x02],0 (bodyPart[1]); if nonzero,
             * 0x30001668 MOVSX EBP,word [cmd+0x06] (animIndex[1], sign-extended),
             * same 0x5c-stride write of the mark value 1. */
            if (cmd->bodyPart[1] != 0) {
                table->entries[cmd->animIndex[1]].usedByScript = 1;
            }

            /* 0x30001673 reloads commandCount from [ESI+0x88] before the JL test. */
            commandCount = script->commandCount;
        }

        /* 0x30001685 reloads the scriptList count from [ECX+0x20eac] each pass. */
        scriptCount = scriptList->count;
    }
}
