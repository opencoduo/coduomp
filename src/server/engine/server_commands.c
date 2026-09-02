#include "server_commands.h"

#include "qcommon/msg_delta.h"
#include "qcommon/net_text.h"
#include "qcommon/qcommon_runtime_types.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

enum {
    SERVER_COMMAND_RING_MASK = MAX_RELIABLE_COMMANDS - 1,
    SERVER_COMMAND_UNRELIABLE_BACKLOG_LIMIT = 32,
    SERVER_COMMAND_OVERFLOW_BACKLOG = MAX_RELIABLE_COMMANDS + 1,
    SERVER_COMMAND_FORMAT_BUFFER_SIZE = 32768,
    SERVER_EXPANDED_NEWLINES_SIZE = 1024,
    SERVER_EXPANDED_NEWLINES_STOP_INDEX = SERVER_EXPANDED_NEWLINES_SIZE - 3
};

extern serverStatic_t svs;
extern cvar_t *dedicated;
extern cvar_t *sv_maxclients;

void Com_Printf(const char *format, ...);

static char sv_expandedNewlines[SERVER_EXPANDED_NEWLINES_SIZE];

/*
 * Complete server reliable-command subsystem shared by both engines.
 * Windows CoDUOMP.exe retains the first six bodies at
 * 0x00460a60..0x00460f3d and the three snapshot-delivery bodies at
 * 0x00464580..0x00464745.  Linux coduo_lnxded retains them at
 * 0x08092980..0x0809308e and 0x08096937..0x08096b4a.  The algorithms,
 * field widths, signed sequence comparisons, constants, strings, and
 * side effects agree.  Linux calls its standalone
 * SV_SetClientDeferredDropReason at 0x0808bbff where Windows inlines the
 * same two guards and assignment; the common source keeps that body inline.
 */

char *SV_ExpandNewlines(const char *input)
{
    int32_t outputIndex = 0;

    while (*input != '\0' && outputIndex < SERVER_EXPANDED_NEWLINES_STOP_INDEX) {
        if (*input == '\n') {
            sv_expandedNewlines[outputIndex++] = '\\';
            sv_expandedNewlines[outputIndex] = 'n';
        } else if (*input == '\x14' || *input == '\x15') {
            ++input;
            continue;
        } else {
            sv_expandedNewlines[outputIndex] = *input;
        }
        ++outputIndex;
        ++input;
    }

    sv_expandedNewlines[outputIndex] = '\0';
    return sv_expandedNewlines;
}

qboolean SV_IsFirstTokenEqual(const char *left, const char *right)
{
    while (*right != '\0' && *left != '\0' && *right != ' ' && *left != ' ') {
        if (*right != *left) {
            return qfalse;
        }
        ++right;
        ++left;
    }

    if (*right != '\0' && *right != ' ') {
        return qfalse;
    }
    return (*left == '\0' || *left == ' ') ? qtrue : qfalse;
}

int32_t SV_CanReplaceServerCommand(client_t *client, const char *command)
{
    for (int32_t sequence = client->reliableSent + 1; sequence <= client->reliableSequence; ++sequence) {
        serverReliableCommand_t *const pending = &client->reliableCommands[sequence & SERVER_COMMAND_RING_MASK];
        const signed char commandType = (signed char)command[0];

        if (pending->reliable == qfalse || commandType != (signed char)pending->commandText[0] ||
            (commandType >= 'x' && commandType <= 'z')) {
            continue;
        }

        if (strcmp(command + 1, pending->commandText + 1) == 0) {
            return sequence;
        }

        switch (commandType) {
        case 'a':
        case 'b':
        case 'o':
        case 'p':
        case 'q':
        case 'r':
        case 't':
            return sequence;
        case 'd':
        case 'v':
            if (SV_IsFirstTokenEqual(pending->commandText + 2, command + 2)) {
                return sequence;
            }
            break;
        default:
            break;
        }
    }

    return -1;
}

void SV_CullIgnorableServerCommands(client_t *client)
{
    int32_t compactSequence = client->reliableSent + 1;

    for (int32_t readSequence = compactSequence; readSequence <= client->reliableSequence; ++readSequence) {
        serverReliableCommand_t *const source = &client->reliableCommands[readSequence & SERVER_COMMAND_RING_MASK];
        if (source->reliable == qfalse) {
            continue;
        }

        serverReliableCommand_t *const destination = &client->reliableCommands[compactSequence & SERVER_COMMAND_RING_MASK];
        if (destination != source) {
            *destination = *source;
        }
        ++compactSequence;
    }

    client->reliableSequence = compactSequence - 1;
}

