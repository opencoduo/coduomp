// Source: uo_cgame_mp_x86.dll 0x3003d940..0x3003db5f
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003d940_3003db5f.mcode
//
// CG_RegisterModel(const char *name, int category) -> qhandle_t model handle.
//
// Register a render model by name and return its engine qhandle_t
// (0 => the model could not be loaded). This is a thin trap wrapper with an
// inline "pump the loading screen while precaching" body:
//
//   * If a client snapshot is already installed (cg_snap != NULL), OR a
//     synchronous redraw is already in progress (cg_updateScreenActive != 0),
//     it skips straight to the registration trap. During normal play the
//     precache work is done, so this is the fast path.
//
//   * Otherwise (loading/connecting, no snapshot, no reentry) it runs one pass
//     of the connect/loading screen — identical to the loading half of
//     CG_DrawActive (0x3001c120): clear the three cl_serverload* cvars the
//     server set, draw the current map's levelshot (or "menu/art/unknownmap")
//     full-screen via trap_R_DrawStretchPic, draw the com_expectedhunkusage
//     progress bar (fraction = trap(0x3f) / atoi(...), clamped to 1.0), and
//     force a synchronous present via trap_UpdateScreen (trap 0x19). This makes
//     the load screen animate between successive asset registrations.
//
//   * In every case it finally forwards (name, category) to the register-model
//     trap cgame_syscall(CG_R_REGISTER_MODEL /*0x2d*/, name, category) and
//     returns the full qhandle_t result.
//
// TWIN: CG_RegisterShader (0x3003db80) is byte-for-byte identical to this
// function except its terminal trap id is CG_REGISTER_MATERIAL (0x30, register
// shader/material) instead of CG_R_REGISTER_MODEL (0x2d, register model). They
// are the classic id-Tech CG_RegisterModel / CG_RegisterShader register pair,
// each carrying the same loading-screen pump so that both model and shader
// precache passes update the screen.
//
// NAME ADJUDICATION: the .mcode header's mechanical name "ScriptMover_Updatemove"
// is REJECTED — a pure size-match ("win size 0x21f, matched size 0x220") against
// a game_mp_uo SERVER script-mover routine (wrong DLL, forbidden as an identity
// signal). This function does no mover/script work: it resets client-only
// cl_serverload* cvars, draws the "levelshots/%s.tga" / "menu/art/unknownmap"
// loading art, and forwards (name, category) to the register-MODEL trap 0x2d,
// with the qhandle_t consumed as a model handle by every caller
// (CG_BuildCorpseDObjModels category 7, CG_RegisterItemVisuals categories 6/7).
// The name CG_RegisterModel is already used across the corpus (globals.h weapon
// registration comments) for 0x3003d940; this reconstruction confirms it.
//
// i386/ABI facts recorded but NOT source-level behavior:
//   * /GS stack cookie: __security_cookie (0x30081650) is snapshotted into the
//     frame on entry ([ESP+0x58]) and verified by __security_check_cookie
//     (0x30061639, ECX = saved cookie) on the single RET. Omitted from the body.
//   * cdecl, caller-cleaned (RET, not RET imm; the caller does ADD ESP,8 over the
//     two pushed args). The terminal syscall result is returned unchanged in EAX;
//     consumers that own 16-bit DObj handle fields word-store it.
//   * EBX/ESI are callee-saved and used as the loading-pump's key/scratch
//     registers (EBX = "mapname" key pointer, ESI = the levelshot shader handle);
//     they carry no cross-call state relevant to the source.

#include "../globals.h"
#include "../client_recovered.h"

#include <stddef.h>
#include <stdint.h>

/*
 * CG_RegisterModel (0x3003d940).
 *
 * `name` is the first stack argument ([ESP+0x60] after SUB ESP,0x5c) and
 * `category` the second ([ESP+0x64]); both are forwarded to the register trap in
 * that order. The intervening loading-screen pump uses neither argument.
 */
