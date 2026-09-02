// Source: uo_cgame_mp_x86.dll 0x30038380..0x30038423
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30038380_30038423.mcode
//
// CG_ParseServerinfo — parse the serverinfo config string (config string 0) and
// cache the fields the cgame needs in its cgs serverinfo mirror.
//
// Naming: the .mcode size-guess PM_Weapon_AddFiringAimSpreadScale (broad-corpus
// name, win size 0xa3 == matched size 0xa3) is REJECTED — the body has nothing
// to do with weapon aim-spread. It reads a config string, pulls named fields out
// of it with Info_ValueForKey, copies them into the cgs mirror, forces the
// g_gametype cvar once, and builds the map path. That is CG_ParseServerinfo,
// which the same-module cgame_mp PPC bank confirms exists; named by that proven
// behavior + call graph.
//
// Machine-code shape:
//   - 0x30038382/0x30038389: ESI = &cg_gameState.stringData[
//       cg_gameState.stringOffsets[0]] — config string 0 (the serverinfo string).
//     ([0x30440a00] loads offsets[0]; LEA adds the 0x30442a00 data base.) ESI is
//     then the `info` (ECX) register-arg for every Info_ValueForKey call.
//   - 0x30038394..0x300383ab: Q_strncpyz(cgs_hostname, Info_ValueForKey(info,
//       "sv_hostname"), 0x100).  (Info_ValueForKey: info in ECX, key in EBX.)
//   - 0x300383ae..0x300383c4: Q_strncpyz(cgs_gametype, Info_ValueForKey(info,
//       "g_gametype"), 0x20).
//   - 0x300383c7..0x300383e1: run-once — if cgs_localServer == 0,
//       trap_Cvar_Set("g_gametype", cgs_gametype) via cgame_syscall id 9.
//   - 0x300383e4..0x300383fd: cgs_maxclients =
//       Q_atoi(Info_ValueForKey(info, "sv_maxclients")).
//   - 0x300383f6..0x3003841c: Com_sprintf(cgs_mapname, 0x40, "maps/mp/%s.bsp",
//       Info_ValueForKey(info, "mapname")).  (Com_sprintf: dest in EDI, size in
//       ESI, format+varargs on the caller-cleaned stack.)
//
// ABI notes (i386, not source-level): callee-saved EBX/ESI/EDI push/pop frame;
// Info_ValueForKey and Com_sprintf take register args; caller-cleaned stack
// (ADD ESP,0xc after each variadic/strncpyz call). The final ADD ESP,0xc reclaims
// one dword more than the Com_sprintf format args pushed here — a compiler stack-
// balance artifact, not an extra source argument.

#include "client/cgame/globals.h"
#include "client/cgame/client_recovered.h"

void CG_ParseServerinfo(void)
{
    /* 0x30038382/0x30038389: config string 0 = serverinfo. */
    const char *info =
        &cg_gameState.stringData[cg_gameState.stringOffsets[CS_SERVERINFO]];

    /* 0x30038394..0x300383ab */
    Q_strncpyz(cgs_hostname, Info_ValueForKey(info, sv_hostnameInfoKey),
               sizeof(cgs_hostname));

    /* 0x300383ae..0x300383c4 */
    Q_strncpyz(cgs_gametype, Info_ValueForKey(info, g_gametypeInfoKey),
               sizeof(cgs_gametype));

    /* 0x300383c7..0x300383e1: only force the cvar on first parse. */
    if (cgs_localServer == 0) {
        trap_Cvar_Set(g_gametypeInfoKey, cgs_gametype);
    }

    /* 0x300383e4..0x300383fd */
    cgs_maxclients = coduo_crt_atoi(Info_ValueForKey(info, sv_maxClientsInfoKey));

    /* 0x300383f6..0x3003841c */
    Com_sprintf(cgs_mapname, sizeof(cgs_mapname), mpMapBspPathFormat,
               Info_ValueForKey(info, mapNameInfoKey));
}
