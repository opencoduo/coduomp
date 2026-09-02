// Source: uo_cgame_mp_x86.dll 0x30022270..0x3002260f
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30022270_3002260f.mcode
//
// CG_Obituary — construct the target/attacker names and team colors for a kill
// event, choose the kill icon, maintain the local "fragged by" string, issue the
// local-player kill center print, and submit the obituary to cgame trap 4.
// Diagnostic and icon strings, its EV_OBITUARY dispatcher caller at 0x300234d5,
// and the complete dataflow prove the name.  The size-only Scr_Vehicle_Think
// guess is rejected.

#include "../client_recovered.h"
#include "../globals.h"

#include <stdint.h>
#include <string.h>

enum {
    CG_OBITUARY_MAX_CLIENTS = 64,
    CG_OBITUARY_WORLD_ATTACKER = 1022,
    CG_OBITUARY_MOD_FLAG = 0x80,
    /* 0x3002242c/0x300224bb copy 31 bytes and 0x30022446/0x300224d2
     * terminate at byte 31. Appending "^7" occupies bytes 31..33, proving
     * both the 32-byte name view and the 34-byte colored-name locals. */
    CG_OBITUARY_NAME_SIZE = 32,
    CG_OBITUARY_COLORED_NAME_SIZE = 34,
    CG_OBITUARY_CENTERPRINT_SUPPRESS_FLAG = 0x40000,
    CG_OBITUARY_CENTERPRINT_PRIORITY = 1,

    CG_OBIT_MOD_MELEE = 7,
    CG_OBIT_MOD_HEADSHOT = 8,
    CG_OBIT_MOD_ARTILLERY = 13,
    CG_OBIT_MOD_DROWN = 15,
    CG_OBIT_MOD_CRUSH = 16,
    CG_OBIT_MOD_CRUSH_TANK = 17,
    CG_OBIT_MOD_CRUSH_JEEP = 18,
    CG_OBIT_MOD_FALLING = 20,
    CG_OBIT_MOD_SUICIDE = 21,
    CG_OBIT_MOD_DIED = 22,
    CG_OBIT_MOD_BINOCULARS = 26
};

static const float CG_OBITUARY_ICON_SCALE = 1.4f;
static const float CG_OBITUARY_WIDE_ICON_SCALE = 2.8f;
static const float CG_OBITUARY_CENTERPRINT_Y = 390.0f; /* PUSH 0x43c30000 @0x300225a3 */
static const float CG_OBITUARY_CENTERPRINT_CHAR_WIDTH = 9.6f;

