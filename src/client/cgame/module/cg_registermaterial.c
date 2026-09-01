// Source: uo_cgame_mp_x86.dll 0x3003db80..0x3003dd9f
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003db80_3003dd9f.mcode
//
// CG_RegisterMaterial (0x3003db80) — register a render MATERIAL by name and
// return its qhandle_t, pumping the connect/loading screen once first.
//
// This is the generic material register; the earlier first-caller-derived
// CG_RegisterFlameMaterial name was rejected once the wider caller set proved it.
// Its whole payload is the final trap: cgame_syscall(CG_REGISTER_MATERIAL, name,
// param) at 0x3003dd8a, whose result (a qhandle_t) is returned. Callers observe it as
// f(name, param): CG_InitFlameChunks passes (buf, 4), CG_RegisterItemVisuals /
// CG_HeadIcon pass (icon, 5), etc. `param` is the material sort/type flag.
//
// Before that register, when NO snapshot is installed yet and no screen redraw is
// already running (cg_snap == NULL && cg_updateScreenActive == 0), it runs one full
// connect/LOADING-screen frame so the loading bar advances while assets stream in.
// That loading block is byte-identical to the loading half of CG_DrawActive
// (0x3001c120) and to its sibling register-wrappers:
//   * CG_RegisterModel (0x3003d940) — identical body, final trap 0x2d instead of 0x30.
//   * the 0x3002ba50 connect cluster — inlines the same block.
// When a snapshot IS present (cg_snap != NULL), or a redraw is already active, it
// jumps straight to the material register (0x3003dd80) and skips all screen work.
//
// Loading block (mirrors CG_DrawActive's, proven by the same cvars/strings/traps):
//   - mark cg_updateScreenActive = 1 for the duration; DEC it on the way out;
//   - clear each server-set "cl_serverload*" cvar that is currently flagged set
//     (Cvar_Set map/gametype to "", waiting to "0");
//   - resolve the map name from config-string 0 via Info_ValueForKey(info,"mapname"),
//     register its levelshot "levelshots/<map>.tga" (RegisterShader sort 2), falling
//     back to "menu/art/unknownmap" if absent/zero;
//   - R_SetColor(NULL); draw the levelshot full-screen via trap_R_DrawStretchPic
//     (virtual 640x480 scaled by cgs_screenX/YScale, texcoords 0,0..1,1);
//   - read "com_expectedhunkusage", and if > 0 divide the current hunk usage
//     (trap 0x3f) by it, clamp to 1.0, and draw the loading bar via
//     CG_DrawFilledBarStyled(200, 468, 240, 10, frac);
//   - trap_UpdateScreen (trap 0x19) to force an out-of-frame present.
//
// Name adjudication: the .mcode size-guess "Weapon_Melee" (from game_mp_uo, wrong
// DLL) is REJECTED — this fires no weapon; it registers a material. The canonical
// address decl CG_RegisterMaterial(const char *name, int param) at 0x3003db80 is
// CORRECT (the many callers CALL this exact address); its comment's vague "flame
// warm-up work guarded by an internal flag" is precisely this loading-screen pump.
// The resolved source-level symbol is CG_RegisterMaterial.
//
// One extra call vs CG_DrawActive: CG_DrawInformation(0) (0x3002a530) is invoked
// once right before EACH RegisterShader as a loading pump; its return is discarded.
//
// i386/ABI facts (not source behavior):
//   * /GS cookie: __security_cookie (0x30081650) is snapshotted into the frame and
//     verified by __security_check_cookie (0x30061639) before RET. Omitted.
//   * The two args (name, param) sit above the return address; the register-material
//     epilogue reads them straight from the incoming stack and the whole frame plus
//     the trap's 3 pushes are freed by one ADD ESP,0x68. Caller-cleaned cdecl.

#include "../globals.h"
#include "../client_recovered.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

