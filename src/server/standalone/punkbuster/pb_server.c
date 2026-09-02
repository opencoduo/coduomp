#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <unistd.h>

#include "qcommon/q_command.h"
#include "../core_cvar/cvar_private.h"
#include "../core_runtime/core_runtime_private.h"
#include "../server/sv_punkbuster_bridge_private.h"
#include "server/standalone/bindings/coduo_engine_structs.h"

enum {
    PB_SERVER_START_CLIENT_NUM = -1,
    PB_SERVER_COMMAND_BUFFER_SIZE = 2048,
    PB_SERVER_STRING_SEPARATOR_SIZE = 1,
    PB_SERVER_COMMAND_NUL_BYTE = 1,
    PB_COMMAND_PREFIX_LENGTH = 3,
    PB_SERVER_SB_COMMAND_EXEC = 15,
    PB_SERVER_SB_FORCE_PROCESS = 51,
    PB_SERVER_SB_MODULE_IDLE = 113,
    PB_SERVER_SB_MODULE_SHUTDOWN = 114,
    PB_INTEGER_DECIMAL_BASE = 10,
    PB_INTEGER_CONVERSION_MAX_BASE = 36,
    PB_INTEGER_CONVERSION_DIGIT_BUFFER_SIZE = 35,
    PB_LIFECYCLE_SENTINEL = 65535,
    PB_LIFECYCLE_START = 1,
    PB_LIFECYCLE_SHUTDOWN = 0,
    PB_MODULE_FILE_MODE = 0777,
    PB_HOME_PATH_GETCWD_LIMIT = 251,
    PB_COPY_MODULE_NO_SIZE_LIMIT = 0,
    PB_DLOPEN_LAZY = 1,
    PB_COPY_FILE_SEEK_SET = 0,
    PB_COPY_FILE_SEEK_END = 2
};

/*
 * The stock i386 engine imports `_Znaj@GLIBCPP_3.2` and `_ZdlPv@GLIBCPP_3.2`
 * for this PB module-copy buffer path. Under the old 32-bit GNU C++ ABI,
 * `_Znaj` is operator new[](unsigned int) and `_ZdlPv` is operator delete for
 * a void pointer. These direct declarations keep the original external symbol
 * identities; the symbols are meaningful only for compatible PB-enabled
 * builds.
 */
void *_Znaj(uint32_t size);
void _ZdlPv(void *ptr);

static serverPbState_t pb_serverObject;
static char *pb_consoleCaptureBuffer;
static int32_t pb_consoleCaptureBufferSize;

/* NOT_FROM_ORIGINAL_SOURCE: typed dlsym bridge for ISO C function pointers. */
static serverPbModuleSaCallback_t coduomp_pb_load_server_sa_symbol(void *handle, const char *symbolName)
{
    serverPbModuleSaCallback_t function;
    void *symbol = dlsym(handle, symbolName);

    _Static_assert(sizeof(function) == sizeof(symbol), "PB server SA symbol pointer size mismatch");
    memcpy(&function, &symbol, sizeof(function));
    return function;
}

/* NOT_FROM_ORIGINAL_SOURCE: typed dlsym bridge for ISO C function pointers. */
static serverPbModuleSbCallback_t coduomp_pb_load_server_sb_symbol(void *handle, const char *symbolName)
{
    serverPbModuleSbCallback_t function;
    void *symbol = dlsym(handle, symbolName);

    _Static_assert(sizeof(function) == sizeof(symbol), "PB server SB symbol pointer size mismatch");
    memcpy(&function, &symbol, sizeof(function));
    return function;
}

static const char pbCommandSetSvPunkbuster[] = "set_sv_punkbuster";
static const char pbCommandConsoleCaptureBufferLength[] = "ConCapBufLen";
static const char pbCommandConsoleCaptureBuffer[] = "ConCapBuf";
static const char pbCommandExecute[] = "Cmd_Exec";
static const char pbCommandPrefix[] = "pb_";
static const char pbCommandDropClient[] = "DropClient";
static const char pbCommandCvarSet[] = "Cvar_Set";
static const char pbQueryFailed[] = "PB Error: Query Failed";
static const char pbSkipNotifyPrefix[] = "[skipnotify]";
static const char pbOutputFormat[] = "%s: %s\n";
static const char pbTitle[] = "PunkBuster Server";
static const char pbModuleServerNew[] = "pbsvnew.so";
static const char pbModuleServerOld[] = "pbsvold.so";
static const char pbModuleServer[] = "pbsv.so";
static const char pbModuleClient[] = "pbcl.so";
static const char pbModuleAgent[] = "pbag.so";
static const char pbFileReadMode[] = "rb";
static const char pbFileWriteMode[] = "wb";
static const char pbLoadFailure[] = "PB Error: Server DLL Load Failure";
static const char pbProcedureFailure[] = "PB Error: Server DLL Get Procedure "
                                         "Failure";
