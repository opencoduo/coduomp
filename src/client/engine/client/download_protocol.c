#include "cgame.h"
#include "download.h"
#include "server_browser.h"

#include "filesystem/filesystem.h"
#include "../filesystem/server_namespace.h"
#include "../platform/case_sensitive_fs.h"
#include "filesystem/filesystem_path_security.h"
#include "../system_platform.h"
#include "../ui/ui_module_loader.h"

#include <stdio.h>
#include <string.h>

enum {
    CL_DOWNLOAD_NAME_CAPACITY = 256,
    CL_DOWNLOAD_LIST_CAPACITY = 1024,
    CL_UPDATE_FILE_NAME_MINIMUM_LENGTH = 5,
    CL_UPDATE_PATH_SHIFT = 7,
};

/* Original Win32 storage at 0x04ad3d00. During an autoupdate download this
 * retains the single update filename until CL_DownloadsComplete launches it. */
char cl_updateFileName[MAX_QPATH];
/* Original 0x0389fce8. This remembers that the aborting state was already
 * reported while the server completes the WWW-download abort handshake. */
static qboolean cl_wwwDownloadAbortReported;

/* Source: CoDUOMP.exe 0x00413420..0x004136df.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00413420_004136e0.mcode.
 * Name and state transitions: exact same-module Mac symbol
 * CL_WWWDownload. */
void CL_WWWDownload(void)
{
    if (clc.wwwDownloadAborting != qfalse) {
        if (cl_wwwDownloadAbortReported != qfalse)
            return;

        Com_DPrintf("CL_WWWDownload: WWWDlAborting\n");
        cl_wwwDownloadAbortReported = qtrue;
        return;
    }

    if (cl_wwwDownloadAbortReported != qfalse) {
        Com_DPrintf("CL_WWWDownload: WWWDlAborting done\n");
        cl_wwwDownloadAbortReported = qfalse;
    }

    const dlDownloadResult_t result = DL_DownloadLoop();
    if (result == DL_DOWNLOAD_CONTINUE)
        return;

    if (result == DL_DOWNLOAD_SUCCESS) {
        char destinationPath[MAX_OSPATH];

        clc.downloadFile = 0;
        if (coduomp_server_namespace_build_download_path(
                cls.staticDownload.originalDownloadName,
                destinationPath, sizeof(destinationPath)) == qfalse) {
            Com_Error(ERR_DROP,
                      "Refusing invalid redirected download path\n");
            return;
        }

        if (rename(cls.staticDownload.downloadTempName,
                   destinationPath) != 0) {
            FS_Copyfiles(
                cls.staticDownload.downloadTempName,
                destinationPath);
            FS_Remove(cls.staticDownload.downloadTempName);
        }
        coduomp_case_path_cache_clear();

        Cvar_Set("cl_downloadName", "");
        cls.staticDownload.downloadName[0] = '\0';
        cls.staticDownload.downloadTempName[0] = '\0';

        if (cls.wwwDownloadDisconnected != 0) {
            if (cl_updateStarted == qfalse) {
                Cbuf_AddText("reconnect\n");
                clc.wwwDownloadActive = qfalse;
                CL_NextDownload();
                return;
            }
        } else {
            CL_AddReliableCommand("wwwdl done");

            const size_t historyLength =
                strlen(clc.redirectedList);
            const size_t nameLength = strlen(
                cls.staticDownload.originalDownloadName);
            if (historyLength + nameLength + 1u >=
                sizeof(clc.redirectedList)) {
                Com_Printf(
                    "ERROR: redirectedList overflow (%s)\n",
                    cls.staticDownload.originalDownloadName);
                clc.wwwDownloadActive = qfalse;
                CL_NextDownload();
                return;
            }

            strcat(clc.redirectedList, "@");
            strcat(
                clc.redirectedList,
                cls.staticDownload.originalDownloadName);
        }

        clc.wwwDownloadActive = qfalse;
        CL_NextDownload();
        return;
    }

    if (cls.wwwDownloadDisconnected != 0) {
        const char *const errorMessage = va(
            "Download failure while getting '%s'\n",
            cls.staticDownload.downloadName);
        cls.wwwDownloadDisconnected = 0;
        CL_ClearStaticDownload();
        /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
        Com_Error(ERR_DROP, "%s", errorMessage);
        return;
    }

    Com_Printf(
        "Download failure while getting '%s'\n",
        cls.staticDownload.downloadName);
    CL_AddReliableCommand("wwwdl fail");
    clc.wwwDownloadAborting = qtrue;
}

/* Source: CoDUOMP.exe 0x004136e0..0x004137ba.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004136e0_004137bb.mcode.
 * Name and pak-name argument: exact same-module Mac symbol
 * CL_WWWBadChecksum. A downloaded pak is removed only when it appears in the
 * redirect history; the separate bad-checksum history prevents redirecting it
 * again during the same connection. */
