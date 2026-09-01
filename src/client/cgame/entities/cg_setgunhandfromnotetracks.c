// Source: uo_cgame_mp_x86.dll 0x3001f760..0x3001f801
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001f760_3001f801.mcode

#include "../client_recovered.h"
#include "../globals.h"

#include <stdint.h>

/*
 * CG_SetGunHandFromNotetracks (0x3001f760) — record which hand holds the gun for a
 * given client by scanning the notetrack list of the animation currently bound on
 * the active DObj.
 *
 * Behavior (proven against the .mcode):
 *   - Ignore an out-of-range client: `TEST EDI,EDI; JL` rejects clientNum < 0 and
 *     `CMP EDI,0x40; JGE` rejects clientNum >= 64 (MAX_CLIENTS). clientNum arrives in
 *     EDI (register-arg ABI); expressed here as an ordinary parameter.
 *   - trap_XAnimGetNotetracks(&list) (cgame_syscall id 0x99) returns the entry count
 *     and writes a pointer to the engine's xanim_deferred_notify_t[] into `list`. A count <= 0
 *     ends the routine (JLE 0x3001f7ff).
 *   - For each entry, only kind == 1 is processed (MOVZX EAX,word[+0x04]; DEC EAX;
 *     JNZ skip). The name pointer is at +0x00.
 *   - An `anim_gunhand = "left"` name (string @0x300772a8, compared FIRST) sets
 *     gunHandLeft = 1; an `anim_gunhand = "right"` name (@0x30077290, compared
 *     SECOND) sets gunHandLeft = 0. Either match then sets dobjNeedsUpdate = 1 (the shared
 *     store at 0x3001f7f0, EBX = 1). A name matching neither leaves both fields
 *     untouched. Comparison is coduo_crt_stricmp (0x30069275),
 *     called cdecl `push name-string; push literal`.
 *   - The per-client fields live in bgs.clientinfo[clientNum]: the machine code
 *     computes clientNum*0x4d0 + 0x305e2334 (gunHandLeft, +0x400 of the 0x4d0-stride
 *     element based at 0x305e1f34) and +0x305e2338 (dobjNeedsUpdate, +0x404).
 *
 * Naming: the .mcode's size-guessed "PM_BeginWeaponDeploy" is rejected — this touches
 * no pmove state; it reads XAnim notetracks and writes the cgame per-client anim-state
 * array. Role name from the `anim_gunhand` notetrack strings and the gun-hand fields
 * it sets; the same-module PPC bank lists CG_ProcessClientNoteTracks /
 * CG_ProcessWeaponNoteTracks, but neither is address-proven to be this narrow helper,
 * so it is named by its proven role.
 *
 * Loop-index note: the .mcode walks by byte offset (ESI += 0xc). Expressed here as an
 * index into xanim_deferred_notify_t[] (the proven 12-byte stride), which is
 * address-identical.
 */
void CG_SetGunHandFromNotetracks(int clientNum)
{
    xanim_deferred_notify_t *list;
    int count;
    int i;

    if (clientNum < 0 || clientNum >= MAX_CLIENTS_IN_SNAPSHOT)
        return;

    count = trap_XAnimGetNotetracks(&list);
    if (count <= 0)
        return;

    for (i = 0; i < count; i++) {
        if (list[i].notifyType != XANIM_NOTIFY_CLIENT)
            continue;

        if (coduo_crt_stricmp(list[i].name, "anim_gunhand = \"left\"") == 0) {
            bgs.clientinfo[clientNum].gunHandLeft = 1;
            bgs.clientinfo[clientNum].dobjNeedsUpdate = 1;
        } else if (coduo_crt_stricmp(list[i].name, "anim_gunhand = \"right\"") == 0) {
            bgs.clientinfo[clientNum].gunHandLeft = 0;
            bgs.clientinfo[clientNum].dobjNeedsUpdate = 1;
        }
    }
}