static const char pbExportServerSa[] = "sa";
static const char pbExportServerSb[] = "sb";
static const char pbPathSeparator[] = "/";
static const char pbDirectoryName[] = "pb/";
static const char pbCvarBasepath[] = "fs_basepath";
static const char pbCvarHomepath[] = "fs_homepath";

int32_t PB_ServerCommand(const char *command, intptr_t value);
const char *PB_ServerQuery(serverPbQuery_t query, char *buffer);
int32_t PB_ServerOutput(const char *text);
int32_t PB_ServerChecksum(const char *data, int32_t length, int32_t clientNum);
int32_t PB_ServerPacket(const char *address, uint16_t port, const void *data, int32_t length);
void PB_SetServerCallbacks(serverPbState_t *server);
void PB_CallServerSaCommandDrain(void);
intptr_t PB_CallServerSb(serverPbState_t *server, int32_t opcode, intptr_t arg0, intptr_t arg1, intptr_t arg2, intptr_t arg3);
int32_t PB_CallServerSa(serverPbState_t *server, serverPbSaValue_t value);
char *itoa(int32_t value, char *buffer, int32_t base);
void PB_InitServerObject(serverPbState_t *server);
void PB_ShutdownServerObject(serverPbState_t *server);
/*
 * The binary only proves auxiliary module handle slots +0x0c and +0x10 here;
 * it does not prove client/agent roles for those handles.
 */
void PB_CloseAuxiliaryModuleHandle0c(serverPbState_t *server);
void PB_CloseAuxiliaryModuleHandle10(serverPbState_t *server);
void PB_ClearServerCallbacks(serverPbState_t *server);
void PB_UnloadServerDll(serverPbState_t *server);
const char *PB_LoadServerModule(serverPbState_t *server);
void PB_ChmodModulePath(const char *path);
char *PB_BuildModulePath(serverPbState_t *server, char *path, const char *filename);
void PB_GetBasePath(serverPbState_t *server, char *buffer);
void PB_CopyModuleIfMissing(serverPbState_t *server, const char *filename, const char *sourceBasePath);
void PB_GetHomePath(serverPbState_t *server);
qboolean PB_CopyModuleFile(const char *sourcePath, const char *destPath, int32_t maxBytes);

void PB_CallServerSbGlobal(int32_t opcode, int32_t clientNum, uint32_t length, const char *text)
{
    PB_CallServerSb(&pb_serverObject, opcode, clientNum, (intptr_t)length, (intptr_t)text, 0);
}

void PbServerCompleteCommand(int32_t clientNum, const char *left, const char *right)
{
    char command[PB_SERVER_COMMAND_BUFFER_SIZE + PB_SERVER_STRING_SEPARATOR_SIZE + PB_SERVER_COMMAND_NUL_BYTE];
    uint32_t leftLength = (uint32_t)strlen(left);
    uint32_t rightLength = (uint32_t)strlen(right);
    uint32_t combinedLength = leftLength + rightLength;

    if (combinedLength > PB_SERVER_COMMAND_BUFFER_SIZE) {
        return;
    }

    strcpy(command, left);
    strcat(command, " ");
    strcat(command, right);
    PB_CallServerSbGlobal(PB_SERVER_SB_COMMAND_EXEC, clientNum, strlen(command), command);
}

