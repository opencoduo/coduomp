// Source: uo_cgame_mp_x86.dll 0x3003ac90..0x3003b37c
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3003ac90_3003b37c.mcode
// CG_ServerCommand — dispatch one compact, single-byte reliable server command.

#include "../client_recovered.h"

#include <stdlib.h>

void CG_ServerCommand(void)
{
    char name[150];
    const char *text;
    int32_t value;

    /* 0x3003ac9d..0x3003acb8: command byte and the default-reporting path use
     * the shared argv scratch at 0x300da488, not a stack-local buffer. */
    trap_Argv(0, g_textScratchBuffer, sizeof(g_textScratchBuffer));

    /* 0x3003acb8..0x3003acd2: MOVSX of command[0]; CMP 0x76/JA sends every byte
     * above 'v' (and, sign-extended, >= 0x80) to the unknown-command default;
     * the index byte table at 0x3003b3f0 maps byte 0x00 to the silent shared
     * epilogue (0x3003b367) and every other unlisted byte to the default. */
    switch ((unsigned char)g_textScratchBuffer[0]) {
    case '\0':  /* 0x3003b367: silent return, no unknown-command print */
        return;
    case 'a':   /* 0x3003acd9 */
        CG_SelectWeaponIndex(coduo_crt_atoi(CG_Argv(1)), cg_weaponSelect_vmCvar.integer);
        return;
    case 'b':   /* 0x3003ad0c */
        CG_ParseScores();
        return;
    case 'c':   /* 0x3003ad26 */
        CG_BoldGameMessage(CG_TranslateMessage(CG_Argv(1), "announcement message"));
        return;
    case 'd':   /* 0x3003ad5a */
        CG_ConfigStringModified();
        return;
    case 'e':   /* 0x3003ad74: 'e' and 'f' share jump-table index 0x0b */
    case 'f':
        CG_GameMessage(CG_TranslateMessage(CG_Argv(1), "game message"));
        return;
    case 'g':   /* 0x3003ada8 */
        CG_PlayClientSoundAliasByName(cg_soundGameMessage);
        CG_BoldGameMessage(CG_TranslateMessage(CG_Argv(1), "bold game message"));
        return;
    case 'h':   /* 0x3003adc1 */
        if (cg_teamChatsOnly_vmCvar.integer)
            return;
        /* 0x3003ade0: the raw localize syscall — NOT CG_TranslateMessage
         * (0x3002d850): chat lines get no "[{command}]" binding-marker
         * replacement. */
        text = (const char *)(intptr_t)cgame_syscall(CG_SE_LOCALIZE_MESSAGE, (intptr_t)CG_Argv(1), (intptr_t)"chat message");
        CG_PlayClientSoundAliasByName(cgs_media_playerTalkSound);
        /* 0x3003adfa..0x3003ae11: strncpy(name, text, 0x95); name[0x95] = 0. */
        Q_strncpyz(name, text, (int32_t)sizeof(name));
        Q_StripControl0x19(name);
        CG_AddToTeamChat(name);
        Com_PrintMessage("%s\n", name);
        return;
    case 'i':   /* 0x3003ae4a */
        /* 0x3003ae5c: raw localize syscall, as in the 'h' leg. */
        text = (const char *)(intptr_t)cgame_syscall(CG_SE_LOCALIZE_MESSAGE, (intptr_t)CG_Argv(1), (intptr_t)"team chat message");
        CG_PlayClientSoundAliasByName(cgs_media_playerTalkSound);
        /* 0x3003ae76..0x3003ae8d: strncpy(name, text, 0x95); name[0x95] = 0. */
        Q_strncpyz(name, text, (int32_t)sizeof(name));
        Q_StripControl0x19(name);
        CG_AddToTeamChat(name);
        Com_PrintMessage("%s\n", name);
        return;
    case 'j':
        CG_ParseVoiceChat(0);
        return;   /* 0x3003aec6 */
    case 'k':
        CG_ParseVoiceChat(1);
        return;   /* 0x3003aee5 */
    case 'l':
        CG_ParseVoiceChat(2);
        return;   /* 0x3003af04 */
    case 'm':   /* 0x3003af23 */
        value = coduo_crt_atoi(CG_Argv(1));
        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered client-module boundary input and state before use. */
        if (value < -CG_COMPLAINT_STATUS_COUNT || value >= MAX_CLIENTS) {
            Com_Error(ERR_DROP,
                      "\x15"
                      "CG_ServerCommand: invalid complaint client "
                      "number %i",
                      value);
            return;
        }

        cg_complaintEndTime = coduo_int32_from_bits((uint32_t)cg_time + 20000u);
        cg_complaintClientNum = value;
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (cg_complaintClientNum < 0) {
            cg_complaintEndTime = coduo_int32_from_bits((uint32_t)cg_time + 10000u);
        }
        return;
    case 'n':
        CG_MapRestart(qtrue);
        return; /* 0x3003af79 */
    case 'o': { /* 0x3003af98 */
        snd_alias_t *alias = trap_Com_PickSoundAlias(CG_Argv(1), vec3_origin);
        cgame_syscall(CG_MSS_PLAY_MUSIC_ALIAS, (intptr_t)alias);
        return;
    }
    case 'p': /* 0x3003afda */
        cgame_syscall(CG_MSS_STOP_MUSIC, coduo_crt_atoi(CG_Argv(1)));
        return;
    case 'q': /* 0x3003b011 */
        value = coduo_crt_atoi(CG_Argv(2));
        trap_MSS_FadeAllSounds((float)atof(CG_Argv(1)), value);
        return;
    case 'r':
        CG_ReverbCmd();
        return; /* 0x3003b058 */
    case 's':
        CG_LocalSound();
        return; /* 0x3003b072 */
    case 't':
        CG_OpenScriptMenu();
        return; /* 0x3003b0a6 */
    case 'u':
        CG_CloseScriptMenu();
        return; /* 0x3003b0c0 */
    case 'v': /* 0x3003b0da */
        /* 0x3003b0e4..0x3003b104: strncpy(name, argv1, 0x95); name[0x95] = 0. */
        Q_strncpyz(name, CG_Argv(1), (int32_t)sizeof(name));
        trap_Cvar_Set(name, CG_Argv(2));
        return;
    case 'B':
        CG_MapRestart(qfalse);
        return; /* 0x3003b12c */
    case 'C': /* 0x3003b14b */
        trap_Cvar_VariableStringBuffer("cg_autodemo", name, 2);
        if (coduo_crt_atoi(name) != 1)
            return;
        trap_Cvar_VariableStringBuffer("name", name, 32);
        Q_StripToAlphanumeric(name);
        text = trap_DateTimeStamp();
        /* 0x3003b195..0x3003b1a8: the name pointer pushed for the (no-argument)
         * trap_DateTimeStamp stays on the stack as va's THIRD argument:
         * va("record %s-%s", stamp, name). */
        cgame_syscall(CG_EXECUTE_COMMAND, (intptr_t)va("record %s-%s", text, name));
        return;
    case 'D': /* 0x3003b1cf */
        trap_Cvar_VariableStringBuffer("cg_autodemo", name, 2);
        if (coduo_crt_atoi(name) == 1)
            cgame_syscall(CG_EXECUTE_COMMAND, (intptr_t)"stoprecord");
        return;
    case 'E': /* 0x3003b221 */
        trap_Cvar_VariableStringBuffer("cg_autoscreenshot", name, 2);
        if (coduo_crt_atoi(name) != 1)
            return;
        trap_Cvar_VariableStringBuffer("name", name, 32);
        Q_StripToAlphanumeric(name);
        text = trap_DateTimeStamp();
        /* 0x3003b26b..0x3003b27e: same stamp-then-name order as the 'C' leg. */
        cgame_syscall(CG_EXECUTE_COMMAND, (intptr_t)va("screenshotJPEG %s-%s", text, name));
        return;
    case 'F': /* 0x3003b2a5 */
        cgame_syscall(CG_FX_REWIND_TIME, coduo_crt_atoi(CG_Argv(1)));
        return;
    case 'S':
        CG_LocalSound_f();
        return; /* 0x3003b08c */
    default: /* 0x3003b2dc */
        trap_Argv(0, g_textScratchBuffer, sizeof(g_textScratchBuffer));
        Com_PrintMessage("Unknown client game command: %s\n", g_textScratchBuffer);
        value = trap_Argc();
        if (value > 1) {
            Com_PrintMessage("Arguments(%i):", value - 1);
            for (int32_t i = 1; i < value; ++i) {
                trap_Argv(i, g_textScratchBuffer, sizeof(g_textScratchBuffer));
                Com_PrintMessage(" %s", g_textScratchBuffer);
            }
            Com_PrintMessage("\n");
        }
        return;
    }
}
