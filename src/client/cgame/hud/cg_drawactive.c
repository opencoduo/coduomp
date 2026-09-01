// Source: uo_cgame_mp_x86.dll 0x3001c120..0x3001c48c
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001c120_3001c48c.mcode
//
// CG_DrawActive(stereoFrame_t stereoView) — the cgame per-frame top-level draw
// entry. It has two mutually exclusive halves keyed on whether a snapshot is
// installed (cg_snap):
//
//   * NO snapshot (cg_snap == NULL): the LOADING/CONNECT screen. Reentry-guarded
//     by cg_updateScreenActive, it clears the three "cl_serverload*" cvars the
//     server set, draws the current map's levelshot (or the "unknownmap" art)
//     full-screen via trap_R_DrawStretchPic, then draws a horizontal progress bar
//     whose fraction is trap(0x3f) / Q_atoi("com_expectedhunkusage") clamped to
//     1.0, and forces a synchronous present via trap_UpdateScreen (trap 0x19).
//
//   * snapshot present (cg_snap != NULL): the normal 3D view. It applies stereo
//     eye-separation to the view origin per `stereoView` (0=mono, 1=left*-0.5,
//     2=right*+0.5, else fatal "CG_DrawActive: Undefined stereoView"), sets the
//     refdef no-world bit per cg_skybox_vmCvar.integer, submits the scene with
//     trap_R_RenderScene (trap 0x45), restores the origin, advances the shell-shock
//     fade overlay, tiles the letterbox borders (CG_TileClear) and draws the 2D
//     HUD/screen (CG_Draw2D).
//
// Name adjudication: the .mcode header's mechanical name "G_GetNonPVSFriendlyInfo"
// is REJECTED. That is a pure size-guess ("win size 0x36c, matched size 0x368")
// against a game_mp_uo SERVER PVS/entity routine, which the recovery contract
// forbids as an identity signal. This function does no PVS or entity-visibility
// work: it is a cgame CLIENT draw function, proven by the client-only cvars it
// resets ("cl_serverloadmap", "cl_serverloadgametype", "cl_serverloadwaiting"),
// the "levelshots/%s.tga" / "menu/art/unknownmap" shaders it registers, the
// cg_snap gate, the cg.refdef view-origin/stereo math, and above all the
// diagnostic string it emits on a bad stereo argument: "CG_DrawActive: Undefined
// stereoView". The role name CG_DrawActive is the id-Tech/CoD symbol that owns
// that exact error string. Its sole caller (0x30042782) is a per-frame draw
// dispatcher that passes a value it earlier compared against 2 — the stereoView.
//
// i386/ABI facts recorded but NOT source-level behavior:
//   * /GS stack cookie: __security_cookie (0x30081650) is snapshotted into the
//     frame on entry ([ESP+0x5c]) and verified by __security_check_cookie
//     (0x30061639, ECX = saved cookie) on each return. Omitted from the body.
//   * Both RET paths clean nothing (RET, not RET imm): the single stereoView
//     argument sits above the return address and is caller-cleaned (the caller
//     does ADD ESP,0x14 covering this push plus others).

#include "../globals.h"
#include "../client_recovered.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* stereoFrame_t: which eye this frame renders (id-Tech stereoFrame_t). The switch
 * at 0x3001c339 dispatches 0/1/2 and treats anything else as an error. */
enum {
    STEREO_CENTER = 0,  /* mono: no eye separation                                */
    STEREO_LEFT   = 1,  /* left eye:  origin += (cg_stereoSeparation_vmCvar.value * -0.5) * right */
    STEREO_RIGHT  = 2   /* right eye: origin += (cg_stereoSeparation_vmCvar.value *  0.5) * right */
};

/* The fixed loading progress-bar rectangle (virtual 640x480 UI coords), pushed as
 * float bit patterns to CG_DrawFilledBarStyled at 0x3001c2f8..0x3001c30c. */
enum {
    /* kept as named float constants below; enum only groups the concept */
    LOADBAR_UNUSED = 0
};