qhandle_t CG_RegisterMaterial(const char *name, int param)
{
    /* Run the loading-screen pump only when no snapshot is installed yet and no
     * redraw is already in progress; otherwise skip straight to the register. */
    if (cg_snap == NULL && cg_updateScreenActive == 0) {   /* 0x3003db8c..0x3003dba0 */
        char hunkUsageStr[64];                             /* [ESP+0x08] CvarVariableStringBuffer dst */
        const char *mapName;
        qhandle_t levelShotShader;
        float loadFraction;

        cg_updateScreenActive = 1;                         /* 0x3003dbad MOV [..cf8],1 */

        /* Clear each server-set load cvar that is currently flagged as set. */
        if (cl_serverloadmap.string[0] != 0) {                /* 0x3003dba6 MOV AL,[..0030] */
            cgame_syscall(CG_CVAR_SET,
                          (intptr_t)"cl_serverloadmap",
                          (intptr_t)"");          /* 0x3003dbb9..0x3003dbc5 */
        }
        if (cl_serverloadgametype.string[0] != 0) {           /* 0x3003dbce MOV AL,[..8970] */
            cgame_syscall(CG_CVAR_SET,
                          (intptr_t)"cl_serverloadgametype",
                          (intptr_t)"");          /* 0x3003dbd7..0x3003dbe3 */
        }
        if (cl_serverloadwaiting.integer != 0) {            /* 0x3003dbec MOV EAX,[..346c] */
            cgame_syscall(CG_CVAR_SET,
                          (intptr_t)"cl_serverloadwaiting",
                          (intptr_t)"0");         /* 0x3003dbf5..0x3003dc01 */
        }

        /* Resolve the map name from config string 0 (CS_SERVERINFO):
         *   info = &cg_gameState.stringData[cg_gameState.stringOffsets[0]]
         *   mapName = Info_ValueForKey(info, "mapname")   (ECX=info, EBX=key). */
        {
            const char *serverInfo =
                &cg_gameState.stringData[cg_gameState.stringOffsets[0]]; /* 0x3003dc0a..0x3003dc12 */
            mapName = Info_ValueForKey(serverInfo, "mapname");         /* 0x3003dc1d CALL Info_ValueForKey */

            levelShotShader = 0;
            if (mapName != NULL && mapName[0] != '\0') {   /* 0x3003dc22..0x3003dc29 */
                const char *shaderName =
                    va("levelshots/%s.tga", mapName);      /* 0x3003dc2c..0x3003dc31 CALL va */
                CG_DrawInformation(0);                  /* 0x3003dc3a CALL (loading pump, result unused) */
                /* trap(0x59, name, 2) -> shader handle (CG_R_RegisterShader). */
                levelShotShader =
                    (qhandle_t)cgame_syscall(CG_R_REGISTERSHADER,
                                  (intptr_t)shaderName, 2);   /* 0x3003dc3f..0x3003dc44 */
            }
            if (levelShotShader == 0) {                    /* 0x3003dc4f TEST ESI,ESI / else path */
                CG_DrawInformation(0);                  /* 0x3003dc55 CALL (loading pump) */
                levelShotShader =
                    (qhandle_t)cgame_syscall(CG_R_REGISTERSHADER,
                                  (intptr_t)"menu/art/unknownmap", 2); /* 0x3003dc5c..0x3003dc63 */
            }
        }

        /* Reset the 2D draw color to white (R_SetColor(NULL)). */
        cgame_syscall(CG_R_SETCOLOR, 0);                   /* 0x3003dc70..0x3003dc72 */

        /* Draw the levelshot full-screen. Coords are virtual->real scaled:
         *   x = screenXScale * 0,   y = screenYScale * 0,
         *   w = screenXScale * 640, h = screenYScale * 480,   s/t = (0,0)-(1,1).
         * The stretch-pic args carry the float bit patterns as 32-bit words. */
        {
            float x = cgs_screenXScale * 0.0f;             /* 0x3003dcbd FMUL 0.0 (0x3007bcec) */
            float y = cgs_screenYScale * 0.0f;             /* 0x3003dcad FMUL 0.0 (0x3007bcec) */
            float w = cgs_screenXScale * 640.0f;           /* 0x3003dc9d FMUL 640.0 (0x3007bf34) */
            float h = cgs_screenYScale * 480.0f;           /* 0x3003dc78 FMUL 480.0 (0x3007c148) */
            uint32_t xb, yb, wb, hb, one, zero;
            float f1 = 1.0f, f0 = 0.0f;
            memcpy(&xb, &x, 4);
            memcpy(&yb, &y, 4);
            memcpy(&wb, &w, 4);
            memcpy(&hb, &h, 4);
            memcpy(&one, &f1, 4);                /* PUSH 0x3f800000 (1.0) x2 */
            memcpy(&zero, &f0, 4);               /* PUSH 0 (0.0) x2 */
            trap_R_DrawStretchPic((int32_t)xb, (int32_t)yb, (int32_t)wb, (int32_t)hb,
                                  (int32_t)zero, (int32_t)zero,   /* s1, t1 = 0,0 */
                                  (int32_t)one,  (int32_t)one,    /* s2, t2 = 1,1 */
                                  levelShotShader);        /* 0x3003dccc CALL trap_R_DrawStretchPic */
        }

        /* Progress fraction: read "com_expectedhunkusage" and atoi it as the total. */
        cgame_syscall(CG_CVAR_VARIABLE_STRING_BUFFER,
                      (intptr_t)"com_expectedhunkusage",
                      (intptr_t)hunkUsageStr,
                      (int32_t)sizeof(hunkUsageStr));      /* 0x3003dcd1..0x3003dcdf (0x40 = 64) */
        {
            int32_t expectedHunkUsage = coduo_crt_atoi(hunkUsageStr); /* 0x3003dcea CALL Q_atoi (thunk 0x3005b6ce) */

            /* 0x3003dcf9..0x3003dd11 pre-fill four scratch slots [ESP+0xc..0x18] with
             * 0.8f (0x3f4ccccd); those slots are only re-read inside the
             * expectedHunkUsage>0 branch (where [ESP+0xc] is overwritten by the trap
             * result), so the 0.8f fill is dead on every observed path. Kept for
             * fidelity: loadFraction is always reassigned before it is read. */
            loadFraction = 0.8f;

            if (expectedHunkUsage > 0) {                   /* 0x3003dcf2 TEST + 0x3003dd1a JLE */
                int32_t currentHunkUsage = (int32_t)cgame_syscall(CG_HUNK_USED); /* 0x3003dd1c trap(0x3f) */
                long double loadFractionWide =
                    (long double)currentHunkUsage /
                    (long double)expectedHunkUsage;
                loadFraction = (float)loadFractionWide;    /* 0x3003dd33 FST */
                if (loadFractionWide > 1.0L) {             /* 0x3003dd36 FCOMP 1.0 */
                    loadFraction = 1.0f;                   /* 0x3003dd43 store 0x3f800000 */
                }
                /* Draw the fixed-rect filled loading bar. */
                CG_DrawFilledBarStyled(200.0f, 468.0f, 240.0f, 10.0f, loadFraction); /* 0x3003dd62 */
            }
        }

        /* Force an out-of-frame present so the load screen animates. */
        cgame_syscall(CG_UPDATE_SCREEN);                   /* 0x3003dd6a trap(0x19) */

        cg_updateScreenActive = cg_updateScreenActive - 1; /* 0x3003dd72..0x3003dd7b DEC */
    }

    /* Register the material and return its handle (this is the whole payload). */
    return (qhandle_t)cgame_syscall(CG_REGISTER_MATERIAL,
                                    (intptr_t)name,
                                    (int32_t)param);       /* 0x3003dd80..0x3003dd8a trap(0x30,name,param) */
}
