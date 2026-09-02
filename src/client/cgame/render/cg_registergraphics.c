// Source: uo_cgame_mp_x86.dll 0x3002ba50..0x3002c988
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3002ba50_3002c988.mcode
//
// CG_RegisterGraphics — cgame render/effect media precache for a map load.
//
// NAME ADJUDICATION: the .mcode header carries a size-guessed working name
// "PM_UpdateViewAngles" (win size 0xf38 matched game_mp_uo size 0xffb). That name is
// REJECTED per the no-size-match rule: this function registers HUD/hint/objective/
// headicon shaders, the 2D number font, tank/jeep tread + flesh impact effects, and
// precaches the CS_MODELS / CS_EFFECTS / CS_SHELLSHOCKS config strings plus the inline
// "*N" brush models. It is the classic id-Tech/CoD cgame CG_RegisterGraphics, proven
// by the registered asset strings ("lagometer", "hintActivate", "hudStanceStand",
// "levelshots/%s.tga", "menu/art/unknownmap", the " - textures"/" - models"/" - items"/
// " - inline models"/" - server models"/" - game media done" CG_LoadingString markers)
// and by the call graph (CG_RegisterItems, CG_RegisterImpactEffects, CG_ShellShockLoad).
//
// ABI: __cdecl, no args, no return. /GS-protected: on entry the routine snapshots the
// MSVC __security_cookie into a frame slot ([ESP+0xa0]) and verifies it via
// __security_check_cookie (0x30061639) on exit. Those two facts are calling-convention
// crust (recorded here, not modelled as source statements) — see the comments below.
//
// LOADING-SCREEN INLINING: three ~150-instruction blocks (0x3002bddf.., 0x3002c003..,
// 0x3002c237..) are byte-for-byte the inlined body of the load-screen updater at
// 0x3002a530 (guarded by cg_snap==0 && cg_updateScreenActive==0, clearing the
// cl_serverload* cvars, drawing the levelshot + hunk-usage progress bar, and issuing
// trap_UpdateScreen). The original source called that helper by name; the compiler
// inlined it. We represent each occurrence as a call to the existing 0x3002a530 symbol
// (CG_DrawInformation in client_recovered.h) — the source-level
// shape — and keep the inlined machine-code details in that helper's own reconstruction
// (0x3002a530), not repeated three times here.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

#include <stdint.h>
#include <string.h>

/* Precache loop iteration counts, proven exactly from the loop guards (the guard tests
 * the config index against a rebased limit, which yields one fewer iteration than the raw
 * limit): the models loop runs 255 times (config 406..660), the effects loop 79 times
 * (writing cg_effectDefs[1..79]), the shellshock loop 15 times (config 1254..1268). */
enum {
    NUM_GAME_MODELS = CS_MODELS_COUNT - 1,
    NUM_EFFECTS = CS_EFFECTS_COUNT - 1,
    NUM_SHELLSHOCKS = CS_SHELLSHOCKS_COUNT - 1,
    NUM_HUD_NUMBER_SHADERS = 11 /* '0'..'9' and '-' */
};

/* The SHADER_LIGHTMAP_* registration-type values formerly defined here were
 * promoted to client_recovered.h (next to CG_RegisterMaterial) once a second
 * consumer (CG_DrawCrosshair) needed them. */

/* Convenience: register a 2D HUD/icon shader via the load-screen-pumping wrapper at
 * 0x3003db80 (CG_RegisterMaterial, the generic material-registration wrapper;
 * it forwards (name, lightmap) to trap CG_REGISTER_MATERIAL and returns the qhandle_t).
 * Every "PUSH lightmap; PUSH name; CALL 0x3003db80; ADD ESP,8; MOV [slot],EAX" site
 * below is exactly one of these calls. */

/* Bounds returned by the inline-model commit trap live in these two adjacent stack vec3
 * slots at the call site; the midpoint (a+b)*0.5 per axis is stored to
 * cg_inlineModelMidpoints[i]. Modelled as locals in the loop below. */

/* Each constant CG_LoadingString call was inlined by the retail compiler. The
 * standalone body at 0x3002a4e0 has the same 1023-byte CRT strncpy, explicit
 * terminator, print, and update-screen behavior for these nonempty literals, so
 * use that recovered source function instead of inventing a second helper. */

