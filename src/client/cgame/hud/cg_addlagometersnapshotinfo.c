// Source: uo_cgame_mp_x86.dll 0x30018a40..0x30018a8a
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30018a40_30018a8a.mcode
//
// CG_AddLagometerSnapshotInfo — record one engine snapshot in the cgame lagometer
// snapshot ring (cg_lagometer). Standard Quake3/CoD lagometer bookkeeping.
//
// Name evidence: the mechanical .mcode name "Vector4Scale" (a size-guess) is
// REJECTED — the body contains no x87 float ops at all. It is a two-parallel-ring
// writer that masks a running index with 0x7f (& (LAG_SAMPLES-1), 128 samples) and
// stores integer fields of a snapshot. The consumer/draw pass at 0x30018e00 reads
// the same rings: snapshotSamples (0x305382a4) via FILD (an integer latency value,
// with -1 as the dropped-snapshot sentinel) and snapshotFlags (0x305380a4) via
// TEST BYTE,0x1 (per-snapshot flag bits). This is exactly Q3
// CG_AddLagometerSnapshotInfo(snapshot_t *snap). The lagometer ring, LAG_SAMPLES,
// and the cg_lagometer storage are declared in globals.h.
//
// ABI: `snap` arrives in EAX (register-passed, not a stack slot); the function
// takes no stack arguments and ends with a bare RET (no callee stack cleanup).
//
// Machine-code notes:
//   NULL path (TEST EAX,EAX / JNZ):
//     30018a44  MOV EAX,[0x305384a4]                 read snapshotCount
//     30018a49  AND EAX,0x7f                          index = count & (LAG_SAMPLES-1)
//     30018a4c  MOV [EAX*4 + 0x305382a4],0xffffffff   snapshotSamples[index] = -1
//     30018a57  INC [0x305384a4]                      snapshotCount++
//   snap != NULL path:
//     30018a5e  MOV ECX,[0x305384a4]; AND ECX,0x7f    index = count & (LAG_SAMPLES-1)
//     30018a64  MOV EDX,[EAX+0x4]                     snap->ping
//     30018a6a  MOV [ECX*4 + 0x305382a4],EDX          snapshotSamples[index] = snap->ping
//     30018a71  MOV ECX,[0x305384a4]; AND ECX,0x7f    re-read count, same masked index
//     30018a77  MOV EDX,[EAX]                         snap->snapFlags
//     30018a7c  MOV [ECX*4 + 0x305380a4],EDX          snapshotFlags[index] = snap->snapFlags
//     30018a83  INC [0x305384a4]                      snapshotCount++
//   The count is not incremented between the two stores, so both use the same slot;
//   the second re-read of snapshotCount yields the same masked index as the first.
//   snapshotSamples[] holds a signed latency sample compared as an int by the draw
//   pass, so the -1 sentinel is written as int32_t -1 (the 0xffffffff immediate).

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

void CG_AddLagometerSnapshotInfo(snapshot_t *snap)
{
    if (snap == NULL) {
        uint32_t slot = (uint32_t)cg_lagometer.snapshotCount &
                        (uint32_t)(LAG_SAMPLES - 1);
        cg_lagometer.snapshotSamples[slot] = -1;
        cg_lagometer.snapshotCount = coduo_int32_from_bits(
            (uint32_t)cg_lagometer.snapshotCount + 1u);
        return;
    }

    uint32_t sampleSlot = (uint32_t)cg_lagometer.snapshotCount &
                          (uint32_t)(LAG_SAMPLES - 1);
    cg_lagometer.snapshotSamples[sampleSlot] = snap->ping;
    /* 0x30018a71 reloads snapshotCount before the second mask. */
    uint32_t flagSlot = (uint32_t)cg_lagometer.snapshotCount &
                        (uint32_t)(LAG_SAMPLES - 1);
    cg_lagometer.snapshotFlags[flagSlot].word = coduo_int32_from_bits(snap->snapFlags);
    cg_lagometer.snapshotCount = coduo_int32_from_bits(
        (uint32_t)cg_lagometer.snapshotCount + 1u);
}
