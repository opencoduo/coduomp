// Source: uo_cgame_mp_x86.dll 0x3001e7f0..0x3001e951
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001e7f0_3001e951.mcode

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

/*
 * CG_EntityEffects(centity_t *cent)
 *
 * Add a client entity's per-frame ambient audio/visual effects: its looping sound
 * and its dynamic light. This is the classic Q3/CoD cgame CG_EntityEffects; the
 * .mcode's size-matched "BG_TakePlayerWeapon" guess is REJECTED — the body reads
 * cgame render state (currentState.constantLight / clientSound / itemIndex),
 * config-string sound aliases, and issues the cgame sound and add-light traps.
 * A BG_ playerState weapon routine has none of that.
 *
 * Argument ABI: cent arrives in EAX (MOV ESI,EAX). No stack parameters. Callee
 * cleans nothing beyond its own frame (RET, no immediate); the two sub-calls and
 * the trap are cdecl and clean their own pushed args.
 *
 * === Looping sound (guarded by currentState.clientSound != 0) ===
 * 3001e7f6 MOV ECX,[ESI+0x84]            clientSound = cent->currentState.clientSound
 * 3001e7fc TEST ECX,ECX / JZ 0x8ac      if 0, skip the sound entirely
 * 3001e805 CMP [ESI+0xa0],0xffffff       solid == SOLID_BMODEL ?
 * 3001e80f LEA EDI,[ECX+0x295]           cfgIndex = clientSound + CS_SOUNDS
 * 3001e815 JNZ 0x875                      != 0xffffff -> use raw lerpOrigin
 *
 * Offset-origin arm (solid == SOLID_BMODEL): build
 *   soundOrigin[k] = cent->lerpOrigin[k] + cg_inlineModelMidpoints[modelindex][k]
 * with x87 float adds (FLD [ESI+0x208+4k]; FADD table[k]; FSTP slot), into the
 * three stack floats at [ESP+0x18/0x1c/0x20]. The channel object handed to
 * CG_PlaySoundAliasByName is &soundOrigin (LEA ECX,[ESP+0x1c]).
 *
 * Raw-origin arm (solid != SOLID_BMODEL): the channel object is
 * &cent->lerpOrigin at cent+0x208 directly (LEA ECX,[ESI+0x208]).
 *
 * Both arms: an out-of-range cfgIndex is reported via the inlined CG_ConfigString
 * bounds check (JL cfgIndex<0 ; CMP 0x800 ; JL) and still falls through to the
 * lookup, exactly as CG_ConfigString does. Then:
 *   3001e898 MOV EAX,[EDI*4 + cg_gameState.stringOffsets]   offset = offsets[cfgIndex]
 *   3001e89f ADD EAX,cg_gameState.stringData                soundName = &stringData[offset]
 *   entityNum = cent->currentState.number ([ESI]) pushed; call CG_PlaySoundAliasByName
 *   (custom ABI: channelObj in ECX, soundName in EAX, entityNum on the stack).
 *
 * === Dynamic light (guarded by currentState.constantLight != 0) ===
 * 3001e8ac MOV EAX,[ESI+0x80]            constantLight (packed 0xAABBGGRR)
 * 3001e8b2 TEST EAX,EAX / JZ 0x94b       if 0, no light
 * The packed dword is unpacked (all channel extracts are unsigned AND 0xff except
 * the top byte via SHR 24):
 *   r = (constantLight        & 0xff) * (1/255)
 *   g = ((constantLight >> 8)  & 0xff) * (1/255)
 *   b = ((constantLight >> 16) & 0xff) * (1/255)
 *   intensity = (float)((constantLight >> 24) << 2)     (SHR 24 unsigned, then *4)
 * Note the channel ordering in the machine code: byte2 (bits16-23) is converted
 * first and its normalized value is pushed LAST (deepest = arg 'b'); byte0 (bits0-7)
 * is pushed as the 'r' argument. ESI is advanced to &cent->lerpOrigin (ADD ESI,0x208)
 * before the trap, and that origin is the light position. The 5 floats plus the
 * origin pointer are pushed with id 0x42 (CG_R_ADD_LIGHT_TO_SCENE), six dwords
 * cleaned (ADD ESP,0x18).
 *
 * Constant: 0x3007be24 is the float 1/255 (0x3b808081); written here as the natural
 * source form (1.0f/255.0f), which clang rounds to the identical single-precision
 * value the multiply uses.
 */

/* 1/255 color-normalization factor (float const at .rdata 0x3007be24 = 0x3b808081). */
#define BYTE_TO_UNIT (0.00392156886f)

void CG_EntityEffects(centity_t *cent)
{
    int clientSound = cent->currentState.clientSound;

    /* --- Looping sound --- */
    if (clientSound != 0) {
        int32_t cfgIndex = coduo_int32_from_bits((uint32_t)clientSound + (uint32_t)CS_SOUNDS);
        const char *soundName;
        void *channelObj;
        vec3_t soundOrigin;

        if ((uint32_t)cent->currentState.solid == SOLID_BMODEL) {
            /* For a brush model, emit at lerpOrigin plus its registered midpoint. */
            const vec_t *ofs = cg_inlineModelMidpoints[cent->currentState.itemIndex];
            const vec_t *lerp = cent->lerpOrigin; /* cent+0x208, consumed here as origin */
            soundOrigin[0] = (float)((long double)lerp[0] + (long double)ofs[0]);
            soundOrigin[1] = (float)((long double)lerp[1] + (long double)ofs[1]);
            soundOrigin[2] = (float)((long double)lerp[2] + (long double)ofs[2]);
            channelObj = soundOrigin;
        } else {
            /* Emit at the raw render origin (cent+0x208). */
            channelObj = cent->lerpOrigin;
        }

        /* Inlined CG_ConfigString bounds check: report out-of-range, then still
         * look up (the machine code falls through into the lookup). Signed compare:
         * cfgIndex < 0 || cfgIndex >= MAX_CONFIGSTRINGS. */
        if (cfgIndex < 0 || cfgIndex >= MAX_CONFIGSTRINGS) {
            Com_ErrorMessage("CG_ConfigString: bad index: %i", cfgIndex);
        }

        soundName = &cg_gameState.stringData[cg_gameState.stringOffsets[cfgIndex]];
        CG_PlaySoundAliasByName(cent->currentState.number, channelObj, soundName);
    }

    /* --- Dynamic light --- */
    {
        uint32_t constantLight = cent->currentState.constantLight;
        if (constantLight != 0) {
            /* 0x3001e8c9/0x3001e8f5/0x3001e91a: each masked byte is FILDed
             * straight into the FMUL by 1/255 with no intermediate FSTP DWORD, so
             * no (float) cast (it would round under -std=c11; the byte enters exact). */
            float b = (float)((long double)((constantLight >> 16) & 0xffu) * (long double)BYTE_TO_UNIT);
            float g = (float)((long double)((constantLight >> 8) & 0xffu) * (long double)BYTE_TO_UNIT);
            float r = (float)((long double)(constantLight & 0xffu) * (long double)BYTE_TO_UNIT);
            /* 0x3001e92d FILD; 0x3001e936 FSTP DWORD: intensity IS rounded to
             * float (no multiply follows), so the (float) conversion is kept. */
            float intensity = (float)((constantLight >> 24) << 2);

            cgame_syscall(CG_R_ADD_LIGHT_TO_SCENE, (intptr_t)cent->lerpOrigin, /* &cent->lerpOrigin (cent+0x208) */
                          CG_FloatBits(intensity), CG_FloatBits(r), CG_FloatBits(g), CG_FloatBits(b));
        }
    }
}
