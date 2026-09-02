// Source: uo_cgame_mp_x86.dll 0x3001dfb0..0x3001e37e
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001dfb0_3001e37e.mcode
//
// CG_RegisterImpactEffects — load and register every impact/explosion effect
// definition. Enumerates "fx/*.csv", parses each file into the 22 per-effect-type
// effectDef_t[24] surface tables, then registers the engine effect handles for all
// 24 surface types of all 22 effect types and reports how many surface slots were
// left without a definition.
//
// Naming: the .mcode header's "G_TryPushingEntity" is a size-match against
// game_mp.dll and is REJECTED — this function does no entity push/physics; it lists
// fx CSV files (CG_FS_GETFILELIST), formats paths with va, sorts them with qsort,
// drives the Com parse-session tokenizer, and calls CG_RegisterEffectDefSurfaces.
// The name CG_RegisterImpactEffects comes from the same-module PPC bank
// (cgame_mp.dll) and this proven behavior.
//
// ABI/compiler notes (not source-level behavior):
//  - The function has a ~0x1c4c4-byte stack frame; the MSVC `_chkstk` probe at
//    0x30060a30 (CALL with EAX=0x1c4c4) is the stack-touch prologue for the large
//    locals below (fileList[0x10000], fileNames[0x1000], defTables[22][24]).
//  - MSVC /GS: entry snapshots __security_cookie into a canary slot and the epilogue
//    passes it to __security_check_cookie (0x30061639); both are compiler-inserted
//    and omitted from the source body.
//  - AND ESP,~7 at entry is 8-byte stack alignment; not modeled.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"
#include "compat/crt/qsort_compat.h"

#include <stdlib.h>
#include <string.h> /* strlen, memset */

/* fx-directory listing limits (proven from the machine code). */
enum {
    CG_IMPACT_FX_DIR_LISTBUF = 0x10000, /* PUSH 0x10000: FS_GetFileList buffer size */
    CG_IMPACT_MAX_CSV_FILES = 0x1000   /* CMP ESI,0x1000 / JBE: file count is capped here */
};

