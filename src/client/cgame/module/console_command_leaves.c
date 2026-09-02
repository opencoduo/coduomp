// Source: uo_cgame_mp_x86.dll console-command handlers at the RVAs shown.
// Evidence: corresponding cgame_mp/mcode/uo_cgame_mp_x86/FUN_*.mcode records.

#include <stdint.h>
#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

void CG_Tcmd_f(void) /* 0x30017210 */
{
    int32_t target = cg_crosshairEntNum;
    int32_t expiry =
        coduo_int32_from_bits((uint32_t)cg_crosshairEntTime + 1000u);
    if ((int32_t)cg_time > expiry) target = -1;
    else if (target == 0) return;

    char orderText[4];
    trap_Argv(1, orderText, sizeof(orderText));
    cgame_syscall(CG_SEND_CONSOLE_COMMAND,
        (intptr_t)va("gc %i %i", target, coduo_crt_atoi(orderText)));
}

void CG_ScoresDown_f(void) /* 0x30017330 */
{
    int32_t refreshDeadline = coduo_int32_from_bits(
        (uint32_t)cgs_scoreboardTime + 2000u);
    if (refreshDeadline < coduo_int32_from_bits(cg_time)) {
        cgs_scoreboardTime = coduo_int32_from_bits(cg_time);
        cgame_syscall(CG_SEND_CLIENT_COMMAND, (intptr_t)"score");
        if (!cg_scoreboardShowing) {
            cg_scoreboardNumClients = 0;
            cg_scoreboardScrollPos = 0;
        } else {
            /* 0x30017363 jumps directly to RET when the post-command value is
             * any nonzero dword; it does not normalize that value to qtrue. */
            return;
        }
    }
    cg_scoreboardShowing = qtrue;
}

void CG_LoadHud_f(void) /* 0x30017380 */
{
    String_Init();
    menuCount = 0;
    CG_LoadMenus(5, "ui_mp/hud.txt");
    cg_loadHudState = 0;
}

void CG_ShellShock_Save_f(void) /* 0x300175f0 */
{
    if (trap_Argc() != 2) {
        Com_PrintMessage("USAGE: cg_shellshock_save <name>\n");
        return;
    }
    char name[64];
    trap_Argv(1, name, sizeof(name));
    CG_ShellShockSave(name);
}

void CG_TellTarget_f(void) /* 0x30017660 */
{
    int32_t currentTime = coduo_int32_from_bits(cg_time);
    int32_t expiry = coduo_int32_from_bits(
        (uint32_t)cg_crosshairEntTime + 1000u);
    if (currentTime > expiry || cg_crosshairEntNum == -1) return;
    char args[128];
    char command[128];
    cgame_syscall(CG_ARGS, (intptr_t)args, sizeof(args));
    Com_sprintf(command, sizeof(command), "tell %i \"\x15%s\"", cg_crosshairEntNum, args);
    cgame_syscall(CG_SEND_CLIENT_COMMAND, (intptr_t)command);
}

void CG_TeamVoiceChat_f(void) /* 0x30017820 */
{
    if (trap_Argc() != 2) return;
    snapshot_t *snap = cg_snap;
    if (snap != NULL && snap->ps.pmType != PM_TYPE_INTERMISSION &&
        (snap->ps.playerStateFlags & PSF_ACTIVE_PLAYER) == 0) {
        Com_PrintMessage("%s\n", CG_SafeTranslateString_Internal("cgame", "CGAME_NOSPECTATORVOICECHAT"));
        return;
    }
    char token[64];
    trap_Argv(1, token, sizeof(token));
    cgame_syscall(CG_SEND_CONSOLE_COMMAND,
                  (intptr_t)va("cmd vsay_team %s\n", token));
}
