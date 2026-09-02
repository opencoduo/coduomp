// Source: uo_cgame_mp_x86.dll 0x30025570..0x300255fa
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30025570_300255fa.mcode

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

#include <string.h>

/*
 * CG_ClearFlameChunks (0x30025570) — reset the client flame-chunk subsystem to
 * empty and rebuild the free list.
 *
 * Name: the .mcode header size-match "HudElem_DestroyAll" (win size 0x8a) is
 * REJECTED. This function touches no HUD-element state; it clears the flame-chunk
 * pool and the per-owner flame-info region, and threads every pool node onto the
 * free list. Its identity is proven by the call graph and data it owns:
 *   - The pool base cg_flameChunks (0x30134ce8) is allocated by CG_InitFlameChunks
 *     (0x300279d0), which requests exactly 0x2a0000 bytes via cgame trap 0xc0 and
 *     then CALLs this function (0x300279f8) — so this is the pool initializer.
 *   - CG_SpawnFlameChunk (0x30025600) pops nodes from the cg_freeFlameChunks list
 *     this function builds; CG_SpawnFlameChunkOnBone (0x30023d50) calls that
 *     spawner and prints "Out of flame chunks" on failure.
 *   - CG_AddFlameChunks (0x30024050) indexes the per-owner region cg_flameInfo
 *     (0x300ab750) with element stride 0xb8.
 * The exact original symbol is unproven (no cgame symbol table recovered), but the
 * role — "clear/rebuild the flame-chunk pool" — is proven; named accordingly. The
 * matching PPC bank entry is cgame_mp.dll!CG_ClearFlameChunks.
 *
 * ABI: PUSH EBP/ESI/EDI prologue, RET with no immediate (cdecl, no args, void).
 *
 * Behaviour, proven instruction-by-instruction against the .mcode:
 *   0x30025571  EBP = cg_flameChunks (pool base pointer).
 *   0x3002557b  REP STOSD, ECX=0xa8000, EDI=EBP, EAX=0: zero the whole pool
 *               (0xa8000 dwords == 0x2a0000 bytes == FLAME_CHUNK_COUNT*FLAME_CHUNK_SIZE).
 *   0x30025584  REP STOSD, ECX=0xb800, EDI=cg_flameInfo, EAX=0: zero the whole
 *               per-owner flame-info region (0xb800 dwords == 0x2e000 bytes ==
 *               FLAME_INFO_COUNT*FLAME_INFO_SIZE).
 *   0x30025592  cg_freeFlameChunks = EBP (free list head := &node[0]).
 *   0x30025598  cg_activeFlameChunks = 0.
 *   0x3002559e  cg_flameChunkList = 0.
 *   0x300255b0  loop over i in [0, FLAME_CHUNK_COUNT):
 *                 node[i].next = &node[i+1]          (MOV [EAX-4],EDX; EDX=&node[i+1])
 *                 node[i].unresolvedField_1c = 0     (MOV [EAX+0x1c],EDI; EDI=0)
 *                 node[i].prev = (i==0) ? NULL       (SETLE/DEC/AND branchless:
 *                                       : &node[i-1]  ESI holds &node[i-1])
 *               EAX advances by FLAME_CHUNK_SIZE (0x150) each iteration; the loop
 *               counter compares ECX < 0x2000 (JL).
 *   0x300255e0  node[FLAME_CHUNK_COUNT-1].next = 0: terminate the last node
 *               (0x29feb0 == (FLAME_CHUNK_COUNT-1)*FLAME_CHUNK_SIZE == node[8191]+0).
 *   0x300255e6  cg_numActiveFlameChunks = 0.
 *   0x300255ee  cg_flameChunksInited = 1.
 *
 * The per-node prev link is computed branchlessly in the machine code:
 *   DL = (i <= 0);  EDX = DL - 1;  EDX = EDX & &node[i-1]
 * i.e. EDX is &node[i-1] for i>0 and 0 for i==0 (since i is a nonnegative counter,
 * (i<=0) is exactly (i==0)). Expressed here as the equivalent conditional.
 */
void CG_ClearFlameChunks(void)
{
    flameChunk_t *chunks = cg_flameChunks;

    /* Zero the entire flame-chunk pool and the entire per-owner flame-info region
     * (both are cleared wholesale by REP STOSD in the machine code). sizeof is
     * 0x150 on i386 and naturally includes widened list pointers on 64-bit. */
    memset(chunks, 0, (size_t)FLAME_CHUNK_COUNT * sizeof(*chunks));
    memset(cg_flameInfo, 0,
           (size_t)FLAME_INFO_COUNT * sizeof(*cg_flameInfo));

    /* Free list starts at the first node; both other list heads start empty. */
    cg_freeFlameChunks = chunks;
    cg_activeFlameChunks = NULL;
    cg_flameChunkList = NULL;

    /* Thread every node onto the free list: next -> node[i+1], prev -> node[i-1]. */
    for (int32_t i = 0; i < FLAME_CHUNK_COUNT; ++i) {
        chunks[i].next = &chunks[i + 1];
        chunks[i].unresolvedField_1c = 0;
        chunks[i].prev = (i == 0) ? NULL : &chunks[i - 1];
    }

    /* Terminate the last node's forward link (overwrites the &node[i+1] written
     * for i == FLAME_CHUNK_COUNT-1, which pointed one past the pool). */
    chunks[FLAME_CHUNK_COUNT - 1].next = NULL;

    cg_numActiveFlameChunks = 0;
    cg_flameChunksInited = qtrue;
}