void CG_Obituary(centity_t *self)
{
    int32_t target = self->currentState.vehicleEntityNum;
    int32_t attacker = self->currentState.compassBlipIndex;
    uint32_t meansOfDeath = (uint32_t)self->currentState.eventParm;
    const char *killIcon = "killIconDied";
    float killIconScale = CG_OBITUARY_ICON_SCALE;
    vec4_t targetColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    vec4_t attackerColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    vec4_t iconColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    char targetName[CG_OBITUARY_COLORED_NAME_SIZE];
    char attackerName[CG_OBITUARY_COLORED_NAME_SIZE];
    clientInfo_t *targetInfo;
    clientInfo_t *attackerInfo = NULL;
    int32_t targetTeam;

    if ((int8_t)meansOfDeath < 0) {
        meansOfDeath &= ~((uint32_t)CG_OBITUARY_MOD_FLAG);

        /* 0x30022318 MOV EDX,EAX keys the MOD override table on the masked value here,
         * in the negative/MOD branch ONLY. The weapon-index branch zeroes the key
         * (0x30022347 XOR EDX,EDX), so LEA EAX,[EDX-7]=-7; CMP EAX,0x13; JA always hits
         * the default -- a weapon index NEVER reaches a case. A prior pass placed this
         * switch after the if/else keyed on meansOfDeath, so a positive weapon index
         * equal to a MOD case value (e.g. 7) wrongly overrode the weapon's killIcon. */
        switch (meansOfDeath) {
    case CG_OBIT_MOD_MELEE:       killIcon = "killIconMelee"; killIconScale = CG_OBITUARY_ICON_SCALE; break;
    case CG_OBIT_MOD_HEADSHOT:    killIcon = "killIconHeadShot"; killIconScale = CG_OBITUARY_ICON_SCALE; break;
    case CG_OBIT_MOD_ARTILLERY:   killIcon = "gfx/icons/hud@artillery"; killIconScale = CG_OBITUARY_ICON_SCALE; break;
    case CG_OBIT_MOD_DROWN:       killIcon = "killIconDrown"; killIconScale = CG_OBITUARY_ICON_SCALE; break;
    case CG_OBIT_MOD_CRUSH:       killIcon = "killIconCrush"; killIconScale = CG_OBITUARY_ICON_SCALE; break;
    case CG_OBIT_MOD_CRUSH_TANK:  killIcon = "killIconCrushTank"; killIconScale = CG_OBITUARY_ICON_SCALE; break;
    case CG_OBIT_MOD_CRUSH_JEEP:  killIcon = "killIconCrushJeep"; killIconScale = CG_OBITUARY_ICON_SCALE; break;
    case CG_OBIT_MOD_FALLING:     killIcon = "killIconFalling"; killIconScale = CG_OBITUARY_ICON_SCALE; break;
    case CG_OBIT_MOD_SUICIDE:     killIcon = "killIconSuicide"; killIconScale = CG_OBITUARY_ICON_SCALE; break;
    case CG_OBIT_MOD_DIED:        killIcon = "killIconDied"; killIconScale = CG_OBITUARY_ICON_SCALE; break;
    case CG_OBIT_MOD_BINOCULARS:  killIcon = "gfx/icons/hud@bino_owned"; killIconScale = CG_OBITUARY_ICON_SCALE; break;
    default: break;
        }
    } else {
        weaponInfo_t *weapon = bg_weaponInfos[meansOfDeath];
        if (weapon->killIcon[0] != '\0') {
            killIcon = weapon->killIcon;
            if (weapon->wideKillIcon != 0) {
                killIconScale = CG_OBITUARY_WIDE_ICON_SCALE;
            }
        }
    }

    if (target < 0 || target >= CG_OBITUARY_MAX_CLIENTS) {
        Com_ErrorMessage("CG_Obituary: target out of range");
    }
    uint32_t targetRowOffsetBits =
        (uint32_t)target * (uint32_t)sizeof(bgs.clientinfo[0]);
    intptr_t targetRowDisplacement =
        (intptr_t)coduo_int32_from_bits(targetRowOffsetBits);
    targetInfo = (clientInfo_t *)(
        (uintptr_t)(void *)bgs.clientinfo +
        (uintptr_t)targetRowDisplacement);
    if (targetInfo->infoValid == 0) {
        return;
    }

    Q_strncpyz(targetName, targetInfo->name,
               CG_OBITUARY_NAME_SIZE);
    targetName[CG_OBITUARY_NAME_SIZE - 1] = '\0';
    strcat(targetName, "^7");
    targetTeam = targetInfo->obituaryTeam;
    CG_DrawScoreboard_GetTeamColor(targetTeam, targetColor);

    if (bgs.clientinfo[cg_clientNum].infoValid == 0) {
        return;
    }

    if (attacker >= 0 && attacker < CG_OBITUARY_MAX_CLIENTS) {
        attackerInfo = &bgs.clientinfo[attacker];
        if (attackerInfo->infoValid == 0) {
            return;
        }
        Q_strncpyz(attackerName, attackerInfo->name,
                   CG_OBITUARY_NAME_SIZE);
        attackerName[CG_OBITUARY_NAME_SIZE - 1] = '\0';
        strcat(attackerName, "^7");
        int32_t attackerColorTeam = attackerInfo->obituaryTeam;
        CG_DrawScoreboard_GetTeamColor(attackerColorTeam, attackerColor);

        if (target == cg_snap->ps.psClientNum) {
            Q_strncpyz(cg_fraggedByName, attackerName,
                       CG_OBITUARY_NAME_SIZE);
            cg_fraggedByName[CG_OBITUARY_NAME_SIZE - 1] = '\0';
        }
    } else {
        attacker = CG_OBITUARY_WORLD_ATTACKER;
        attackerName[0] = '\0';
    }

    if (attacker == target) {
        attackerName[0] = '\0';
    } else if (attacker == cg_snap->ps.psClientNum &&
               (cg_snap->ps.playerStateFlags &
                CG_OBITUARY_CENTERPRINT_SUPPRESS_FLAG) == 0) {
        const char *message;
        /* 0x30022567 reloads the attacker team after the name/color helpers,
         * then 0x3002256e compares the target row directly rather than retaining
         * either earlier color-selection load. */
        int32_t liveAttackerTeam =
            attackerInfo != NULL ? attackerInfo->obituaryTeam : 0;
        if (liveAttackerTeam != 0 &&
            targetInfo->obituaryTeam == liveAttackerTeam) {
            message = va("CGAME_YOUKILLED\x15^1%%s^7 %s\x14%s",
                         targetName, "CGAME_TEAMMATE");
        } else {
            message = va("CGAME_YOUKILLED\x15%s", targetName);
        }
        CG_PriorityCenterPrint(message,
                               CG_OBITUARY_CENTERPRINT_Y,
                               CG_OBITUARY_CENTERPRINT_CHAR_WIDTH,
                               CG_OBITUARY_CENTERPRINT_PRIORITY);
    }

    cgame_syscall(CG_DEATH_MESSAGE,
                  (intptr_t)attackerName,
                  (intptr_t)attackerColor,
                  (intptr_t)targetName,
                  (intptr_t)targetColor,
                  (intptr_t)killIcon,
                  CG_FloatBits(killIconScale),
                  CG_FloatBits(CG_OBITUARY_ICON_SCALE),
                  (intptr_t)iconColor);
}