void CG_RegisterImpactEffects(void)
{
    /* Large stack locals (frame offsets from the .mcode):
     *   fileNames  @ ESP+0xc8    : char *[0x1000]  (name pointers into fileList)
     *   defTables  @ ESP+0x40c8  : effectDef_t[22][24]  (0x8400 bytes, zeroed)
     *   fileList   @ ESP+0xc4c8  : char[0x10000]   (NUL-separated FS_GetFileList output) */
    char *fileNames[CG_IMPACT_MAX_CSV_FILES];
    effectDef_t defTables[CG_IMPACT_EFFECT_TYPES][CG_IMPACT_SURFACE_TYPES];
    char fileList[CG_IMPACT_FX_DIR_LISTBUF];

    /* The 22 (effectTypeName, handle-row) pairs, in source-declaration order. Each
     * handle row is &cg_impactEffects[row][0]; the compiler emitted these as the row
     * base addresses (0x3044c274 + row*0x60) stored at ESP+0x84..0xd8 (final-loop
     * ESP+0x70), paired with the effect-type name strings at ESP+0x2c..0x80
     * (final-loop ESP+0x18). The final registration loop walks these in order. */
    const char *const effectTypeNames[CG_IMPACT_EFFECT_TYPES] = {
        "bullet_pistol_normal",   "bullet_pistol_reflect",  "bullet_rifle_normal", "bullet_rifle_reflect",  "bullet_smg_normal",
        "bullet_smg_reflect",     "bullet_lmg_normal",      "bullet_lmg_reflect",  "bullet_hmg_normal",     "bullet_hmg_reflect",
        "bullet_umg_normal",      "bullet_umg_reflect",     "grenade_explode",     "smoke_grenade_explode", "rocket_explode",
        "molotov_explode_normal", "artillery_explode",      "mortar_explode",      "tank_explode",          "b17_explode",
        "grenade_bounce",         "molotov_explode_reflect"};
    /* The machine code constructs these row-base pointers directly in the
     * stack frame; it does not keep a static row-number lookup table. */
    qhandle_t *const effectHandleRows[CG_IMPACT_EFFECT_TYPES] = {
        cg_impactEffects[1],  cg_impactEffects[7],  cg_impactEffects[2],  cg_impactEffects[8],  cg_impactEffects[0],  cg_impactEffects[6],
        cg_impactEffects[3],  cg_impactEffects[9],  cg_impactEffects[4],  cg_impactEffects[10], cg_impactEffects[5],  cg_impactEffects[11],
        cg_impactEffects[13], cg_impactEffects[14], cg_impactEffects[15], cg_impactEffects[16], cg_impactEffects[17], cg_impactEffects[18],
        cg_impactEffects[19], cg_impactEffects[20], cg_impactEffects[12], cg_impactEffects[21]};

    int numFiles;
    int i;
    int missing;

    /* trap(0x13, "fx", "csv", fileList, 0x10000) -> file count. */
    numFiles =
        (int32_t)cgame_syscall(CG_FS_GETFILELIST, (intptr_t)"fx", (intptr_t)"csv", (intptr_t)fileList, (int32_t)CG_IMPACT_FX_DIR_LISTBUF);

    if (numFiles == 0) {
        Com_ErrorMessage("No CSV files in the fx directory to identify impact effects\n");
    } else if ((uint32_t)numFiles > (uint32_t)CG_IMPACT_MAX_CSV_FILES) {
        /* 0x3001e1af uses unsigned JBE. Thus every nonzero value whose target
         * dword is above 4096, including a negative signed result, is capped. */
        numFiles = CG_IMPACT_MAX_CSV_FILES;
    }

    /* Walk the NUL-separated list, storing a pointer to each file name. (numFiles<=0
     * skips the loop; the TEST ESI,ESI / JLE at 0x3001e1c5.) */
    if (numFiles > 0) {
        char *p = fileList;
        for (i = 0; i < numFiles; i++) {
            fileNames[i] = p;
            p += strlen(p) + 1; /* inner MOV DL,[EAX]/INC/TEST/JNZ is strlen; +1 past NUL */
        }
    }

    /* Sort the file names case-insensitively so parse order is deterministic. */
    coduo_crt_qsort(fileNames, (size_t)numFiles, sizeof(fileNames[0]), SortStringPtrsCaseInsensitive);

    /* Zero the per-effect-type surface tables (STOSD.REP, 0x2100 dwords == the whole
     * effectDef_t[22][24] == 0x8400 bytes). */
    memset(defTables, 0, sizeof(defTables));

    /* Parse each CSV file into defTables. (TEST ESI,ESI / JLE at 0x3001e20c-e220
     * guards the loop; numFiles<=0 skips it.) */
    for (i = 0; i < numFiles; i++) {
        const char *path;
        int32_t f = 0; /* FS file handle (out-param of CG_FS_FOPEN_FILE) */
        int length;
        char *text;
        char *parseError;

        /* trap(0xf, "fx/<name>", &f, FS_READ) -> length (negative on failure). */
        path = va("fx/%s", fileNames[i]);
        length = (int32_t)cgame_syscall(CG_FS_FOPEN_FILE, (intptr_t)path, (intptr_t)&f, FS_READ);
        if (length < 0) {
            continue; /* JL 0x3001e319: skip to next file */
        }

        /* Allocate length+1 bytes, read the file, close it, NUL-terminate. */
        text = (char *)(intptr_t)cgame_syscall(CG_Z_MALLOC_INTERNAL, coduo_int32_from_bits((uint32_t)length + 1u));
        cgame_syscall(CG_FS_READ, (intptr_t)text, length, (int32_t)f);
        cgame_syscall(CG_FS_FCLOSE_FILE, (int32_t)f);
        text[length] = '\0';

        /* Parse the CSV against the 22 effect-type names into defTables. */
        Com_BeginParseSession(path);
        com_parseSession->csv = 1; /* MOV [EDX+0x40c],1: enable comma-separated mode */
        parseError = CG_ParseImpactEffects(path, text, CG_IMPACT_EFFECT_TYPES, effectTypeNames, &defTables[0][0]);
        /* Inlined Com_EndParseSession (0x3001e2c4-e2f4): pop the parse session. */
        if (com_numParseSessions == 0) {
            Com_Error(ERR_FATAL, "\x15"
                                 "Com_EndParseSession: session underflow");
        }
        com_numParseSessions = coduo_int32_from_bits((uint32_t)com_numParseSessions - 1u);
        com_parseSession = &com_parseSessions[com_numParseSessions];

        cgame_syscall(CG_Z_FREE_INTERNAL, (intptr_t)text); /* free the file text */

        if (parseError != NULL) {
            /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
            Com_ErrorMessage("%s", parseError);
        }
    }

    /* Register the effect handles for every surface of every effect type, summing the
     * count of surfaces left without a definition. (Loop 0x3001e335-e357: byte cursor
     * ESI 0..0x58 step 4 == 22 iterations; the defTables pointer advances 0x600 ==
     * one effectDef_t[24] row per iteration.) */
    missing = 0;
    for (i = 0; i < CG_IMPACT_EFFECT_TYPES; i++) {
        missing += CG_RegisterEffectDefSurfaces(defTables[i], effectTypeNames[i], effectHandleRows[i]);
    }

    if (missing != 0) {
        Com_ErrorMessage("%i missing entries in effect CSV files (see console for details)", missing);
    }
}
