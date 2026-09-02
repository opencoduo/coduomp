// Sources: uo_cgame_mp_x86.dll 0x30008f20..0x30009051,
//          uo_game_mp_x86.dll  0x20008cd0..0x20008e00,
//          game.mp.uo.i386.so  0x00024838..0x000249cb
//
// PM_FlyMove — the pmove free-fly (spectator) movement step. It applies
// friction, builds a full-3D wish-velocity from the command forward/right movement
// bytes rotated into the pmove forward/right basis (using ALL THREE basis
// components, not flattened to horizontal), adds the command up-move and the
// up/down button impulses to the vertical axis, then accelerates the player-state
// velocity toward it and runs the slide/step move WITHOUT gravity.
//
// Name adjudication: the .mcode bank labels this "LookAtKiller" with the note
// "win size 0x131, matched size 0x131". That is a pure SIZE match and is REJECTED
// per the no-size-matching rule (LookAtKiller would be a killer-direction
// vectoangles helper; this function is proven to be a pmove mover with no
// vectoangles call and no killer-entity access). Resolved by BEHAVIOR and CALL
// GRAPH, all from the machine code:
//   * It reads the global pmove context pm (0x30539850) and its command
//     movement bytes movement.forwardmove/rightmove/upmove (+0x18/+0x19/+0x1a) and
//     the second command button byte command.wbuttons (+0x09).
//   * It calls, in order: PM_Friction (0x30008470), PM_CmdScale (0x30008690),
//     VectorNormalize (0x30049700), PM_Accelerate (0x300085d0) with accel = 8.0f,
//     and the slide/step mover PM_StepSlideMove (0x3000f220) with gravity = 0.
//     Friction, command scale, wishvel, normalize, accelerate, slide-with-no-gravity
//     is exactly the Quake3/CoD free-fly (spectator) mover body. Unlike PM_AirMove
//     (0x30009060) it does NOT flatten/renormalize the basis, does NOT clip against a
//     ground plane, and runs no post-move angle fixup.
//   * The pmove pm_type dispatch table at 0x3000e720 (indexed by move->ps->pm_type-1)
//     routes THIS function for table index 3, i.e. pm_type == 4 ==
//     UO_PM_TYPE_SPECTATOR (server pm_type enum). That branch (0x3000e423) calls
//     PM_Friction, the anim gates (0x3000d7a0/0x3000d800/0x3000b010), then this
//     function, confirming the spectator/free-fly role.
//   * The Mac cgame symbol PM_FlyMove has the identical five-function direct-call
//     set: PM_Friction, PM_CmdScale, VectorNormalize, PM_Accelerate, and
//     PM_StepSlideMove. The call fingerprint resolves the source name.
//
// pml.forward/pml.right (0x30539580/0x3053958c) are the pmove-locals basis vectors
// and pml.frametime the current pmove frametime — all resolved in globals.h.
// The pmove context type is pmove_t and its player-state is
// playerState_t (the deferred-rename views of pmove_t/playerState_t documented
// in globals.h), reused here for consistency with the sibling movers.
//
// ABI: __cdecl, no arguments, RET with no immediate (SUB ESP,0x24 frame; lone PUSH
// ESI is register preservation). Callee cleanup of the PM_Accelerate stack args and
// the PM_StepSlideMove arg (ADD ESP,0xc after all three pushes) is a
// calling-convention detail, expressed here as normal C calls.

#include "bg_pmove.h"

#include "compat/coduo_x87emu.h"
#include "math/q_math.h"

#include <stdint.h>

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

/* Original exported Linux BG constant at RVA 0x0009b4d0. Both optimized
 * Windows modules materialize the same binary32 value directly at the call. */
const float pm_flyaccelerate = 8.0f;

/*
 * command.wbuttons (pmove_t +0x09) up/down free-fly impulse bits, consumed only
 * here. Bit 0x20 drives the player DOWN (its impulse is subtracted from the vertical
 * wish-velocity) and bit 0x10 drives the player UP (added). The selected bit
 * value is shifted left four places: 0x20 becomes a 512-unit downward impulse,
 * while 0x10 becomes a 256-unit upward impulse.
 * Exact source usercmd-button-enum names are unresolved; named here by their proven
 * free-fly vertical-move role.
 */
