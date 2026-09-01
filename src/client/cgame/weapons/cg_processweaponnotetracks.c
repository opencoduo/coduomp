// Source: uo_cgame_mp_x86.dll 0x30042c40..0x30042d2f
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30042c40_30042d2f.mcode
//
// CG_ProcessWeaponNoteTracks — play the local player's per-weapon
// "noteTrackSound{A..D}" sounds for whichever notetracks fired on the currently
// bound animation this frame.
//
// Naming: the .mcode carries the mechanical pre-hint `finishSpawningKeyedMover`,
// a size-match (win size 0xef == matched size 0xef) to a game_mp_uo SERVER name.
// It is REJECTED: this body issues the cgame notetrack trap (0x99), Q_stricmps the
// literal notetrack names "noteTrackSoundA/B/C/D", and plays weapon sounds via
// CG_PlaySoundAliasByName — a cgame weapon/sound emitter, not a mover-spawn routine.
// The Mac CG_ProcessWeaponNoteTracks walks the same notetrack/sound path, resolving
// the source name.
//
// Machine-code notes:
//  - 0x30042c41  MOV EAX,[cg_predictedPlayerState.currentWeapon] (0x3048329c); JZ ret
//                when 0 (no weapon equipped). EDI (playedHandle) pre-zeroed.
//  - 0x30042c51  IMUL EAX,EAX,0x1c4 ; ADD EAX,0x30413580  ->  &cg_weaponInfos[w].
//  - 0x30042c5f  push &list ; push 0x99 ; call [0x30085e9c] ; add esp,8
//                == trap_XAnimGetNotetracks(&list) -> count (EAX). JLE ret.
//  - per notetrack (ESI steps by 0xc): read name = *(char**)(list + ESI) (+0x00),
//                Q_stricmp against "noteTrackSoundA".."noteTrackSoundD" in order;
//                first match loads the matching weapon handle (+0x118..+0x124) into
//                EDI and breaks the compare chain. Note the +0x04 kind word is NOT
//                read here (it is in the sibling CG_SetGunHandFromNotetracks).
//  - 0x30042d02  if (playedHandle != 0): LEA ECX,[cg_snap+0x20]
//                (&cg_snap->ps.psOrigin, the ECX/this arg); EAX = playedHandle
//                (soundName); push cg_snap->ps.psClientNum (+0xe0, entityNum);
//                call CG_PlaySoundAliasByName (0x3002ca80); add esp,4.
//  - loop DEC EBP (count) / JNZ back to top.

#include "client/cgame/globals.h"
#include "client/cgame/client_recovered.h"

void CG_ProcessWeaponNoteTracks(void)
{
    /* 0x30042c41: no equipped weapon -> nothing to play. */
    int weaponIndex = cg_predictedPlayerState.currentWeapon;
    if (weaponIndex == 0) {
        return;
    }

    /* 0x30042c51..0x30042c5d: select this weapon's cgame registration record. */
    const cgWeaponInfo_t *weapon = &cg_weaponInfos[weaponIndex];

    /* 0x30042c5f..0x30042c6f: fetch the notetrack list of the currently bound
     * animation. trap id 0x99; returns the record count (signed). */
    xanim_deferred_notify_t *list = NULL;
    int count = trap_XAnimGetNotetracks(&list);

    /* 0x30042c72: JLE ret — nothing to do on zero or negative count. */
    if (count <= 0) {
        return;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */

    /* 0x30042c7c..0x30042d23: walk the notetracks. ESI is the running byte offset
     * (ADD ESI,0xc), EBP the remaining count (DEC EBP). */
    for (int i = 0; i < count; ++i) {
        const char *trackName = list[i].name; /* +0x00 */
        const char *playedAliasName = NULL;   /* fix: reset per track (stock: stale) */

        /* 0x30042c84..0x30042cfc: first matching notetrack name wins; the chain
         * short-circuits (each JNZ falls through to the next Q_stricmp only when the
         * previous did not match). Stock's D branch `JNZ 0x30042d02` (0x30042cfa)
         * skips the MOV entirely, leaving the stale prior handle; with the per-track
         * reset above, a no-match track now leaves the alias empty instead. */
        if (coduo_crt_stricmp(trackName, "noteTrackSoundA") == 0) {
            playedAliasName = weapon->noteTrackSoundA;   /* +0x118 */
        } else if (coduo_crt_stricmp(trackName, "noteTrackSoundB") == 0) {
            playedAliasName = weapon->noteTrackSoundB;   /* +0x11c */
        } else if (coduo_crt_stricmp(trackName, "noteTrackSoundC") == 0) {
            playedAliasName = weapon->noteTrackSoundC;   /* +0x120 */
        } else if (coduo_crt_stricmp(trackName, "noteTrackSoundD") == 0) {
            playedAliasName = weapon->noteTrackSoundD;   /* +0x124 */
        }

        /* 0x30042d02: TEST EDI,EDI / JZ — only start a sound when the handle is
         * nonzero (the matched track's handle; stock could also see a stale
         * nonzero handle here, which the per-track reset above eliminates). */
        if (playedAliasName != NULL) {
            /* 0x30042d06..0x30042d1c: CG_PlaySoundAliasByName(&cg_snap->ps.psOrigin,
             * (const char *)handle, cg_snap->ps.psClientNum). channelObj (ECX/this) =
             * cg_snap + 0x20; soundName (EAX) = the registered handle; entityNum
             * (stack) = cg_snap->ps.psClientNum (+0xe0). */
            CG_PlaySoundAliasByName(cg_snap->ps.psClientNum,
                                    &cg_snap->ps.psOrigin,
                                    playedAliasName);
        }
    }
}
