#include "../client_recovered.h"
#include "../globals.h"

// Source: uo_cgame_mp_x86.dll 0x3002ab00..0x3002abbd
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002ab00_3002abbd.mcode
//
// CG_AddMovingTracer — the LE_MOVING_TRACER (leType == 2) local-entity render
// handler dispatched by CG_AddLocalEntities (0x3002ad00, case 2). Register ABI:
// the localEntity_t * arrives in ESI (MOV ESI,EAX at 0x3002ab04 copies the
// dispatch loop's EAX/ESI). No stack args, no return value (bare RET).
//
// Behavior: pick a (width, length) tracer parameter pair by le->leFlags, bail if
// either is negligibly small, then evaluate the entity's position trajectory at
// cg_time, march that point `length` units along the normalized trajectory
// velocity (le->pos.trDelta), and hand the marched end point plus `width` to the
// tracer polygon builder (0x30048460).
//
// NAME: the .mcode size-guess name "PM_SwitchIfEmpty" is REJECTED — it was matched
// only by byte size (win 0xbd ~ corpus 0xbc), which the naming rules forbid, and
// PM_SwitchIfEmpty is a pmove weapon-switch routine with no x87/trajectory work.
// This function does no pmove state access; it reads a localEntity_t trajectory,
// normalizes a velocity, and builds a stretched tracer. The adopted name
// CG_AddMovingTracer comes from the same-module PPC bank
// (cgame_mp!CG_AddMovingTracer, the function immediately after CG_AddFadeRGB in
// that bank), corroborated by this behavior and by its dispatch slot (case 2,
// sibling to CG_AddFadeRGB at case 0).
//
// The four tracer parameter globals (cg_tracerWidth/Length ModeA/ModeB) are
// read-only floats with no writer anywhere in this DLL's .text (loaded via
// `MOV reg,ds:addr` / `FLD float ptr` only); they are cvar-like tunables set by an
// initializer outside .text. Their exact source names are unresolved, so they
// keep an address suffix. See globals.h for the type repair (uint32_t -> float).

void CG_AddMovingTracer(localEntity_t *le)
{
    float width;   /* [ESP+0x8] "int" slot: mode-selected tracer width (float bits) */
    float length;  /* ST0 / [ESP+0x4]:     mode-selected tracer march length         */
    vec3_t pos;    /* [ESP+0x14]: BG_EvaluateTrajectory result (world position)       */
    vec3_t dir;    /* [ESP+0x20]: normalized le->pos.trDelta (unit velocity)          */
    vec3_t endPoint; /* [ESP+0x34..0x3c]: pos + length*dir, the tracer head point     */

    /*
     * 0x3002ab06-0x3002ab2d: select the (width, length) pair from leFlags.
     *   CMP dword [le+0xc],0x20 ; JNZ  -> exact leFlags == LEF_TRACER_MODE_A test.
     * On == : width = cg_tracerwidthlmg_vmCvar.value (0x304407c8), length = cg_tracerlengthlmg_vmCvar.value (0x304506e8).
     * On != : width = cg_tracerwidth_vmCvar.value (0x3048c268), length = cg_tracerlength_vmCvar.value (0x30530668).
     * `length` is the float held live in ST0 (FLD); `width` is stored to a stack
     * slot as raw dword (MOV) and reloaded as a float below.
     */
    if (le->leFlags == LEF_TRACER_MODE_A) {
        width  = cg_tracerwidthlmg_vmCvar.value;
        length = cg_tracerlengthlmg_vmCvar.value;
    } else {
        width  = cg_tracerwidth_vmCvar.value;
        length = cg_tracerlength_vmCvar.value;
    }

    /*
     * 0x3002ab2d-0x3002ab4d: gate on both parameters being non-negligible.
     *   FST [esp+4] ; FCOMP [1e-06] ; FNSTSW AX ; TEST AH,5 ; JNP exit  (length)
     *   FLD [esp+8] ; FCOMP [1e-06] ; FNSTSW AX ; TEST AH,5 ; JNP exit  (width)
     * The comparand at 0x3007bff4 is the shared .rdata float 1e-06. After FCOMP,
     * TEST AH,5 masks C0|C2; JNP is taken exactly when ST0 < mem (C0 set, one bit,
     * PF=0), so the function returns when either parameter is strictly below the
     * epsilon. Unordered values also proceed, so the exact predicate is that
     * neither parameter is ordered below 1e-06.
     */
    if (length < 1e-6f) {
        return;
    }
    if (width < 1e-6f) {
        return;
    }

    /*
     * 0x3002ab4f-0x3002ab5d: BG_EvaluateTrajectory(&le->pos, cg_time, pos).
     * Register ABI proven by the callee (0x30005f30): result in ECX (LEA ECX,[ESP+0x14]),
     * trajectory in EBX (LEA EBX,[ESI+0x18] == &le->pos), atTime in EAX (MOV EAX,[cg_time]).
     */
    BG_EvaluateTrajectory(&le->pos, coduo_int32_from_bits(cg_time), pos);

    /*
     * 0x3002ab62-0x3002ab6e: VectorNormalize2(le->pos.trDelta, dir).
     * in in EDI (LEA EDI,[ESI+0x30] == &le->pos.trDelta), out in ESI (LEA ESI,[ESP+0x20]).
     * The returned length (left in ST0) is discarded (FSTP ST0 at 0x3002ab6e).
     */
    (void)VectorNormalize2(le->pos.trDelta, dir);

    /*
     * 0x3002ab70-0x3002abaa: endPoint = pos + length*dir  (a VectorMA).
     *   endPoint[i] = dir[i]*length + pos[i], component by component
     *   (FLD dir[i] ; FMUL length ; FADD pos[i] ; FSTP endPoint[i]).
     */
    endPoint[0] = (float)(
        (long double)dir[0] * length + pos[0]);
    endPoint[1] = (float)(
        (long double)dir[1] * length + pos[1]);
    endPoint[2] = (float)(
        (long double)dir[2] * length + pos[2]);

    /*
     * 0x3002ab7c-0x3002abb3: CG_DrawMovingTracerPoly(pos, endPoint, width).
     *   LEA EDI,[&pos] (the tail point, register arg) ; PUSH EDX (= width dword,
     *   reloaded from the int slot) ; PUSH EAX (= &endPoint, the head point) ;
     *   CALL 0x30048460 ; ADD ESP,0x8  (the two stack args are caller-cleaned; the
     *   EDI point is a register argument). The callee computes endPoint - pos
     *   ([EBP]-[EDI]) as the segment direction, confirming pos is the tail point.
     * The width dword is the mode-selected width float's bit pattern, which the
     * callee reads back as `float ptr` — so passing the float value reproduces it.
     */
    CG_DrawMovingTracerPoly(pos, endPoint, width);
}