#if defined(WINDOWS_BEHAVIOR)
void PM_FlyMove(void)
{
    /* 0x30008f24: apply friction to the pmove player-state velocity in place. */
    PM_Friction();

    /* 0x30008f29: the pmove context (server pmove_t). */
    pmove_t *move = pm;

    /* 0x30008f2f: LEA ECX,[ESI+4] passes the complete current usercmd embedded
     * at move+0x04 to PM_CmdScale. */
    usercmd_t *cmd = &move->command;

    /* 0x30008f32: scale = PM_CmdScale(&move->command). 0x30008f37: stored as a temp. */
    float scale = PM_CmdScale(cmd);

    /* 0x30008f3b..0x30008f4c: if the command scale is exactly 0.0f, the wish-velocity
     * is zero on all three axes (the FUCOMPP/JP zero-scale branch stores wishvel = {0,0,0}
     * and jumps past the basis rotation). Otherwise build the 3-D wish-velocity from the
     * signed command movement bytes rotated through the full pml.forward/pml.right basis
     * (all three components, NOT flattened), scaled by `scale`, plus the up-move byte. */
    vec3_t wishvel;
    /* The x component remains in ST1 from 0x30008f9d until its sole FSTP at
     * 0x30009018. The z component remains in ST0 through the button-impulse
     * FISUB/FIADD and reaches its sole FSTP at 0x30009024. Carry both live x87
     * values without inventing an earlier memory-format spill. */
#if EMULATE_X87
    x87f wishvelX;
    x87f wishvelZ;
#else
    long double wishvelX;
    long double wishvelZ;
#endif
    if (scale == 0.0f) {
#if EMULATE_X87
        wishvelX = x87f_load_f32(0.0f);
        wishvelZ = x87f_load_f32(0.0f);
#else
        wishvelX = 0.0f;
        wishvelZ = 0.0f;
#endif
        wishvel[1] = 0.0f;
    } else {
        /* 0x30008f67..0x30008f8b: sign-extend the command movement bytes and convert
         * to float (MOVSX + FILD/FST): forwardmove (+0x18 == forwardmove),
         * rightmove (+0x19 == rightmove) are stored (FST/FSTP DWORD) so the
         * (float) casts are faithful; upmove (+0x1a) enters via bare FILD below. */
        float fmove = (float)cmd->forwardmove; /* forwardmove */
        float smove = (float)cmd->rightmove; /* rightmove   */
        int32_t umove = cmd->upmove;      /* upmove (bare FILD, no cast) */

        /* 0x30008f8b..0x30008fbb: per component, evaluated in x87 order
         *   (pml.forward[i]*fmove + pml.right[i]*smove) * scale
         * with the FLD forward / FMUL fmove computed first, then FLD right / FMUL smove
         * FADDP, then FMUL scale. */
#if EMULATE_X87
        wishvelX = x87f_mul(x87f_add(x87f_mul(x87f_load_f32(fmove), x87f_load_f32(pml.forward[0])),
                                     x87f_mul(x87f_load_f32(smove), x87f_load_f32(pml.right[0]))),
                            x87f_load_f32(scale));
        wishvel[1] = x87f_store_f32(x87f_mul(x87f_add(x87f_mul(x87f_load_f32(fmove), x87f_load_f32(pml.forward[1])),
                                                      x87f_mul(x87f_load_f32(smove), x87f_load_f32(pml.right[1]))),
                                             x87f_load_f32(scale)));
#else
        wishvelX = ((long double)pml.forward[0] * (long double)fmove + (long double)pml.right[0] * (long double)smove) * (long double)scale;
        wishvel[1] = (float)(((long double)pml.forward[1] * (long double)fmove + (long double)pml.right[1] * (long double)smove) *
                             (long double)scale);
#endif

        /* 0x30008fbf..0x30008fe5: z picks up the basis-rotated term AND the up-move byte,
         * both scaled: (pml.forward[2]*fmove + pml.right[2]*smove)*scale + upmove*scale
         * (the machine code multiplies the rotated term by scale, then adds
         * upmove*scale via FILD/FMUL/FADDP). */
#if EMULATE_X87
        wishvelZ = x87f_add(x87f_mul(x87f_add(x87f_mul(x87f_load_f32(fmove), x87f_load_f32(pml.forward[2])),
                                              x87f_mul(x87f_load_f32(smove), x87f_load_f32(pml.right[2]))),
                                     x87f_load_f32(scale)),
                            x87f_mul(x87f_load_i32(umove), x87f_load_f32(scale)));
#else
        wishvelZ =
            ((long double)pml.forward[2] * (long double)fmove + (long double)pml.right[2] * (long double)smove) * (long double)scale +
            (long double)umove * (long double)scale;
#endif
    }

    /* 0x30008fe7..0x3000900e: free-fly vertical button impulses. Gated on the
     * player-state base speed being nonzero (ps->speed, +0x48). When enabled, the
     * DOWN button (command.wbuttons & 0x20) subtracts a fixed 512-unit impulse from the
     * vertical wish-velocity and the UP button (command.wbuttons & 0x10) adds a fixed
     * 256-unit impulse. The AND/SHL-4 idiom keeps the impulse exactly 0 when the bit
     * is clear (0x20<<4 == 0x200, 0x10<<4 == 0x100). These integer impulses are
     * FISUB/FIADD applied to wishvel[2] (no (float) cast -- integer operands). */
    playerState_t *ps = move->ps;
    if (ps->speed != 0) {
        int32_t downImpulse = (move->command.wbuttons & PM_WBUTTON_LEAN_RIGHT) << 4;
        int32_t upImpulse = (move->command.wbuttons & PM_WBUTTON_LEAN_LEFT) << 4;
#if EMULATE_X87
        wishvelZ = x87f_add(x87f_sub(wishvelZ, x87f_load_i32(downImpulse)), x87f_load_i32(upImpulse));
#else
        wishvelZ = wishvelZ - downImpulse + upImpulse;
#endif
    }
#if EMULATE_X87
    wishvel[0] = x87f_store_f32(wishvelX); /* 0x30009018 FSTP [ESP+0x10] */
    wishvel[2] = x87f_store_f32(wishvelZ); /* 0x30009024 FSTP [ESP+0x18] */
#else
    wishvel[0] = (float)wishvelX;   /* 0x30009018 FSTP [ESP+0x10] */
    wishvel[2] = (float)wishvelZ;   /* 0x30009024 FSTP [ESP+0x18] */
#endif

    /* 0x30009012..0x30009028: wishdir = wishvel; wishspeed = |wishdir| (3-D). The
     * machine code lays wishvel into a stack vec3 (FXCH reorders the x87 x/z pair so
     * wishdir[0]=x, wishdir[1]=y from [ESP+0x20], wishdir[2]=z) and calls
     * VectorNormalize on it in place (ESI = &wishdir). */
    vec3_t wishdir;
    wishdir[0] = wishvel[0];
    wishdir[1] = wishvel[1];
    wishdir[2] = wishvel[2];
    float wishspeed = VectorNormalize(wishdir);

    /* 0x30009031..0x3000903d: accelerate the player velocity toward wishdir with a
     * free-fly acceleration of 8.0f (the pushed 0x41000000). PM_Accelerate takes
     * wishdir in EDX and pushes wishspeed then accel. */
    PM_Accelerate(wishdir, wishspeed, pm_flyaccelerate);

    /* 0x30009042..0x30009044: slide/step the pmove origin with NO gravity (arg = 0). */
    PM_StepSlideMove(0);
}
#else
/* Linux game RVA 0x00024838..0x000249cb. Unlike the two Windows modules,
 * Linux stores every rotated lane as binary32, stores the up-move addition,
 * and separately stores both vertical button adjustments. Its multiply graph
 * also applies scale before each signed command component. */