qboolean CL_WWWBadChecksum(const char *pakName)
{
    if (strstr(
            clc.redirectedList,
            va("@%s", pakName)) == NULL) {
        return qfalse;
    }

    Com_Printf(
        "WARNING: file %s obtained through download redirect "
        "has wrong checksum\n",
        pakName);
    Com_Printf(
        "         this likely means the server configuration "
        "is broken\n");

    const size_t historyLength =
        strlen(clc.badChecksumList);
    const size_t nameLength = strlen(pakName);
    if (historyLength + nameLength + 1u >=
        sizeof(clc.badChecksumList)) {
        Com_Printf(
            "ERROR: badChecksumList overflowed (%s)\n",
            clc.badChecksumList);
        return qfalse;
    }

    strcat(clc.badChecksumList, "@");
    strcat(clc.badChecksumList, pakName);
    Com_DPrintf(
        "bad checksums: %s\n", clc.badChecksumList);
    return qtrue;
}

/* Source: CoDUOMP.exe 0x00411940..0x00411abe.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00411940_00411abf.mcode.
 * Name and no-argument signature: same-module Mac symbol
 * CL_DownloadsComplete. Autoupdate completion launches the downloaded
 * updater; ordinary pure-server downloads restart the filesystem before
 * telling the server "donedl"; a no-download path enters map loading. */
void CL_DownloadsComplete(void)
{
    (void)Cvar_Set2(
        "cl_downloadName", "", qtrue);

    if (cl_updateStarted != qfalse) {
        if (strlen(cl_updateFileName) >=
            CL_UPDATE_FILE_NAME_MINIMUM_LENGTH) {
            const char *const updateDirectory =
                FS_ShiftStr("ni]Zm^l", CL_UPDATE_PATH_SHIFT);
            const char *const updatePath =
                va("%s/%s", updateDirectory, cl_updateFileName);

            if (cls.wwwDownloadDisconnected != 0) {
                cls.wwwDownloadDisconnected = 0;
                CL_ClearStaticDownload();
            }
            Sys_StartProcess(updatePath, qtrue);
        }

        cl_updateStarted = qfalse;
        if (cls.wwwDownloadDisconnected == 0)
            CL_Disconnect(qtrue);
        cls.wwwDownloadDisconnected = 0;
        CL_ClearStaticDownload();
        return;
    }

    if (cls.staticDownload.downloadRestart != qfalse) {
        cls.staticDownload.downloadRestart = qfalse;
        FS_Restart(clc.checksumFeed);
        if (cls.wwwDownloadDisconnected == 0)
            CL_AddReliableCommand("donedl");
        cls.wwwDownloadDisconnected = 0;
        CL_ClearStaticDownload();
        return;
    }

    cls.state = CA_LOADING;
    (void)Com_EventLoop();
    if (cls.state != CA_LOADING)
        return;

    (void)Cvar_Set2(
        "r_uiFullScreen", "0", qtrue);
    if (sv_running->integer == 0) {
        CL_ShutdownAll();
        Hunk_ClearToStart();
    } else if (coduo_cgameVm != NULL) {
        return;
    }

    CL_StartHunkUsers();
    (void)Cvar_Set2("cl_paused", "1", qtrue);
    CL_InitCGame();
    CL_SendPureChecksums();
    CL_WritePacket();
    CL_WritePacket();
    CL_WritePacket();
}

/* Source: CoDUOMP.exe 0x00411ac0..0x00411b74.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00411ac0_00411b75.mcode.
 * Name and argument order: same-module Mac symbol CL_BeginDownload. This is
 * the in-band UDP download request; the temporary filename and counters are
 * initialized before the reliable "download" command is queued. */
void CL_BeginDownload(const char *localName, const char *remoteName)
{
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    if (coduo_compat_path_is_safe_relative(localName) == qfalse ||
        coduo_compat_path_is_safe_relative(remoteName) == qfalse) {
        Com_Error(
            ERR_DROP,
            "Refusing invalid download path\n");
        return;
    }

    Com_DPrintf(
        "***** CL_BeginDownload *****\n"
        "Localname: %s\n"
        "Remotename: %s\n"
        "****************************\n",
        localName, remoteName);

    strncpy(
        cls.staticDownload.downloadName, localName,
        CL_DOWNLOAD_NAME_CAPACITY - 1);
    cls.staticDownload.downloadName[
        CL_DOWNLOAD_NAME_CAPACITY - 1] = '\0';
    Com_sprintf(
        cls.staticDownload.downloadTempName,
        CL_DOWNLOAD_NAME_CAPACITY, "%s.tmp", localName);

    (void)Cvar_Set2(
        "cl_downloadName", remoteName, qtrue);
    (void)Cvar_Set2(
        "cl_downloadSize", "0", qtrue);
    (void)Cvar_Set2(
        "cl_downloadCount", "0", qtrue);
    Cvar_SetValue("cl_downloadTime", (float)cls.realtime);

    clc.downloadBlock = 0;
    clc.downloadCount = 0;
    CL_AddReliableCommand(va("download %s", remoteName));
}