int32_t PB_ServerCommand(const char *command, intptr_t value)
{
    char *valueText = (char *)value;

    if (strcasecmp(command, pbCommandSetSvPunkbuster) == 0) {
        SV_SetPunkBusterCvar(valueText);
        return 0;
    }

    if (strcasecmp(command, pbCommandConsoleCaptureBufferLength) == 0) {
        pb_consoleCaptureBufferSize = (int32_t)value;
        return 0;
    }

    if (strcasecmp(command, pbCommandConsoleCaptureBuffer) == 0) {
        pb_consoleCaptureBuffer = valueText;
        return 0;
    }

    if (strcasecmp(command, pbCommandExecute) == 0) {
        qboolean isPbCommand = strncasecmp(valueText, pbCommandPrefix, PB_COMMAND_PREFIX_LENGTH) == 0 ? qtrue : qfalse;

        Cmd_ExecuteString(valueText);
        if (isPbCommand != qfalse) {
            PB_CallServerSaCommandDrain();
        }
        return 0;
    }

    char *cursor = valueText;
    while (*cursor == ' ') {
        cursor++;
    }

    while (*cursor != '\0' && *cursor != ' ') {
        cursor++;
    }

    char *valueStart = cursor;
    while (*cursor == ' ') {
        cursor++;
    }

    if (strcasecmp(command, pbCommandDropClient) == 0) {
        PB_DropClient(atoi(valueText), cursor);
        return 0;
    }

    if (strcasecmp(command, pbCommandCvarSet) == 0) {
        char separator = *valueStart;

        *valueStart = '\0';
        Cvar_SetExisting(valueText, cursor);
        *valueStart = separator;
    }

    return 0;
}

const char *PB_ServerQuery(serverPbQuery_t query, char *buffer)
{
    buffer[SERVER_PB_QUERY_COPY_LIMIT] = '\0';

    if (query == PB_SERVER_QUERY_MAX_CLIENTS) {
        itoa(Pb_Q_maxclients(), buffer, PB_INTEGER_DECIMAL_BASE);
        return NULL;
    }

    if (query == PB_SERVER_QUERY_CLIENT_INFO) {
        if (Pb_Q_client((int32_t)atoi(buffer), buffer) == qfalse) {
            return pbQueryFailed;
        }
        return NULL;
    }

    if (query == PB_SERVER_QUERY_CVAR_STRING) {
        strncpy(buffer, Cvar_VariableString(buffer), SERVER_PB_QUERY_COPY_LIMIT);
        return NULL;
    }

    if (query == PB_SERVER_QUERY_CLIENT_STATUS && Pb_Q_stats((int32_t)atoi(buffer), buffer) == qfalse) {
        return pbQueryFailed;
    }

    return NULL;
}

int32_t PB_ServerOutput(const char *text)
{
    if (strncasecmp(pb_serverObject.title, pbSkipNotifyPrefix, sizeof(pbSkipNotifyPrefix) - 1) == 0) {
        Com_Printf(pbOutputFormat, &pb_serverObject.title[sizeof(pbSkipNotifyPrefix) - 1], text);
    } else {
        SV_PrintPunkBusterMessage(pb_serverObject.title, text);
    }

    return 0;
}

void PB_StartServer(void)
{
    PB_SetServerCallbacks(&pb_serverObject);
    PB_CallServerSbGlobal(PB_SERVER_SB_START, PB_SERVER_START_CLIENT_NUM, 0, "");

    if (pb_serverObject.serverSbCallback == NULL) {
        SV_SetPunkBusterCvar("0");
    }
}

void PB_RunServerFrame(void)
{
    PB_CallServerSa(&pb_serverObject, PB_SERVER_SA_FRAME);
}

void PB_CallServerSaCommandDrain(void)
{
    PB_CallServerSa(&pb_serverObject, PB_SERVER_SA_COMMAND_DRAIN);
}

void PbServerForceProcess(int32_t arg0, int32_t arg1)
{
    PB_CallServerSb(&pb_serverObject, PB_SERVER_SB_FORCE_PROCESS, PB_SERVER_START_CLIENT_NUM, arg1, arg0, 0);
}

void PB_InvokeEventCallback(const char *text, const uint8_t *packetData)
{
    if (pb_serverObject.eventCallback != NULL) {
        pb_serverObject.eventCallback(&pb_serverObject, text, (intptr_t)packetData);
    }
}

const char *PB_InvokeStringQueryCallback(const char *text, intptr_t arg1, const char *arg2)
{
    if (pb_serverObject.stringQueryCallback == NULL) {
        return NULL;
    }

    return pb_serverObject.stringQueryCallback(&pb_serverObject, text, arg1, (intptr_t)arg2);
}

void PbServerProcessEvents(void)
{
    PB_CallServerSb(&pb_serverObject, PB_SERVER_SB_MODULE_IDLE, PB_SERVER_START_CLIENT_NUM, 0, 0, 0);
}