void PM_FlyMove(void)
{
    pmove_t *move;
    vec3_t wishvel;
    vec3_t wishdir;
    float wishSpeed;
    float scale;

    PM_Friction();
    move = pm;
    scale = PM_CmdScale(&move->command);

    if (scale == 0.0f) {
        wishvel[0] = 0.0f;
        wishvel[1] = 0.0f;
        wishvel[2] = 0.0f;
    } else {
        const int32_t forwardMove = move->command.forwardmove;
        const int32_t rightMove = move->command.rightmove;

        for (int32_t lane = 0; lane < 3; ++lane) {
#if EMULATE_X87
            wishvel[lane] = x87f_store_f32(
                x87f_add(x87f_mul(x87f_mul(x87f_load_f32(scale), x87f_load_f32(pml.forward[lane])), x87f_load_i32(forwardMove)),
                         x87f_mul(x87f_mul(x87f_load_f32(scale), x87f_load_f32(pml.right[lane])), x87f_load_i32(rightMove))));
#else
            wishvel[lane] = (float)(((long double)scale * (long double)pml.forward[lane]) * (long double)forwardMove +
                                    ((long double)scale * (long double)pml.right[lane]) * (long double)rightMove);
#endif
        }

#if EMULATE_X87
        wishvel[2] =
            x87f_store_f32(x87f_add(x87f_load_f32(wishvel[2]), x87f_mul(x87f_load_i32(move->command.upmove), x87f_load_f32(scale))));
#else
        wishvel[2] = (float)((long double)wishvel[2] + (long double)move->command.upmove * (long double)scale);
#endif
    }

    if (move->ps->speed != 0) {
        const int32_t downImpulse = (move->command.wbuttons & PM_WBUTTON_LEAN_RIGHT) << 4;
        const int32_t upImpulse = (move->command.wbuttons & PM_WBUTTON_LEAN_LEFT) << 4;
#if EMULATE_X87
        wishvel[2] = x87f_store_f32(x87f_sub(x87f_load_f32(wishvel[2]), x87f_load_i32(downImpulse)));
        wishvel[2] = x87f_store_f32(x87f_add(x87f_load_f32(wishvel[2]), x87f_load_i32(upImpulse)));
#else
        wishvel[2] = (float)((long double)wishvel[2] - downImpulse);
        wishvel[2] = (float)((long double)wishvel[2] + upImpulse);
#endif
    }

    wishdir[0] = wishvel[0];
    wishdir[1] = wishvel[1];
    wishdir[2] = wishvel[2];
    wishSpeed = (float)VectorNormalize(wishdir);
    PM_Accelerate(wishdir, wishSpeed, pm_flyaccelerate);
    PM_StepSlideMove(0);
}
#endif
