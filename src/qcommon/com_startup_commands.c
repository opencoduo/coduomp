#include "com_startup_commands.h"

#include "filesystem/filesystem.h"
#include "q_command.h"
#include "q_cvar.h"
#include "q_string.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum {
    COM_MAX_CONSOLE_LINES = 32,
    COM_JOURNAL_RECORD_MODE = 1,
    COM_JOURNAL_REPLAY_MODE = 2
};

static char *com_consoleLines[COM_MAX_CONSOLE_LINES];
static int32_t com_numConsoleLines;
static qboolean com_safemode;

cvar_t *com_journal;
int32_t com_journalFile;
int32_t com_journalDataFile;

void Com_Printf(const char *format, ...);

/*
 * The Windows client and Linux dedicated command-line splitters retain the
 * same 32 pointers and stop at the same '+' and newline delimiters:
 * CoDUOMP.exe 0x0043a370 and coduo_lnxded 0x080706b8.  The supporting Mac
 * client exports the canonical Com_ParseCommandLine name.
 */
void Com_ParseCommandLine(char *commandLine)
{
    com_consoleLines[0] = commandLine;
    com_numConsoleLines = 1;

    for (char *cursor = commandLine; *cursor != '\0'; ++cursor) {
        if (*cursor != '+' && *cursor != '\n') {
            continue;
        }
        if (com_numConsoleLines == COM_MAX_CONSOLE_LINES) {
            break;
        }

        com_consoleLines[com_numConsoleLines++] = cursor + 1;
        *cursor = '\0';
    }
}

/* CoDUOMP.exe 0x0043a3c0; coduo_lnxded 0x08070717. */
qboolean Com_SafeMode(void)
{
    for (int32_t lineIndex = 0;
         lineIndex < com_numConsoleLines;
         ++lineIndex) {
        Cmd_TokenizeString(com_consoleLines[lineIndex]);
        const char *const command = Cmd_Argv(0);
        if (Q_stricmp(command, "safe") == 0 ||
            Q_stricmp(command, "cvar_restart") == 0) {
            com_consoleLines[lineIndex][0] = '\0';
            return qtrue;
        }
    }

    return com_safemode;
}

/* CoDUOMP.exe 0x0043a460; coduo_lnxded 0x080707ad. */
void Com_SetSafeMode(void)
{
    com_safemode = qtrue;
}

/* CoDUOMP.exe 0x0043a470; coduo_lnxded 0x080707bc. */
void Com_StartupVariable(const char *name)
{
    for (int32_t lineIndex = 0;
         lineIndex < com_numConsoleLines;
         ++lineIndex) {
        Cmd_TokenizeString(com_consoleLines[lineIndex]);
        if (strcmp(Cmd_Argv(0), "set") != 0) {
            continue;
        }

        const char *const cvarName = Cmd_Argv(1);
        if (name != NULL && strcmp(cvarName, name) != 0) {
            continue;
        }

        Cvar_Set(cvarName, Cmd_Argv(2));
        Cvar_Get(cvarName, "", 0)->flags |= CVAR_USER_CREATED;
    }
}

/* CoDUOMP.exe 0x0043a560; coduo_lnxded 0x0807088e. */
qboolean Com_AddStartupCommands(void)
{
    qboolean addedCommand = qfalse;

    for (int32_t lineIndex = 0;
         lineIndex < com_numConsoleLines;
         ++lineIndex) {
        const char *const line = com_consoleLines[lineIndex];
        if (line == NULL || line[0] == '\0') {
            continue;
        }

        if (Q_stricmpn(line, "set", 3) != 0) {
            addedCommand = qtrue;
        }
        Cbuf_AddText(line);
        Cbuf_AddText("\n");
    }

    return addedCommand;
}

/*
 * The journal open and failure paths agree at CoDUOMP.exe
 * 0x0043a6d0..0x0043a7c2 and coduo_lnxded 0x08070a72..0x08070b86.
 * The Mac client exports the canonical Com_InitJournaling name.
 */
void Com_InitJournaling(void)
{
    Com_StartupVariable("journal");
    com_journal = Cvar_Get("journal", "0", CVAR_INIT);
    if (com_journal->integer == 0) {
        return;
    }

    if (com_journal->integer == COM_JOURNAL_RECORD_MODE) {
        Com_Printf("Journaling events\n");
        com_journalFile = FS_FOpenFileWrite("journal.dat");
        com_journalDataFile = FS_FOpenFileWrite("journaldata.dat");
    } else if (com_journal->integer == COM_JOURNAL_REPLAY_MODE) {
        Com_Printf("Replaying journaled events\n");
        (void)FS_FOpenFileRead("journal.dat", &com_journalFile, qtrue);
        (void)FS_FOpenFileRead(
            "journaldata.dat", &com_journalDataFile, qtrue);
    }

    if (com_journalFile == 0 || com_journalDataFile == 0) {
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        Cvar_Set("com_journal", "0");
        com_journalFile = 0;
        com_journalDataFile = 0;
        Com_Printf("Couldn't open journal files\n");
    }
}
