#include "../client_recovered.h"
#include "../globals.h"
#include "compat/coduo_native_x87.h"

enum { CG_WHIZBY_ENTITYNUM_WORLD = 1022 };

// Source: uo_cgame_mp_x86.dll 0x300480f0..0x30048259
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300480f0_30048259.mcode
//
// CG_WhizbySound — play the "whizby" bullet fly-by sound near the listener
// when a shot's trajectory passes close to the current view origin.
//
// The size-match name "VEH_GroundMove" (win 0x168 == a game_mp.dll size) is
// REJECTED per naming policy: it carries no behavioral basis and this is cgame
// code that emits a sound, not a vehicle ground-move step. The prior caller-guess
// decl name "CG_TracerMuzzleFlashDist" is also superseded — the bytes prove no
// "distance" is returned (the function is void) and the emitted effect is a sound,
// not a muzzle flash. The name is derived from behavior + the emitted sound handle:
//   * the sole caller CG_SpawnTracer (0x30048d60) passes impactOrigin (EAX) and the
//     shot muzzle point (EBX);
//   * the geometry projects the view origin onto the shot ray and gates on close
//     approach to the listener;
//   * the played sound handle read from 0x3044c1e4 is registered from the string
//     "whizby" (0x30078590, RegisterSound trap 0xc3 at 0x3002b943), so this is the
//     bullet fly-by ("whizby") sound.
// The Mac cgame symbol CG_WhizbySound has the identical two named direct callees,
// providing an independent cross-architecture name match.
//
// Register-argument ABI (proven from the bytes and the caller's LEA sequence, not
// cdecl): the impact-origin pointer arrives in EAX and the muzzle-point pointer in
// EBX (both caller-set; EBX is used without ever being loaded in this function).
// No stack arguments; plain RET. Modeled as two const vec3_t.
//
// Callees (identities reused per provisional-decl policy, call shapes re-derived
// from these bytes):
//   0x30049920 VectorNormalize2(const vec3_t in, vec3_t out): in=EDI=&diff,
//     out=ESI=&dir; the FSTP ST0 at 0x3004811e discards its returned length.
//   0x3002ca80 CG_PlaySoundAliasByName(void *channelObj, const char *soundName,
//     int entityNum): ECX=channelObj=&emitPoint, EAX=soundName=whizby handle,
//     pushed stack arg=0x3fe (the entity/loop number). The result is discarded.
//
// .rdata float constants (dumped via objdump -s -j .rdata; never guessed):
//   0x3007bec8 = 140.0f   (max close-approach distance to the listener)
//   0x3007bf00 =  16.0f   (pull-back along the ray for the emit point)
//   0x3007bffc = -64.0f   (t + 64 forward-clearance term)
//   0x3007c000 =  64.0f   (min projection t along the ray)
//
// Globals:
//   0x30487a90/94/98 = cg_refdef.vieworg (the current view/listener origin vec3).
//   0x3044c1e4       = the registered "whizby" sound handle (soundName arg).