/*
 * CG_DrawActive (0x3001c120).
 *
 * `stereoView` is read from the first stack argument ([ESP+0x6c] after the frame
 * is set up); modeled as an ordinary int parameter.
 */
void CG_DrawActive(int32_t stereoView)
{
    /* ---- LOADING/CONNECT screen: no snapshot installed yet ---- */
    if (cg_snap == NULL) {                                /* 0x3001c12c TEST/JNZ */
        char hunkUsageStr[64];                            /* [ESP+0x30], CvarVariableStringBuffer dst */
        int32_t expectedHunkUsage;
        const char *mapName;
        int32_t levelShotShader;
        float loadFraction;
        unsigned char serverLoadMapSet;

        /* Reentry guard: bail if a redraw is already in progress; otherwise mark
         * one active for the duration and DEC it on the way out (0x3001c31c). */
        if (cg_updateScreenActive != 0) {                /* 0x3001c13b..0x3001c142 */
            /* nothing to draw; fall through to the shared cookie-checked epilogue */
            return;                                       /* 0x3001c47d..0x3001c48b */
        }
        /* The map cvar's first byte is captured before the guard store; the
         * branch at 0x3001c159 consumes that captured byte. */
        serverLoadMapSet = (unsigned char)cl_serverloadmap.string[0];
        cg_updateScreenActive = 1;                        /* 0x3001c14f MOV [..cf8],1 */

        /* Clear each server-set load cvar that is currently flagged as set. The
         * value string is the empty literal ""; the key is the cvar name. */
        if (serverLoadMapSet != 0) {                         /* 0x3001c148 MOV AL,[..0030] */
            cgame_syscall(CG_CVAR_SET,
                          (intptr_t)"cl_serverloadmap",
                          (intptr_t)"");         /* 0x3001c15b..0x3001c167 */
        }
        if (cl_serverloadgametype.string[0] != 0) {          /* 0x3001c170 MOV AL,[..8970] */
            cgame_syscall(CG_CVAR_SET,
                          (intptr_t)"cl_serverloadgametype",
                          (intptr_t)"");         /* 0x3001c179..0x3001c185 */
        }
        if (cl_serverloadwaiting.integer != 0) {           /* 0x3001c18e MOV EAX,[..346c] */
            cgame_syscall(CG_CVAR_SET,
                          (intptr_t)"cl_serverloadwaiting",
                          (intptr_t)"0");        /* 0x3001c197..0x3001c1a3 */
        }

        /*
         * Resolve the map name from config string 0 (CS_SERVERINFO):
         *   info = &cg_gameState.stringData[cg_gameState.stringOffsets[0]]
         *   mapName = Info_ValueForKey(info, "mapname")   (ECX=info, EBX=key)
         * If the map name is present and non-empty, register its levelshot shader
         * "levelshots/<map>.tga"; otherwise fall back to "menu/art/unknownmap".
         */
        {
            const char *serverInfo =
                &cg_gameState.stringData[cg_gameState.stringOffsets[0]]; /* 0x3001c1ac..0x3001c1b8 */
            mapName = Info_ValueForKey(serverInfo, "mapname");         /* 0x3001c1bd CALL Info_ValueForKey */

            levelShotShader = 0;
            if (mapName != NULL && mapName[0] != '\0') {  /* 0x3001c1c2..0x3001c1c9 */
                const char *shaderName =
                    va("levelshots/%s.tga", mapName);     /* 0x3001c1cc..0x3001c1da CALL va */
                CG_DrawInformation(0);                    /* 0x3001c1da CALL 0x3002a530 */
                /* trap(0x59, name, 2) -> shader handle (CG_R_RegisterShader). */
                levelShotShader = coduo_int32_from_bits((uint32_t)cgame_syscall(
                    CG_R_REGISTERSHADER, (intptr_t)shaderName, 2)); /* 0x3001c1e2..0x3001c1e7 */
            }
            if (levelShotShader == 0) {                   /* 0x3001c1f2 TEST ESI,ESI / else path */
                CG_DrawInformation(0);                    /* 0x3001c1f6 CALL 0x3002a530 */
                levelShotShader = coduo_int32_from_bits((uint32_t)cgame_syscall(
                    CG_R_REGISTERSHADER,
                    (intptr_t)"menu/art/unknownmap", 2)); /* 0x3001c200..0x3001c209 */
            }
        }

        /* Reset the 2D draw color to white (R_SetColor(NULL)). */
        cgame_syscall(CG_R_SETCOLOR, 0);                  /* 0x3001c214..0x3001c218 */

        /*
         * Draw the levelshot full-screen. Coordinates are virtual->real scaled:
         *   x = screenXScale * 0,   y = screenYScale * 0,
         *   w = screenXScale * 640, h = screenYScale * 480,   s/t = (0,0)-(1,1).
         * trap_R_DrawStretchPic takes its args as 32-bit words carrying the float
         * bit patterns; forward them bit-exact.
         */
        {
            /* 0x3001c21e..0x3001c26f evaluates and stores h, w, y, then x,
             * reloading the scale global for each product. */
            float h = (float)((long double)cgs_screenYScale * 480.0L);
            float w = (float)((long double)cgs_screenXScale * 640.0L);
            float y = (float)((long double)cgs_screenYScale * 0.0L);
            float x = (float)((long double)cgs_screenXScale * 0.0L);
            uint32_t xb, yb, wb, hb, one, zero;
            /* reinterpret the floats as their 32-bit words (the syscall slots) */
            memcpy(&xb, &x, 4);
            memcpy(&yb, &y, 4);
            memcpy(&wb, &w, 4);
            memcpy(&hb, &h, 4);
            { float f1 = 1.0f, f0 = 0.0f;
              memcpy(&one, &f1, 4);
              memcpy(&zero, &f0, 4); }          /* PUSH 0x3f800000 (1.0), PUSH 0 (0.0) */
            trap_R_DrawStretchPic((int32_t)xb, (int32_t)yb, (int32_t)wb, (int32_t)hb,
                                  (int32_t)zero, (int32_t)zero,           /* s1, t1 = 0,0 */
                                  (int32_t)one,  (int32_t)one,            /* s2, t2 = 1,1 */
                                  levelShotShader);        /* 0x3001c272 CALL trap_R_DrawStretchPic */
        }

        /*
         * Progress fraction. Read "com_expectedhunkusage" into hunkUsageStr and
         * atoi it as the total; if positive, divide the current hunk usage
         * (trap 0x3f) by it, clamp to 1.0, and draw the loading bar.
         */
        cgame_syscall(CG_CVAR_VARIABLE_STRING_BUFFER,
                      (intptr_t)"com_expectedhunkusage",
                      (intptr_t)hunkUsageStr,
                      (int32_t)sizeof(hunkUsageStr));      /* 0x3001c27a..0x3001c288 (0x40 = 64) */
        expectedHunkUsage = coduo_crt_atoi(hunkUsageStr);          /* 0x3001c293 CALL (via thunk 0x3005b6ce) */

        /*
         * 0x3001c2a1..0x3001c2b9 pre-fill four scratch slots [ESP+0xc..0x18] with
         * 0.8f (0x3f4ccccd). Those slots are only touched again inside the
         * expectedHunkUsage>0 branch ([ESP+0xc] is overwritten by the trap result),
         * so the 0.8f fill is dead on every observed path; loadFraction is always
         * assigned from the division before it is read. Kept the write for fidelity.
         */
        loadFraction = 0.8f;

        if (expectedHunkUsage > 0) {                       /* 0x3001c29b TEST + 0x3001c2c1 JLE */
            int32_t currentHunkUsage = coduo_int32_from_bits(
                (uint32_t)cgame_syscall(CG_HUNK_USED));    /* 0x3001c2c3 trap(0x3f) */
            /* frac = currentHunkUsage / expectedHunkUsage, computed entirely in
             * integer-sourced x87: FILD [ESP+0xc] (0x3001c2cf) then FIDIV
             * [ESP+0x1c] (0x3001c2d6) -- NEITHER operand is rounded to float
             * first; the machine keeps both integers EXACT in the 80-bit divide.
             * (float) casts here would round each operand before the divide,
             * which diverges once a hunk byte count exceeds 2^24 (~16 MB, the
             * common case). Cast the dividend to long double (a no-op at
             * FLT_EVAL_METHOD=2) and leave the divisor an int so it FIDIVs.
             * The quotient is then kept UNROUNDED in st0 across the clamp
             * compare: FST [ESP+0x8] (0x3001c2da) stores the float copy WITHOUT
             * popping, and FCOMP 1.0 (0x3001c2de) tests the 80-bit value. */
            long double loadQuotient =
                (long double)currentHunkUsage / expectedHunkUsage; /* 0x3001c2cf FILD; 0x3001c2d6 FIDIV */
            loadFraction = (float)loadQuotient;            /* 0x3001c2da FST (no pop) */
            if (loadQuotient > 1.0f) {                     /* 0x3001c2de FCOMP 1.0; test 0x41 / JNE */
                loadFraction = 1.0f;                       /* 0x3001c2eb store 0x3f800000 */
            }
            /* Draw the fixed-rect gray-on-white filled progress bar. */
            CG_DrawFilledBarStyled(200.0f, 468.0f, 240.0f, 10.0f, loadFraction); /* 0x3001c30c */
        }

        /* Force an out-of-frame present so the load screen animates. */
        cgame_syscall(CG_UPDATE_SCREEN);                   /* 0x3001c314 trap(0x19) */

        cg_updateScreenActive = coduo_int32_from_bits(
            (uint32_t)cg_updateScreenActive - 1u);         /* 0x3001c31c..0x3001c325 DEC */
        /* fall through to the shared cookie-checked epilogue (RET) */
        return;
    }

    /* ---- Normal 3D view: a snapshot is installed ---- */
    {
        float stereoSep;
        vec3_t savedOrigin;   /* [ESP+0xc..0x14]: saved cg_refdef.vieworg before the eye shift */
        uint32_t refFlags;

        /* Eye separation from the stereo frame selector. */
        switch (stereoView) {                              /* 0x3001c339..0x3001c346 */
        case STEREO_CENTER:                                /* 0x3001c383 */
            stereoSep = 0.0f;
            break;
        case STEREO_LEFT:                                  /* 0x3001c371 */
            stereoSep = cg_stereoSeparation_vmCvar.value * -0.5f;       /* FMUL -0.5 (0x3007bf50) */
            break;
        case STEREO_RIGHT:                                 /* 0x3001c35f */
            stereoSep = cg_stereoSeparation_vmCvar.value * 0.5f;        /* FMUL 0.5 (0x3007bce8) */
            break;
        default:                                           /* 0x3001c348 */
            /* PUSHing the diagnostic moves the local from pre-push [ESP+8]
             * to [ESP+0xc]; the store at 0x3001c34d therefore clears the
             * separation before the call, even if the fatal routine returns. */
            stereoSep = 0.0f;
            Com_ErrorMessage("CG_DrawActive: Undefined stereoView"); /* 0x3001c355 CALL Com_ErrorMessage */
            break;
        }

        /* Snapshot the current view origin with the three raw dword moves at
         * 0x3001c391/39a/3a0; this also preserves NaN payload bits exactly. */
        memcpy(&savedOrigin[0], &cg_refdef.vieworg[0], 4);
        memcpy(&savedOrigin[1], &cg_refdef.vieworg[1], 4);
        memcpy(&savedOrigin[2], &cg_refdef.vieworg[2], 4);

        /* If there is a separation, shift the origin along viewaxis[1] by -sep. */
        if (stereoSep != 0.0f) {                           /* 0x3001c38b..0x3001c3b9 FUCOMPP 0.0 / JNP */
            long double negSep = -(long double)stereoSep;  /* 0x3001c3bb FLD [ESP+8]; FCHS */
            cg_refdef.vieworg[0] = (float)(
                (long double)cg_refdef.viewaxis[1][0] *
                    negSep +
                (long double)cg_refdef.vieworg[0]); /* 0x3001c3c1..0x3001c3cf */
            cg_refdef.vieworg[1] = (float)(
                (long double)cg_refdef.viewaxis[1][1] *
                    negSep +
                (long double)cg_refdef.vieworg[1]); /* 0x3001c3d5..0x3001c3e3 */
            cg_refdef.vieworg[2] = (float)(
                (long double)cg_refdef.viewaxis[1][2] *
                    negSep +
                (long double)cg_refdef.vieworg[2]); /* 0x3001c3e9..0x3001c3f5 */
        }

        /* Set the refdef skybox-portal-active bit (0x10), then clear it again only when
         * cg_skybox_vmCvar.integer == 0. 0x3001c40e JNE 0x3001c41d SKIPS the AND
         * when integer != 0, so the AND ~0x10 runs on the fall-through (integer==0)
         * path: the bit is KEPT when the cvar is nonzero, CLEARED when zero. (A
         * prior pass inverted this to `!= 0`, contradicting globals.h:5404.) */
        refFlags = cg_refdef.rdflags;                           /* 0x3001c3fb MOV ECX */
        {
            int32_t skyboxEnabled = cg_skybox_vmCvar.integer;   /* 0x3001c401 MOV EAX */
            refFlags |= RDF_SKYBOX_PORTAL_ACTIVE;               /* 0x3001c406 OR 0x10 */
            cg_refdef.rdflags = refFlags;                       /* 0x3001c40b store */
            if (skyboxEnabled == 0) {                           /* 0x3001c409 TEST/JNZ-skip */
                cg_refdef.rdflags =
                    refFlags & ~(uint32_t)RDF_SKYBOX_PORTAL_ACTIVE; /* 0x3001c413..0x3001c418 */
            }
        }

        /* Submit the scene to the engine renderer. */
        cgame_syscall(CG_R_RENDER_SCENE,
                      (intptr_t)&cg_refdef.x);  /* 0x3001c41d..0x3001c424 trap(0x45,&refdef) */

        /* Restore the pre-shift origin when a separation was applied. */
        if (stereoSep != 0.0f) {                            /* 0x3001c42a..0x3001c43e FUCOMPP 0.0 / JNP */
            memcpy(&cg_refdef.vieworg[0], &savedOrigin[0], 4); /* 0x3001c440..0x3001c44c */
            memcpy(&cg_refdef.vieworg[1], &savedOrigin[1], 4); /* 0x3001c451 */
            memcpy(&cg_refdef.vieworg[2], &savedOrigin[2], 4); /* 0x3001c457 */
        }

        /*
         * Advance the shell-shock fade overlay for this frame (register-arg ABI:
         * ESI=overlay/params, EAX=startTime, ECX=duration/endTime). The machine
         * loads ESI=[cg_shellShockSwayParams], EAX=[cg_shellShockSwayStartTime],
         * ECX=[cg_shellShockSwayDuration]; forward them positionally.
         */
        {
            int32_t swayDuration = cg_shellShockSwayDuration; /* 0x3001c45d ECX */
            int32_t swayStart = cg_shellShockSwayStartTime;   /* 0x3001c463 EAX */
            shellshock_t *swayParams = cg_shellShockSwayParams; /* 0x3001c468 ESI */
            CG_UpdateFadeOverlay(swayParams, swayStart, swayDuration);
        }

        /* COMPATIBILITY_PATCH (NOT_FROM_ORIGINAL_SOURCE): one isolated
         * presentation interface preserves this recovered call in native mode
         * and leaves the engine-cleared black bars intact in classic mode. */
        cgame_compat_tile_clear();
        CG_Draw2D();                               /* 0x3001c478 CALL CG_Draw2D */
        /* fall through to the shared cookie-checked epilogue (RET) */
    }
}