/* The three inlined load-screen updates are the body of CG_DrawInformation at
 * 0x3002a530, inlined by the compiler: levelshot, loading text/progress and
 * trap_UpdateScreen under the cg_snap/cg_updateScreenActive guards. Each block
 * first returns when cg_snap is non-NULL and clears the three cl_serverload*
 * cvars when it runs, proving force==qfalse. */

void CG_RegisterGraphics(void)
{
    int32_t i;

    /* --- /GS prologue: __security_cookie snapshotted to frame slot [ESP+0xa0]. --- */

    /* "^5---------- Fx System Initialization ---------\n" then begin the Fx system. */
    Com_Printf("^5---------- Fx System Initialization ---------\n");
    cgame_syscall(CG_FX_INIT_SYSTEM);                       /* trap 0xeb: begin/reset Fx system */
    Com_Printf("^5----- Fx System Initialization Complete -----\n");

    /* Textures phase banner: the opening CG_LoadingString(" - textures"). */
    CG_LoadingString(" - textures");

    /* HUD bitmap-number font: 11 shaders ('0'..'9', then '-'), indexed by ESI = 0,4,..,0x28.
     * The names come from the initialized global pointer table cg_numberShaderNames[i]
     * (read as [ESI + 0x30085d70]); each is registered into cg_numberShaders[i]. */
    for (i = 0; i < NUM_HUD_NUMBER_SHADERS; i++) {
        cg_numberShaders[i] = CG_RegisterMaterial(cg_numberShaderNames[i], R_IMAGE_TRACK_HUD);
    }

    /*
     * Flat 2D HUD/icon shader registration. Each `slot = CG_RegisterMaterial(name, lm)`
     * below pairs the destination global with the shader NAME the machine code actually
     * registers into that slot's address — proven by tracking which registration's result is
     * in EAX at each `MOV [slot],EAX` (the store lags one register call behind the operand
     * scheduling, but the source-level pairing is unambiguous).
     *
     * FINDING (systematic off-by-one in the mechanical cgs_media_* / stance-array LABELS):
     * many of the destination SYMBOL NAMES below were auto-derived from the .rdata string
     * spatially adjacent to the store, which is the NEXT shader, not the one whose handle is
     * stored. So e.g. the slot named `cgs_media_selectShader` (0x3044b6dc) actually holds the
     * "gfx/2d/select" handle only by coincidence of this fix; the slot named `cgs_media_tracerShader`
     * (0x3044b6e4) holds "gfx/misc/tracer"; and the `cg_stanceHudShaders[9]` label comment
     * ([0]=hudStanceCrouch..) is shifted — element [0] (0x3044bb24) actually receives
     * "hudStanceStand". The CONSUMER-anchored names (cgs_disconnectedIcon @0x3044b6d4,
     * cgs_youInKillCamIcon @0x3044b6d8, cg_objectiveShaders, cgs_media_backTileShader) are
     * correct. The bindings below follow the machine code; the shifted label COMMENTS in
     * globals.h should be reconciled by the coordinator (not renamed here to bound blast radius).
     */
    cgs_media_lagometerShader = CG_RegisterMaterial("gfx/2d/net.tga", R_IMAGE_TRACK_HUD);   /* 0x3044b6e8 */
    cgs_lagometerShader = CG_RegisterMaterial("lagometer", R_IMAGE_TRACK_HUD);
    cgs_disconnectedIcon = CG_RegisterMaterial("headiconDisconnected", R_IMAGE_TRACK_HUD);   /* 0x3044b6d4 */
    cgs_youInKillCamIcon = CG_RegisterMaterial("headiconYouInKillCam", R_IMAGE_TRACK_HUD);   /* 0x3044b6d8 */

    /* 0x3002bb3b-0x3002bbc2: twelve consecutive registrations whose handles are
     * deliberately DISCARDED (no MOV between the CALLs) — precache side effect
     * only; the kill-feed / binocular / artillery icons resolve by name later. */
    (void)CG_RegisterMaterial("killIconMelee", R_IMAGE_TRACK_HUD);            /* 0x30077544 */
    (void)CG_RegisterMaterial("killIconSuicide", R_IMAGE_TRACK_HUD);          /* 0x300774ec */
    (void)CG_RegisterMaterial("killIconFalling", R_IMAGE_TRACK_HUD);          /* 0x300774dc */
    (void)CG_RegisterMaterial("killIconCrush", R_IMAGE_TRACK_HUD);            /* 0x300774cc */
    (void)CG_RegisterMaterial("killIconCrushTank", R_IMAGE_TRACK_HUD);        /* 0x300774b8 */
    (void)CG_RegisterMaterial("killIconCrushJeep", R_IMAGE_TRACK_HUD);        /* 0x300774a4 */
    (void)CG_RegisterMaterial("killIconDrown", R_IMAGE_TRACK_HUD);            /* 0x30077494 */
    (void)CG_RegisterMaterial("killIconSlime", R_IMAGE_TRACK_HUD);            /* 0x300782f8 */
    (void)CG_RegisterMaterial("killIconDied", R_IMAGE_TRACK_HUD);             /* 0x30077554 */
    (void)CG_RegisterMaterial("killIconHeadShot", R_IMAGE_TRACK_HUD);         /* 0x300774fc */
    (void)CG_RegisterMaterial("gfx/icons/hud@bino_owned", R_IMAGE_TRACK_HUD); /* 0x30077528 */
    (void)CG_RegisterMaterial("gfx/icons/hud@artillery", R_IMAGE_TRACK_HUD);  /* 0x30077510 */

    cgs_media_tracerShader = CG_RegisterMaterial("gfx/misc/tracer", R_IMAGE_TRACK_EFFECT);     /* 0x3044b6e4, lm 4 */
    cgs_media_selectShader = CG_RegisterMaterial("gfx/2d/select", R_IMAGE_TRACK_HUD);          /* 0x3044b6dc */
    cgs_media_usableHintShaders[CURSOR_HINT_ACTIVATE] = CG_RegisterMaterial("hintActivate", R_IMAGE_TRACK_HUD);
    cgs_media_usableHintShaders[CURSOR_HINT_NOACTIVATE] = CG_RegisterMaterial("hintNoActivate", R_IMAGE_TRACK_HUD);
    cgs_media_usableHintShaders[CURSOR_HINT_DOOR] = CG_RegisterMaterial("hintDoor", R_IMAGE_TRACK_HUD);
    cgs_media_usableHintShaders[CURSOR_HINT_DOOR_LOCKED] = CG_RegisterMaterial("hintNoDoor", R_IMAGE_TRACK_HUD);
    cgs_media_usableHintShaders[CURSOR_HINT_MG42] = CG_RegisterMaterial("hintMg42", R_IMAGE_TRACK_HUD);
    cgs_media_usableHintShaders[CURSOR_HINT_LMG] = CG_RegisterMaterial("hintLMGMount", R_IMAGE_TRACK_HUD);
    cgs_media_usableHintShaders[CURSOR_HINT_HEALTH] = CG_RegisterMaterial("hintHealth", R_IMAGE_TRACK_HUD);
    cgs_media_usableHintShaders[CURSOR_HINT_LADDER] = CG_RegisterMaterial("hintLadder", R_IMAGE_TRACK_HUD);
    cg_weaponHudIcons[0] = CG_RegisterMaterial("hintFriendly", R_IMAGE_TRACK_HUD);           /* 0x3044b720 */

    cg_stanceHudShaders[0] = CG_RegisterMaterial("hudStanceStand", R_IMAGE_TRACK_HUD);         /* 0x3044bb24 */
    cg_stanceHudShaders[1] = CG_RegisterMaterial("hudStanceCrouch", R_IMAGE_TRACK_HUD);        /* 0x3044bb28 */
    cg_stanceHudShaders[2] = CG_RegisterMaterial("hudStanceProne", R_IMAGE_TRACK_HUD);         /* 0x3044bb2c */
    cg_stanceHudShaders[3] = CG_RegisterMaterial("hudStanceSprint", R_IMAGE_TRACK_HUD);        /* 0x3044bb30 */
    cg_stanceHudShaders[4] = CG_RegisterMaterial("hudStanceFlash", R_IMAGE_TRACK_HUD);         /* 0x3044bb34 */
    cg_stanceHudShaders[5] = CG_RegisterMaterial("hudFatigueStand", R_IMAGE_TRACK_HUD);        /* 0x3044bb38 */
    cg_stanceHudShaders[6] = CG_RegisterMaterial("hudFatigueCrouch", R_IMAGE_TRACK_HUD);       /* 0x3044bb3c */
    cg_stanceHudShaders[7] = CG_RegisterMaterial("hudFatigueProne", R_IMAGE_TRACK_HUD);        /* 0x3044bb40 */
    cg_stanceHudShaders[8] = CG_RegisterMaterial("hudFatigueSprint", R_IMAGE_TRACK_HUD);       /* 0x3044bb44 */

    cg_hudObjectiveReserved = 0;   /* MOV [0x3044bb48],0 — reserved slot zeroed */

    cg_objectiveShaders[0] = CG_RegisterMaterial("hudObjective", R_IMAGE_TRACK_HUD);           /* 0x3044bb4c */
    cg_objectiveShaders[1] = CG_RegisterMaterial("hudObjectiveUp", R_IMAGE_TRACK_HUD);         /* 0x3044bb50 */
    cg_objectiveShaders[2] = CG_RegisterMaterial("hudObjectiveDown", R_IMAGE_TRACK_HUD);       /* 0x3044bb54 */

    /* The compass and hit-direction materials occupy the next media slots. */
    cg_compassFriendlyShaders[0] = CG_RegisterMaterial("gfx/hud/hud@objective_friendly.tga", R_IMAGE_TRACK_HUD);
    cg_compassFriendlyShaders[1] = CG_RegisterMaterial("gfx/hud/hud@objective_friendly_chat.tga", R_IMAGE_TRACK_HUD);
    cg_compassTankShaders[0] = CG_RegisterMaterial("gfx/hud/hud@objective_tank.tga", R_IMAGE_TRACK_HUD);
    cg_compassTankShaders[1] = CG_RegisterMaterial("gfx/hud/hud@objective_jeep.dds", R_IMAGE_TRACK_HUD);
    cg_compassTankShaders[2] = CG_RegisterMaterial("gfx/hud/hud@objective_flak_88.dds", R_IMAGE_TRACK_HUD);
    cg_hitDirectionShader = (qhandle_t)CG_RegisterMaterial("hudHitDirection", R_IMAGE_TRACK_HUD);

    /* Three UI checkbox shaders via a DIRECT trap_R_RegisterShader(name, 5) — not the
     * load-screen-pumping wrapper. Each is preceded by an inlined load-screen update (the
     * levelshot draw + CG_HUNK_USED/Q_atoi("com_expectedhunkusage") progress-bar work lives
     * inside it). trap id 0x59 pushed with (5, name) at 0x3002bff6/0x3002c22c/0x3002c460. */
    CG_DrawInformation(qfalse); /* inlined load-screen updater (0x3002a530) */
    cgs_media_checkboxClear =
        coduo_int32_from_bits((uint32_t)cgame_syscall(CG_R_REGISTERSHADER, (intptr_t)"ui/assets/checkbox_clear", R_IMAGE_TRACK_HUD));
    CG_DrawInformation(qfalse); /* inlined load-screen updater (0x3002a530) */
    cgs_media_checkboxChecked =
        coduo_int32_from_bits((uint32_t)cgame_syscall(CG_R_REGISTERSHADER, (intptr_t)"ui/assets/checkbox_checked", R_IMAGE_TRACK_HUD));
    CG_DrawInformation(qfalse); /* inlined load-screen updater (0x3002a530) */
    cgs_media_checkboxFail =
        coduo_int32_from_bits((uint32_t)cgame_syscall(CG_R_REGISTERSHADER, (intptr_t)"ui/assets/checkbox_fail", R_IMAGE_TRACK_HUD));

    /* Remaining 2D media handles. */
    cgs_media_backTileShader = CG_RegisterMaterial("gfx/2d/backtile", R_IMAGE_TRACK_HUD);      /* 0x3044b6f0 */
    cgs_media_hudNoWeaponIcon = CG_RegisterMaterial("hudNoWeaponIcon", R_IMAGE_TRACK_HUD);      /* 0x3044b6f4 */
    cgs_media_flareShader = CG_RegisterMaterial("flareShader", R_IMAGE_TRACK_EFFECT);       /* 0x3044bba0, lm 4 */
    cgs_media_hudColorBar = CG_RegisterMaterial("hudColorBar", R_IMAGE_TRACK_HUD);          /* 0x3044b6c0 */
    cgs_media_hudAlliedIcon = CG_RegisterMaterial("hudAlliedIcon", R_IMAGE_TRACK_HUD);        /* 0x3044b6bc */
    cgs_media_hudAxisIcon = CG_RegisterMaterial("hudAxisIcon", R_IMAGE_TRACK_HUD);          /* 0x3044b6b8 */
    cgs_media_headiconAxisFlag = CG_RegisterMaterial("gfx/hud/headicon@axis_flag", R_IMAGE_TRACK_HUD); /* 0x3044bbac */
    cgs_media_headiconAlliesFlag = CG_RegisterMaterial("gfx/hud/headicon@allies_flag", R_IMAGE_TRACK_HUD); /* 0x3044bba8 */

    /* " - models" phase banner (built inline, printed with LOADING... prefix). */
    CG_LoadingString(" - models");

    cgs_voiceChatIcon = (uint32_t)CG_RegisterMaterial("headiconVoiceChat", R_IMAGE_TRACK_UI); /* 0x3044b6cc, lm 2 */
    cgs_talkBalloonIcon = CG_RegisterMaterial("headiconTalkBalloon", R_IMAGE_TRACK_UI); /* 0x3044b6d0, lm 2 */
    cg_railCoreShader = CG_RegisterMaterial("railCore", 1); /* 0x3044b6c8, lm 1 (EBP=1 at the call site) */

    /* Second cgame media batch (black/white and friends). */
    CG_RegisterScoreboardShaders();

    /* Zero the complete per-item and per-weapon render caches. The original
     * REP STOSD counts are 0x900 and 0x3880 dwords respectively, exactly the
     * whole PE32 arrays. sizeof preserves those extents on i386 and includes
     * naturally widened pointer members on native hosts. */
    memset(cg_items, 0, sizeof(cg_items));
    memset(cg_weaponInfos, 0, sizeof(cg_weaponInfos));

    /* " - items" phase banner, then register all item visuals. */
    CG_LoadingString(" - items");
    CG_RegisterItems();

    /* markShadow, then the water wake, are registered after the items, right
     * before the inline-models phase. 0x3002c5df pushes 0x30077f90 = "wake"
     * (a SEPARATE 5-byte string; "markShadow" is at 0x30077f98) and the store
     * at 0x3002c5fd puts that handle into 0x3044bba4 — the slot's mechanical
     * label "markShadowFadeShader" predates this proof; it actually holds the
     * wake material handle. */
    cgs_media_markShadowShader = CG_RegisterMaterial("markShadow", R_IMAGE_TRACK_EFFECT); /* 0x3044bb9c, lm 4 */
    cgs_media_wakeMarkShader = CG_RegisterMaterial("wake", R_IMAGE_TRACK_EFFECT); /* 0x3044bba4, lm 4 */

    /* " - inline models" phase banner. */
    CG_LoadingString(" - inline models");

    /* Inline "*N" brush models: for each of trap_CM_NumInlineModels() models present,
     * register "*i" and store its midpoint. */
    cg_numInlineModels = coduo_int32_from_bits((uint32_t)cgame_syscall(CG_CM_NUM_INLINE_MODELS));
    if (cg_numInlineModels > 1) {
        for (i = 1; i < cg_numInlineModels; i++) {
            char name[10];
            vec3_t mins, maxs; /* trap out-params: 0x3002c673 LEA [ESP+0x54] (arg2, mins),
                                * 0x3002c66e LEA [ESP+0x40] (arg3, maxs) */
            int32_t model;
            int axis;

            Com_sprintf(name, sizeof(name), "*%i", i); /* "*1", "*2", ... */
            model = CG_RegisterModel(name, 7);
            /* 0x3002c67b: MOV [EBP*4+0x30448de8],EAX executes BEFORE the trap
             * CALL at 0x3002c682 — the stored handle is CG_RegisterModel's
             * return; the trap's own return value is discarded. */
            cg_inlineModelHandles[i] = model;
            /* trap CG_R_MODEL_BOUNDS (0x4d): fills the two bounds vec3s read by
             * the midpoint loop below (args model, &mins, &maxs). */
            (void)cgame_syscall(CG_R_MODEL_BOUNDS, model, (intptr_t)mins, (intptr_t)maxs);
            /* Per axis the x87 computes: FLD maxs; FSUB mins; FMUL 0.5(double
             * @0x3007bd28); FADD mins; FSTP(float) — i.e. (maxs - mins) * 0.5
             * + mins = the box midpoint, with the multiply/add carried in
             * extended precision and rounded to float on store. Preserved as a
             * double-precision expression cast to float. */
            for (axis = 0; axis < 3; axis++) {
                cg_inlineModelMidpoints[i][axis] = (float)(((double)maxs[axis] - mins[axis]) * 0.5 + mins[axis]);
            }
        }
    }

    /* " - server models" phase banner, then precache the CS_MODELS config strings. Each loop
     * uses the inlined CG_ConfigString(index) (the machine code inlines its bad-index guard:
     * 0 <= index < MAX_CONFIGSTRINGS(0x800), else CG_Error("CG_ConfigString: bad index: %i");
     * the fixed CS_* base keeps the index in range, so the error path never fires here). A
     * nonempty string is registered; the parallel handle table stores the result. */
    CG_LoadingString(" - server models");
    for (i = 0; i < NUM_GAME_MODELS; i++) {
        const char *cs = CG_ConfigString(CS_MODELS + i + 1);
        if (cs[0] == '\0') {
            /* 0x3002c72f JZ 0x3002c750: the first EMPTY config string
             * terminates the whole loop (classic "if (!cs[0]) break;"),
             * jumping to the effects-loop setup. */
            break;
        }
        /* EDI begins at 0x304480e8 = &cg_gameModels[1], while ESI begins at
         * CS_MODELS + 1 (0x196); element 0 is the empty-model sentinel. */
        cg_gameModels[i + 1] = CG_RegisterModel(cs, 7);
    }

    /* Precache the CS_EFFECTS config strings into cg_effectDefs[1..79]; effect id 0 is unused
     * (the loop base pointer is &cg_effectDefs[1] at 0x304484e8). */
    for (i = 1; i <= NUM_EFFECTS; i++) {
        const char *cs = CG_ConfigString(CS_EFFECTS + i);
        if (cs[0] == '\0') {
            /* 0x3002c788 JZ 0x3002c7aa: empty string terminates the loop,
             * jumping to the shellshock-loop setup. */
            break;
        }
        cg_effectDefs[i] = (uint32_t)cgame_syscall(CG_FX_REGISTER_EFFECT, (intptr_t)cs);
    }

    /* Precache the CS_SHELLSHOCKS config strings into cg_shellShocks[0..14]. */
    for (i = 0; i < NUM_SHELLSHOCKS; i++) {
        const char *cs = CG_ConfigString(CS_SHELLSHOCKS + i + 1);
        if (cs[0] == '\0') {
            /* 0x3002c7e5 JZ 0x3002c817: empty string terminates the loop,
             * jumping to the code after it (CG_RegisterImpactEffects). */
            break;
        }
        if (CG_ShellShockLoad(cs) == 0) {
            /* 0x3002c7f8: Com_ErrorMessage(fmt, cs) directly (fmt + one arg), NOT via va. */
            Com_ErrorMessage("couldn't register shell shock '%s' -- see console\n", cs);
        }
        CG_SetShellShockParams(&cg_shellShocks[i]);
    }

    /*
     * Impact + tank/jeep tread effects. CG_RegisterImpactEffects first, then a flat
     * sequence of trap CG_FX_REGISTER_EFFECT(name) calls, each `slot = trap(...)` paired with
     * the effect NAME actually stored (store lags one trap behind the operand scheduling).
     *
     * APPARENT ORIGINAL-SOURCE DUPLICATES: the effect list registers
     * "fx/map_mp/mp_tanktread_snow.efx" twice in a row (0x3002c88a then 0x3002c89f, SAME
     * .rdata pointer 0x30077e68) and "fx/map_mp/mp_jeepwheel_snow.efx" twice in a row
     * (0x3002c90b then 0x3002c920, SAME pointer 0x30077dc4). This looks like a copy-paste
     * slip in the original tread-effect table (a "snow" entry duplicated where a distinct
     * surface was likely intended), but the machine code definitively registers the same
     * string twice, so both stores are preserved deliberately — not a reconstruction error.
     */
    CG_RegisterImpactEffects();
    cgs_media_fleshImpactEffect =
        coduo_int32_from_bits((uint32_t)cgame_syscall(CG_FX_REGISTER_EFFECT, (intptr_t)"fx/impacts/flesh_hit_noblood.efx"));
    cgs_media_vehicleTreadEffects[VEH_TREAD_EFFECT_TANK_SAND] =
        coduo_int32_from_bits((uint32_t)cgame_syscall(CG_FX_REGISTER_EFFECT, (intptr_t)"fx/map_mp/mp_tanktread_sand.efx"));
    cgs_media_vehicleTreadEffects[VEH_TREAD_EFFECT_TANK_GRASS] =
        coduo_int32_from_bits((uint32_t)cgame_syscall(CG_FX_REGISTER_EFFECT, (intptr_t)"fx/map_mp/mp_tanktread_grass.efx"));
    cgs_media_vehicleTreadEffects[VEH_TREAD_EFFECT_TANK_DIRT] =
        coduo_int32_from_bits((uint32_t)cgame_syscall(CG_FX_REGISTER_EFFECT, (intptr_t)"fx/map_mp/mp_tanktread_dirt.efx"));
    cgs_media_vehicleTreadEffects[VEH_TREAD_EFFECT_TANK_ROCK] =
        coduo_int32_from_bits((uint32_t)cgame_syscall(CG_FX_REGISTER_EFFECT, (intptr_t)"fx/map_mp/mp_tanktread_rock.efx"));
    cgs_media_vehicleTreadEffects[VEH_TREAD_EFFECT_TANK_SNOW] =
        coduo_int32_from_bits((uint32_t)cgame_syscall(CG_FX_REGISTER_EFFECT, (intptr_t)"fx/map_mp/mp_tanktread_snow.efx"));
    cgs_media_vehicleTreadEffects[VEH_TREAD_EFFECT_TANK_SNOW_ALT] =
        coduo_int32_from_bits((uint32_t)cgame_syscall(CG_FX_REGISTER_EFFECT, (intptr_t)"fx/map_mp/mp_tanktread_snow.efx"));
    cgs_media_vehicleTreadEffects[VEH_TREAD_EFFECT_JEEP_SAND] =
        coduo_int32_from_bits((uint32_t)cgame_syscall(CG_FX_REGISTER_EFFECT, (intptr_t)"fx/map_mp/mp_jeepwheel_sand.efx"));
    cgs_media_vehicleTreadEffects[VEH_TREAD_EFFECT_JEEP_GRASS] =
        coduo_int32_from_bits((uint32_t)cgame_syscall(CG_FX_REGISTER_EFFECT, (intptr_t)"fx/map_mp/mp_jeepwheel_grass.efx"));
    cgs_media_vehicleTreadEffects[VEH_TREAD_EFFECT_JEEP_DIRT] =
        coduo_int32_from_bits((uint32_t)cgame_syscall(CG_FX_REGISTER_EFFECT, (intptr_t)"fx/map_mp/mp_jeepwheel_dirt.efx"));
    cgs_media_vehicleTreadEffects[VEH_TREAD_EFFECT_JEEP_ROCK] =
        coduo_int32_from_bits((uint32_t)cgame_syscall(CG_FX_REGISTER_EFFECT, (intptr_t)"fx/map_mp/mp_jeepwheel_rock.efx"));
    cgs_media_vehicleTreadEffects[VEH_TREAD_EFFECT_JEEP_SNOW] =
        coduo_int32_from_bits((uint32_t)cgame_syscall(CG_FX_REGISTER_EFFECT, (intptr_t)"fx/map_mp/mp_jeepwheel_snow.efx"));
    cgs_media_vehicleTreadEffects[VEH_TREAD_EFFECT_JEEP_SNOW_ALT] =
        coduo_int32_from_bits((uint32_t)cgame_syscall(CG_FX_REGISTER_EFFECT, (intptr_t)"fx/map_mp/mp_jeepwheel_snow.efx"));

    /* " - game media done" phase banner (final CG_LoadingString + trap_UpdateScreen). */
    CG_LoadingString(" - game media done");

    /* --- /GS epilogue: __security_check_cookie(frame cookie [ESP+0xa0]) then RET. --- */
}