/* Source: CoDUOMP.exe 0x00411b80..0x00411c1c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00411b80_00411c1d.mcode.
 * Name and no-argument signature: same-module Mac symbol CL_NextDownload.
 * The download list is the original '@remote@local' stream emitted by
 * FS_ComparePaks; each accepted pair is removed before packet transfer starts. */
void CL_NextDownload(void)
{
    char *remoteName = clc.downloadList;
    if (remoteName[0] == '@')
        ++remoteName;

    if (remoteName[0] == '\0') {
        CL_DownloadsComplete();
        return;
    }

    char *const remoteEnd = strchr(remoteName, '@');
    if (remoteEnd == NULL) {
        CL_DownloadsComplete();
        return;
    }
    *remoteEnd = '\0';

    char *const localName = remoteEnd + 1;
    char *localEnd = strchr(localName, '@');
    char *remaining;
    if (localEnd != NULL) {
        *localEnd = '\0';
        remaining = localEnd + 1;
    } else {
        remaining = localName + strlen(localName);
    }

    CL_BeginDownload(localName, remoteName);
    cls.staticDownload.downloadRestart = qtrue;
    memmove(
        clc.downloadList, remaining,
        strlen(remaining) + 1);
}

/* Source: CoDUOMP.exe 0x00411c20..0x00411e58.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00411c20_00411e59.mcode.
 * Name and no-argument signature: same-module Mac symbol CL_InitDownloads.
 * Autoupdate requests are accepted only from the selected update server.
 * Ordinary missing-pak downloads require both client and server permission;
 * otherwise the readable missing-pak list is reported and loading continues. */
void CL_InitDownloads(void)
{
    char missingPaks[CL_DOWNLOAD_LIST_CAPACITY];
    const char *const updateDirectory =
        FS_ShiftStr("ni]Zm^l", CL_UPDATE_PATH_SHIFT);

    clc.wwwDownloadActive = qfalse;
    clc.wwwDownloadAborting = qfalse;
    cls.wwwDownloadDisconnected = 0;
    CL_ClearStaticDownload();

    if (cl_updateStarted != qfalse &&
        NET_CompareAdrSigned(
            &clc.serverAddress, &cls.autoUpdateServer) == 0 &&
        strlen(cl_updateFiles->string) >=
            CL_UPDATE_FILE_NAME_MINIMUM_LENGTH) {
        strncpy(
            cl_updateFileName, cl_updateFiles->string,
            sizeof(cl_updateFileName) - 1);
        cl_updateFileName[sizeof(cl_updateFileName) - 1] = '\0';

        strncpy(
            clc.downloadList,
            va("@%s/%s@%s/%s",
               updateDirectory, cl_updateFiles->string,
               updateDirectory, cl_updateFiles->string),
            sizeof(clc.downloadList) - 1);
        clc.downloadList[sizeof(clc.downloadList) - 1] = '\0';
        cls.state = CA_CONNECTED;
        CL_NextDownload();
        return;
    }

    if (sv_running->integer == 0 &&
        cl_allowDownload->integer != 0 &&
        cl_serverAllowDownload->integer != 0 &&
        FS_ComparePaks(
            clc.downloadList, sizeof(clc.downloadList),
            qtrue) != qfalse) {
        Com_Printf("Need paks: %s\n", clc.downloadList);
        if (clc.downloadList[0] != '\0') {
            cls.state = CA_CONNECTED;
            CL_NextDownload();
            return;
        }
        CL_DownloadsComplete();
        return;
    }

    if (FS_ComparePaks(
            missingPaks, sizeof(missingPaks), qfalse) != qfalse) {
        if (cl_serverAllowDownload->integer == 0) {
            Com_Printf(
                "\nWARNING: You are missing some files referenced by "
                "the server:\n%s"
                "You might not be able to join the game\n"
                "Go to the settings menu to turn on autodownload, "
                "or get the file elsewhere\n\n",
                missingPaks);
        } else {
            Com_Printf(
                "\nWARNING: You are missing some files referenced by "
                "the server:\n%s"
                "You might not be able to join the game\n"
                "If you are unable to join, you will need to get the "
                "file elsewhere\n\n",
                missingPaks);
        }
    }

    CL_DownloadsComplete();
}