qhandle_t CG_RegisterModel(const char *name, int category)
{
    /* ---- Loading-screen pump: only while connecting (no snapshot) and not
     *      already redrawing. Otherwise jump straight to the registration. ---- */
    if (cg_snap == NULL /* 0x3003d951 */ && cg_updateScreenActive == 0 /* 0x3003d95e */) {
        char hunkUsageStr[64];               /* [ESP+0x14], CvarVariableStringBuffer dst */
        const char *serverInfo;
        const char *mapName;
        int32_t expectedHunkUsage;
        int32_t levelShotShader;             /* ESI across the pump */
        float loadFraction;

        cg_updateScreenActive = 1;           /* 0x3003d96d MOV [..cf8],1 */

        /* Clear each server-set load cvar that is currently flagged as set. */
        if (cl_serverloadmap.string[0] != 0) {  /* 0x3003d966 MOV AL,[..0030] */
            cgame_syscall(CG_CVAR_SET,
                          (intptr_t)"cl_serverloadmap",
                          (intptr_t)"");   /* 0x3003d979..0x3003d985 */
        }
        if (cl_serverloadgametype.string[0] != 0) {    /* 0x3003d98e MOV AL,[..8970] */
            cgame_syscall(CG_CVAR_SET,
                          (intptr_t)"cl_serverloadgametype",
                          (intptr_t)"");   /* 0x3003d997..0x3003d9a3 */
        }
        if (cl_serverloadwaiting.integer != 0) {     /* 0x3003d9ac MOV EAX,[..346c] */
            cgame_syscall(CG_CVAR_SET,
                          (intptr_t)"cl_serverloadwaiting",
                          (intptr_t)"0");  /* 0x3003d9b5..0x3003d9c1 */
        }

        /*
         * Resolve the map name from config string 0 (CS_SERVERINFO):
         *   info = &cg_gameState.stringData[cg_gameState.stringOffsets[0]]
         *   mapName = Info_ValueForKey(info, "mapname")   (ECX=info, EBX=key)
         * If present and non-empty, register "levelshots/<map>.tga"; otherwise
         * fall back to "menu/art/unknownmap".
         */
        serverInfo = &cg_gameState.stringData[cg_gameState.stringOffsets[0]]; /* 0x3003d9ca..0x3003d9d2 */
        mapName = Info_ValueForKey(serverInfo, "mapname");                  /* 0x3003d9dd CALL Info_ValueForKey */

        levelShotShader = 0;
        if (mapName != NULL && mapName[0] != '\0') {   /* 0x3003d9e2..0x3003d9e9 */
            const char *shaderName = va("levelshots/%s.tga", mapName); /* 0x3003d9ec..0x3003d9f1 CALL va */
            CG_DrawInformation(0);                     /* 0x3003d9fa loading-screen information */
            /* trap(0x59, name, 2) -> shader handle (CG_R_RegisterShader). */
            levelShotShader = (int32_t)cgame_syscall(CG_R_REGISTERSHADER,
                                            (intptr_t)shaderName, 2); /* 0x3003d9ff..0x3003da04 */
        }
        if (levelShotShader == 0) {                    /* 0x3003da0f TEST ESI,ESI */
            CG_DrawInformation(0);                     /* 0x3003da15 loading-screen information */
            levelShotShader = (int32_t)cgame_syscall(CG_R_REGISTERSHADER,
                                            (intptr_t)"menu/art/unknownmap", 2); /* 0x3003da1c..0x3003da23 */
        }

        /* Reset the 2D draw color to white (R_SetColor(NULL)). */
        cgame_syscall(CG_R_SETCOLOR, 0);               /* 0x3003da2e..0x3003da32 */

        /*
         * Draw the levelshot full-screen. Coordinates are virtual->real scaled:
         *   x = screenYScale * 0,   y = screenYScale * 0 (both 0),
         *   w = screenXScale * 640, h = screenYScale * 480,   s/t = (0,0)-(1,1).
         * trap_R_DrawStretchPic forwards its slots as raw 32-bit float words.
         */
        {
            float h = cgs_screenYScale * 480.0f;       /* 0x3003da38 FMUL 480.0 (0x3007c148) */
            float w = cgs_screenXScale * 640.0f;       /* 0x3003da5d FMUL 640.0 (0x3007bf34) */
            float y = cgs_screenYScale * 0.0f;         /* 0x3003da6d FMUL 0.0 (0x3007bcec) */
            float x = cgs_screenXScale * 0.0f;         /* 0x3003da7d FMUL 0.0 (0x3007bcec) */
            trap_R_DrawStretchPic(CG_FloatBits(x), CG_FloatBits(y),
                                  CG_FloatBits(w), CG_FloatBits(h),
                                  CG_FloatBits(0.0f), CG_FloatBits(0.0f),   /* s1, t1 = 0,0 */
                                  CG_FloatBits(1.0f), CG_FloatBits(1.0f),   /* s2, t2 = 1,1 */
                                  levelShotShader);    /* 0x3003da8c CALL trap_R_DrawStretchPic */
        }

        /*
         * Progress fraction. Read "com_expectedhunkusage" into hunkUsageStr and
         * atoi it; if positive, divide the current hunk usage (trap 0x3f) by it,
         * clamp to 1.0, and draw the loading bar.
         */
        cgame_syscall(CG_CVAR_VARIABLE_STRING_BUFFER,
                      (intptr_t)"com_expectedhunkusage",
                      (intptr_t)hunkUsageStr,
                      (int32_t)sizeof(hunkUsageStr));  /* 0x3003da91..0x3003da9f (0x40 = 64) */
        expectedHunkUsage = coduo_crt_atoi(hunkUsageStr);      /* 0x3003daaa CALL (via thunk 0x3005b6ce) */

        /*
         * 0x3003dab9..0x3003dad1 pre-fill four scratch slots with 0.8f
         * (0x3f4ccccd). Those slots are only re-touched inside the
         * expectedHunkUsage>0 branch, so the fill is dead on every observed path;
         * loadFraction is always assigned from the division before it is read.
         * Kept for fidelity.
         */
        loadFraction = 0.8f;

        if (expectedHunkUsage > 0) {                   /* 0x3003dab2 TEST + 0x3003dada JLE */
            int32_t currentHunkUsage = (int32_t)cgame_syscall(CG_HUNK_USED);  /* 0x3003dadc trap(0x3f) */
            /* Both integers are loaded with FILD and divided in x87 extended
             * precision.  0x3003daf3 stores the rounded drawing argument but
             * leaves the quotient live for the following comparison. */
            long double loadFractionWide =
                (long double)currentHunkUsage /
                (long double)expectedHunkUsage;
            loadFraction = (float)loadFractionWide;    /* 0x3003daf3 FST */
            if (loadFractionWide > 1.0L) {             /* 0x3003daf6 FCOMP 1.0 */
                loadFraction = 1.0f;                   /* 0x3003db03 store 0x3f800000 */
            }
            /* Draw the fixed-rect gray-on-white filled progress bar. */
            CG_DrawFilledBarStyled(200.0f, 468.0f, 240.0f, 10.0f, loadFraction); /* 0x3003db22 */
        }

        /* Force an out-of-frame present so the load screen animates. */
        cgame_syscall(CG_UPDATE_SCREEN);               /* 0x3003db2c trap(0x19) */

        cg_updateScreenActive = cg_updateScreenActive - 1; /* 0x3003db3a..0x3003db3b DEC */
    }

    /* ---- Register the model and return its full qhandle_t. ---- */
    /* 0x3003db40: the name and category are forwarded in source order. */
    return (qhandle_t)cgame_syscall(CG_R_REGISTER_MODEL,
                                    (intptr_t)name, category);
}