void PB_NotifyServerEnabled(void)
{
    PB_CallServerSb(&pb_serverObject, PB_SERVER_SB_NOTIFY_ENABLED, PB_SERVER_START_CLIENT_NUM, 0, 0, 0);
}

void PB_NotifyServerDisabled(void)
{
    PB_CallServerSb(&pb_serverObject, PB_SERVER_SB_NOTIFY_DISABLED, PB_SERVER_START_CLIENT_NUM, 0, 0, 0);
}

void PB_Print(const char *text, int32_t textLimit)
{
    if (pb_serverObject.printCallback != NULL) {
        pb_serverObject.printCallback(&pb_serverObject, text, textLimit);
    }

    if (pb_consoleCaptureBuffer == NULL) {
        return;
    }

    int32_t currentLength = (int32_t)strlen(pb_consoleCaptureBuffer);
    int32_t textLength = (int32_t)strlen(text);
    if (currentLength + textLength < pb_consoleCaptureBufferSize) {
        strcpy(pb_consoleCaptureBuffer + currentLength, text);
    }
}

int32_t PB_ServerChecksum(const char *data, int32_t length, int32_t clientNum)
{
    SV_SendPbPacket(length, data, clientNum);
    return 0;
}

int32_t PB_ServerPacket(const char *address, uint16_t port, const void *data, int32_t length)
{
    Sys_SendPacketByName(address, port, data, length);
    return 0;
}

void PB_ServerLifecycleDispatch(int32_t lifecycleValue, int32_t sentinel)
{
    if (sentinel == PB_LIFECYCLE_SENTINEL && lifecycleValue == PB_LIFECYCLE_START) {
        PB_InitServerObject(&pb_serverObject);
    }

    if (sentinel == PB_LIFECYCLE_SENTINEL && lifecycleValue == PB_LIFECYCLE_SHUTDOWN) {
        PB_ShutdownServerObject(&pb_serverObject);
    }
}

void PbServerInitialize(void)
{
    PB_ServerLifecycleDispatch(PB_LIFECYCLE_START, PB_LIFECYCLE_SENTINEL);
}

void PbServerShutdown(void)
{
    PB_ServerLifecycleDispatch(PB_LIFECYCLE_SHUTDOWN, PB_LIFECYCLE_SENTINEL);
}

char *itoa(int32_t value, char *buffer, int32_t base)
{
    char digits[PB_INTEGER_CONVERSION_DIGIT_BUFFER_SIZE];
    int32_t digitIndex = PB_INTEGER_CONVERSION_DIGIT_BUFFER_SIZE - 1;
    uint32_t workingValue = (uint32_t)value;

    if (buffer == NULL) {
        return NULL;
    }

    strcpy(buffer, "0");
    if (value == 0 || base <= 1 || base > PB_INTEGER_CONVERSION_MAX_BASE) {
        return buffer;
    }

    digits[digitIndex] = '\0';
    if (value < 0 && base == 10) {
        workingValue = 0U - (uint32_t)value;
    }

    while (workingValue != 0) {
        uint32_t digit = workingValue % base;

        digitIndex--;
        digits[digitIndex] = (char)(digit < 10 ? digit + '0' : digit + 'W');
        workingValue /= base;
    }

    if (value < 0 && base == 10) {
        digitIndex--;
        digits[digitIndex] = '-';
    }

    strcpy(buffer, &digits[digitIndex]);
    return buffer;
}

void PB_SetServerCallbacks(serverPbState_t *server)
{
    PB_ClearServerCallbacks(server);
    server->commandCallback = PB_ServerCommand;
    server->queryCallback = PB_ServerQuery;
    server->outputCallback = PB_ServerOutput;
    server->checksumCallback = PB_ServerChecksum;
    server->packetCallback = PB_ServerPacket;
}

void PB_CloseAuxiliaryModuleHandle0c(serverPbState_t *server)
{
    if (server->moduleHandle0c != NULL) {
        dlclose(server->moduleHandle0c);
    }

    server->moduleHandle0c = NULL;
}

void PB_CloseAuxiliaryModuleHandle10(serverPbState_t *server)
{
    server->opaque164 = 0;
    if (server->moduleHandle10 != NULL) {
        dlclose(server->moduleHandle10);
    }

    server->moduleHandle10 = NULL;
}

void PB_ClearServerCallbacks(serverPbState_t *server)
{
    server->commandCallback = NULL;
    server->queryCallback = NULL;
    server->outputCallback = NULL;
    server->checksumCallback = NULL;
}