void CG_WhizbySound(const vec3_t impactOrigin, const vec3_t muzzle)
{
    /* 0x300480f3..0x30048115: diff = impactOrigin - muzzle (the shot-travel
     * vector from the muzzle toward the impact), stored to [ESP+0x18..]. */
    vec3_t diff;
    diff[0] = impactOrigin[0] - muzzle[0];
    diff[1] = impactOrigin[1] - muzzle[1];
    diff[2] = impactOrigin[2] - muzzle[2];

    /* 0x30048119: dir = normalize(diff). VectorNormalize2 writes the unit ray
     * direction to the local at [ESP+0xc..]; its returned length (ST0) is FSTP'd
     * away (0x3004811e), so only the direction is used. */
    vec3_t dir;
    VectorNormalize2(diff, dir);

    /* 0x30048120..0x3004814e: t = dot(cg_refdef.vieworg - muzzle, dir) — the scalar
     * projection of the listener onto the shot ray measured from the muzzle. The
     * three (viewOrg - muzzle) terms are each multiplied by the matching dir
     * component and summed in one 80-bit-register chain (FMUL dir.z / FXCH /
     * FMUL dir.y / FADDP / FXCH / FMUL dir.x / FADDP). 0x3004814e is an FST
     * (keep), not FSTP: it writes a float-rounded copy of t to [ESP+0x8] and
     * leaves the unstored value in ST0. `long double` is the established source
     * carrier for that x87 register residency; the runtime control word governs
     * the precision of the arithmetic instructions that honor PC. */
    long double t =
        ((long double)cg_refdef.vieworg[2] - (long double)muzzle[2]) *
            (long double)dir[2] +
        ((long double)cg_refdef.vieworg[1] - (long double)muzzle[1]) *
            (long double)dir[1] +
        ((long double)cg_refdef.vieworg[0] - (long double)muzzle[0]) *
            (long double)dir[0];
    float tRounded = (float)t; /* 0x3004814e FST DWORD [ESP+0x8] */

    /* 0x30048152..0x3004815d: FCOMP 64.0 / TEST AH,5 / JNP return, comparing the
     * unrounded 80-bit t (still in st0) against 64.0f. The ordered compare jumps out
     * (does nothing) when t < 64.0f, so the listener must be at least 64 units past
     * the muzzle along the ray. */
    if (t < 64.0f) {
        return;
    }

    /* 0x30048163..0x30048190: require dot(dir, diff) >= tRounded + 64.0f — the shot
     * must travel far enough that the closest-approach point lies within the segment
     * (plus 64 units of forward clearance). diff is the un-normalized travel vector,
     * so dot(dir, diff) is the full shot length along the ray. rayLength is built in
     * one x87-register chain and FCOMPP'd directly; the t+64 side reloads the
     * rounded copy (FSUB -64.0f @0x30048167).
     * FCOMPP / TEST AH,5 / JNP return jumps out when the length is < tRounded + 64. */
    long double rayLength =
        (long double)dir[2] * (long double)diff[2] +
        (long double)dir[1] * (long double)diff[1] +
        (long double)dir[0] * (long double)diff[0];
    if (rayLength < (long double)tRounded + 64.0f) {
        return;
    }

    /* 0x30048196..0x300481b7: point = muzzle + tRounded*dir — the closest point on
     * the ray to the listener. point[0] is left on the x87 stack without a
     * memory-format store (carried in point0); point[1]/point[2] are FSTP'd to float slots
     * ([ESP+0x1c] @0x300481ab, [ESP+0x20] @0x300481ba). All three use the reloaded
     * rounded tRounded ([ESP+0x8]). */
    long double point0 =
        (long double)muzzle[0] +
        (long double)tRounded * (long double)dir[0];
    vec3_t point; /* [0] never stored -- carried in point0 above */
    point[1] = (float)((long double)muzzle[1] +
                       (long double)tRounded * (long double)dir[1]);
    point[2] = (float)((long double)muzzle[2] +
                       (long double)tRounded * (long double)dir[2]);

    /* 0x300481be..0x300481fd: dist = |cg_refdef.vieworg - point|; FCOMP 140.0 /
     * TEST AH,0x41 / JZ skip. ex/ey/ez and the FSQRT result stay on the x87
     * stack (no float store), so the native-x87 adapter emits the inline FSQRT at
     * 0x300481ea. ex uses the unrounded point0; ey/ez use the reloaded rounded
     * point[1]/point[2]. The mask
     * C3|C0 with JZ skips (does nothing) when dist > 140.0f, so the sound fires only
     * when the closest approach is within 140 units (dist <= 140.0f, equal included). */
    long double ex = point0 - cg_refdef.vieworg[0];
    long double ey =
        (long double)point[1] - (long double)cg_refdef.vieworg[1];
    long double ez =
        (long double)point[2] - (long double)cg_refdef.vieworg[2];
    long double dist = coduo_x87_sqrtl(
        ez * ez + ey * ey + ex * ex);
    if (dist > 140.0f) {
        return;
    }

    /* 0x300481ff..0x3004823f: emitPoint = point - 16.0f*dir — the closest point
     * pulled 16 units back toward the muzzle. Each component is (point[i] -
     * 16.0f*dir[i]) (FMUL 16.0 then FSUBR against the point component; FSTP DWORD is
     * the one rounding per component). emitPoint[0] uses the unrounded point0;
     * emitPoint[1]/[2] use the rounded point[1]/point[2]. ECX = &emitPoint. */
    vec3_t emitPoint;
    emitPoint[0] = (float)(point0 - 16.0f * (long double)dir[0]);
    emitPoint[1] = (float)((long double)point[1] -
                           16.0f * (long double)dir[1]);
    emitPoint[2] = (float)((long double)point[2] -
                           16.0f * (long double)dir[2]);

    /* 0x30048203 / 0x3004820e / 0x30048243: play the "whizby" sound at emitPoint.
     * ECX=&emitPoint (channelObj), EAX=whizby handle (0x3044c1e4), pushed arg=0x3fe
     * (entity/loop number). The returned started-sound number is discarded. */
    CG_PlaySoundAliasByName(CG_WHIZBY_ENTITYNUM_WORLD,
                            emitPoint, cg_soundWhizby);
}