void SV_AddServerCommand(client_t *client, qboolean reliable, const char *command)
{
    if (client->isTestClient != qfalse) {
        return;
    }

    if (client->reliableSequence - client->reliableAcknowledge >= SERVER_COMMAND_UNRELIABLE_BACKLOG_LIMIT || client->state != CS_ACTIVE) {
        SV_CullIgnorableServerCommands(client);
        if (reliable == qfalse) {
            return;
        }
    }

    int32_t sequence = SV_CanReplaceServerCommand(client, command);
    if (sequence < 0) {
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        ++client->reliableSequence;
    } else {
        for (int32_t nextSequence = sequence + 1; nextSequence <= client->reliableSequence; ++sequence, ++nextSequence) {
            client->reliableCommands[sequence & SERVER_COMMAND_RING_MASK] =
                client->reliableCommands[nextSequence & SERVER_COMMAND_RING_MASK];
        }
    }

    if (client->reliableSequence - client->reliableAcknowledge == SERVER_COMMAND_OVERFLOW_BACKLOG) {
        Com_Printf("===== pending server commands =====\n");
        for (sequence = client->reliableAcknowledge + 1; sequence <= client->reliableSequence; ++sequence) {
            const serverReliableCommand_t *const pending = &client->reliableCommands[sequence & SERVER_COMMAND_RING_MASK];
            Com_Printf("cmd %5d: %8d: %s\n", sequence, pending->enqueueTime, pending->commandText);
        }
        Com_Printf("cmd %5d: %8d: %s\n", sequence, svs.realTime, command);
        NET_OutOfBandPrint(NS_SERVER, client->netchan.remoteAddress, "disconnect");
        if (client->state != CS_ZOMBIE && client->deferredDropReason == NULL) {
            client->deferredDropReason = "EXE_SERVERCOMMANDOVERFLOW";
        }
        reliable = qtrue;
        command = "w \"EXE_SERVERCOMMANDOVERFLOW\"";
    }

    serverReliableCommand_t *const destination = &client->reliableCommands[client->reliableSequence & SERVER_COMMAND_RING_MASK];
    MSG_WriteReliableCommandToBuffer(command, destination->commandText, sizeof(destination->commandText));
    destination->enqueueTime = svs.realTime;
    destination->reliable = reliable;
}

void SV_SendServerCommand(client_t *client, qboolean reliable, const char *format, ...)
{
    char command[SERVER_COMMAND_FORMAT_BUFFER_SIZE];
    va_list arguments;

    va_start(arguments, format);
    const int32_t commandLength = vsnprintf(command, sizeof(command), format, arguments);
    va_end(arguments);

    /* NOT_FROM_ORIGINAL_SOURCE: queue only a complete formatted reliable
     * command; truncation would change its protocol meaning. */
    if (commandLength < 0 || (size_t)commandLength >= sizeof(command)) {
        Com_Printf("SV_SendServerCommand: formatted command too large\n");
        return;
    }

    if (client != NULL) {
        SV_AddServerCommand(client, reliable, command);
        return;
    }

    if (dedicated->integer != 0 && strncmp(command, "print", 5) == 0) {
        Com_Printf("broadcast: %s\n", SV_ExpandNewlines(command));
    }

    for (int32_t clientNum = 0; clientNum < sv_maxclients->integer; ++clientNum) {
        client_t *const broadcastClient = &svs.clients[clientNum];
        if (broadcastClient->state >= CS_PRIMED) {
            SV_AddServerCommand(broadcastClient, reliable, command);
        }
    }
}

void SV_UpdateServerCommandsToClient(client_t *client, msg_t *message)
{
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    for (int32_t sequence = client->reliableAcknowledge + 1; sequence <= client->reliableSequence; ++sequence) {
        MSG_WriteByte(message, SERVER_SVC_SERVER_COMMAND);
        MSG_WriteLong(message, sequence);
        MSG_WriteString(message, client->reliableCommands[sequence & SERVER_COMMAND_RING_MASK].commandText);
    }

    client->reliableSent = client->reliableSequence;
}

void SV_UpdateServerCommandsToClient_PreventOverflow(client_t *client, msg_t *message, int32_t maxBytes)
{
    int32_t sequence = client->reliableAcknowledge + 1;

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    while (sequence <= client->reliableSequence) {
        const char *const command = client->reliableCommands[sequence & SERVER_COMMAND_RING_MASK].commandText;
        const int32_t commandBytes = (int32_t)strlen(command) + 6;

        if (message->cursize + commandBytes >= maxBytes) {
            break;
        }

        MSG_WriteByte(message, SERVER_SVC_SERVER_COMMAND);
        MSG_WriteLong(message, sequence);
        MSG_WriteString(message, command);
        ++sequence;
    }

    --sequence;
    if (sequence > client->reliableSent) {
        client->reliableSent = sequence;
    }
}

void SV_PrintServerCommandsForClient(client_t *client)
{
    Com_Printf("-- Unacknowledged Server Commands for client %i:%s --\n", (int32_t)(client - svs.clients), client->name);

    for (int32_t sequence = client->reliableAcknowledge + 1; sequence <= client->reliableSequence; ++sequence) {
        const serverReliableCommand_t *const command = &client->reliableCommands[sequence & SERVER_COMMAND_RING_MASK];
        Com_Printf("cmd %5d: %8d: %s\n", sequence, command->enqueueTime, command->commandText);
    }

    Com_Printf("----------");
}