void PB_UnloadServerDll(serverPbState_t *server)
{
    server->opaque04 = 0;
    server->serverSaCallback = NULL;
    server->serverSbCallback = NULL;
    server->eventCallback = NULL;
    server->stringQueryCallback = NULL;
    server->printCallback = NULL;

    if (server->serverModuleHandle != NULL) {
        dlclose(server->serverModuleHandle);
    }

    server->serverModuleHandle = NULL;
}

intptr_t PB_CallServerSb(serverPbState_t *server, int32_t opcode, intptr_t arg0, intptr_t arg1, intptr_t arg2, intptr_t arg3)
{
    if (server->commandCallback == NULL) {
        return 0;
    }

    if (server->loadPending != 0 || server->serverModuleHandle == NULL) {
        if (server->serverModuleHandle != NULL) {
            PB_UnloadServerDll(server);
            return 0;
        }

        const char *loadResult = PB_LoadServerModule(server);
        if (loadResult != NULL) {
            if (opcode != PB_SERVER_SB_MODULE_IDLE && opcode != PB_SERVER_SB_MODULE_SHUTDOWN) {
                return (intptr_t)loadResult;
            }
            return 0;
        }
    }

    return server->serverSbCallback(server, opcode, arg0, arg1, arg2, arg3);
}

int32_t PB_CallServerSa(serverPbState_t *server, serverPbSaValue_t value)
{
    if (server->commandCallback == NULL) {
        return 0;
    }

    if (server->serverModuleHandle == NULL) {
        if (server->loadPending != 0) {
            PB_CallServerSb(server, PB_SERVER_SB_START, PB_SERVER_START_CLIENT_NUM, 0, (intptr_t)"", 0);
        }
        return 0;
    }

    if (server->loadPending != 0) {
        PB_UnloadServerDll(server);
        return 0;
    }

    return server->serverSaCallback(server, value);
}

void PB_InitServerObject(serverPbState_t *server)
{
    server->magic = SERVER_PUNKBUSTER_MAGIC;
    strcpy(server->title, pbTitle);
    server->serverModuleHandle = NULL;
    server->loadPending = 1;
    PB_ClearServerCallbacks(server);
    server->opaque04 = 0;
    server->serverSbCallback = NULL;
    server->serverSaCallback = NULL;
    server->packetCallback = NULL;
    server->eventCallback = NULL;
    server->stringQueryCallback = NULL;
    server->printCallback = NULL;
}

void PB_ShutdownServerObject(serverPbState_t *server)
{
    PB_UnloadServerDll(server);
    PB_CloseAuxiliaryModuleHandle0c(server);
    PB_CloseAuxiliaryModuleHandle10(server);
}

const char *PB_LoadServerModule(serverPbState_t *server)
{
    char modulePath[MAX_OSPATH];
    char renamePath[MAX_OSPATH];
    const char *renameDestination;
    const char *renameSource;

    if (server->serverModuleHandle != NULL) {
        return NULL;
    }

    PB_UnloadServerDll(server);
    FILE *moduleFile = fopen(PB_BuildModulePath(server, modulePath, pbModuleServerNew), pbFileReadMode);
    if (moduleFile != NULL) {
        fclose(moduleFile);

        PB_ChmodModulePath(PB_BuildModulePath(server, modulePath, pbModuleServerOld));
        remove(PB_BuildModulePath(server, modulePath, pbModuleServerOld));
        renameDestination = PB_BuildModulePath(server, renamePath, pbModuleServerOld);
        renameSource = PB_BuildModulePath(server, modulePath, pbModuleServer);
        rename(renameSource, renameDestination);

        PB_ChmodModulePath(PB_BuildModulePath(server, modulePath, pbModuleServer));
        remove(PB_BuildModulePath(server, modulePath, pbModuleServer));
        renameDestination = PB_BuildModulePath(server, renamePath, pbModuleServer);
        renameSource = PB_BuildModulePath(server, modulePath, pbModuleServerNew);
        rename(renameSource, renameDestination);
    }

    server->serverModuleHandle = dlopen(PB_BuildModulePath(server, modulePath, pbModuleServer), PB_DLOPEN_LAZY);
    if (server->serverModuleHandle == NULL) {
        return pbLoadFailure;
    }

    server->serverSaCallback = coduomp_pb_load_server_sa_symbol(server->serverModuleHandle, pbExportServerSa);
    server->serverSbCallback = coduomp_pb_load_server_sb_symbol(server->serverModuleHandle, pbExportServerSb);
    if (server->serverSaCallback == NULL || server->serverSbCallback == NULL) {
        PB_UnloadServerDll(server);
        return pbProcedureFailure;
    }

    server->loadPending = 0;
    return NULL;
}

void PB_ChmodModulePath(const char *path)
{
    chmod(path, PB_MODULE_FILE_MODE);
}

char *PB_BuildModulePath(serverPbState_t *server, char *path, const char *filename)
{
    char basePath[SERVER_PB_BASE_PATH_SIZE];

    if (server->basePath[0] == '\0') {
        PB_GetHomePath(server);
        PB_GetBasePath(server, basePath);

        size_t baseLength = strlen(basePath);
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (baseLength != 0 && basePath[baseLength - 1] != '/') {
            strcat(basePath, pbPathSeparator);
        }
        strcat(basePath, pbDirectoryName);

        if (strcasecmp(basePath, server->basePath) != 0 && basePath[0] != '\0' && server->basePath[0] != '\0') {
            mkdir(server->basePath, PB_MODULE_FILE_MODE);
            PB_CopyModuleIfMissing(server, pbModuleServer, basePath);
            PB_CopyModuleIfMissing(server, pbModuleClient, basePath);
            PB_CopyModuleIfMissing(server, pbModuleAgent, basePath);
        }
    }

    strcpy(path, server->basePath);
    strcat(path, filename);
    return path;
}

void PB_GetBasePath(serverPbState_t *server, char *buffer)
{
    if (server->queryCallback != NULL) {
        strcpy(buffer, pbCvarBasepath);
        server->queryCallback(PB_SERVER_QUERY_CVAR_STRING, buffer);
    }
}

void PB_CopyModuleIfMissing(serverPbState_t *server, const char *filename, const char *sourceBasePath)
{
    char targetPath[MAX_OSPATH];
    char sourcePath[MAX_OSPATH];

    strcpy(targetPath, server->basePath);
    strcat(targetPath, filename);

    FILE *targetFile = fopen(targetPath, pbFileReadMode);
    if (targetFile != NULL) {
        fclose(targetFile);
        return;
    }

    strcpy(sourcePath, sourceBasePath);
    strcat(sourcePath, filename);
    PB_CopyModuleFile(sourcePath, targetPath, PB_COPY_MODULE_NO_SIZE_LIMIT);
}

void PB_GetHomePath(serverPbState_t *server)
{
    if (server->queryCallback == NULL) {
        return;
    }

    strcpy(server->basePath, pbCvarHomepath);
    server->queryCallback(PB_SERVER_QUERY_CVAR_STRING, server->basePath);
    if (server->basePath[0] == '\0') {
        getcwd(server->basePath, PB_HOME_PATH_GETCWD_LIMIT);
    }

    if (server->basePath[0] != '\0') {
        size_t baseLength = strlen(server->basePath);
        if (server->basePath[baseLength - 1] != '/') {
            strcat(server->basePath, pbPathSeparator);
        }
    }

    strcat(server->basePath, pbDirectoryName);
}

qboolean PB_CopyModuleFile(const char *sourcePath, const char *destPath, int32_t maxBytes)
{
    qboolean copied = qfalse;
    FILE *sourceFile = fopen(sourcePath, pbFileReadMode);

    if (sourceFile == NULL) {
        return copied;
    }

    FILE *destFile = fopen(destPath, pbFileWriteMode);
    if (destFile != NULL) {
        fseek(sourceFile, 0, PB_COPY_FILE_SEEK_END);
        int32_t fileSize = (int32_t)ftell(sourceFile);

        if (fileSize > 0 && (maxBytes == PB_COPY_MODULE_NO_SIZE_LIMIT || fileSize < maxBytes)) {
            void *buffer = _Znaj((uint32_t)fileSize);

            if (buffer != NULL) {
                fseek(sourceFile, 0, PB_COPY_FILE_SEEK_SET);
                size_t bytesRead = fread(buffer, 1, (size_t)fileSize, sourceFile);
                size_t bytesWritten = fwrite(buffer, 1, bytesRead, destFile);

                _ZdlPv(buffer);
                if (bytesWritten == (size_t)fileSize) {
                    copied = qtrue;
                }
            }
        }

        fclose(destFile);
    }

    fclose(sourceFile);
    return copied;
}
