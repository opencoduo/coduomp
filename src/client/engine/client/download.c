#include "download.h"

#include "filesystem/filesystem.h"
#include "../platform/crt_boundary.h"
#include "../q_shared.h"
#include "../platform/libwww_boundary.h"

#include <stdarg.h>
#include <stdint.h>
#include <string.h>

static qboolean dl_initialized;          /* original 0x0389fd4c */
static qboolean dl_running;              /* original 0x0389fd50 */
static qboolean dl_forceRawBytesCount;   /* original 0x0389fd54 */
static HTRequest *dl_request;            /* original 0x0389fd58 */
static int32_t dl_requestStatus = -1000; /* original 0x005c51fc */

/* Source: CoDUOMP.exe 0x0043ee70..0x0043ee80, recovered from the executable
 * gap before DL_InitDownload. Registered as the global libwww after-filter;
 * the fourth callback argument is stored as the completed request status. */
static int32_t DL_AfterFilter(HTRequest *request, HTResponse *response, void *parameter, int32_t status)
{
    (void)request;
    (void)response;
    (void)parameter;

    dl_requestStatus = status;
    HTEventList_stopLoop();
    return HT_OK;
}

/* Source: CoDUOMP.exe 0x0043ee90..0x0043ef18, recovered from the executable
 * gap. Name: exact same-module Mac symbol HTAlertCallback_progress. The
 * HTRequest_net accessor is the typed source-level form of the inlined
 * request->net load at Windows request offset 0x18. */
int32_t HTAlertCallback_progress(HTRequest *request, HTAlertOpcode opcode, int32_t messageNumber, const char *defaultMessage, void *input,
                                 HTAlertPar *reply)
{
    enum {
        HT_PROG_READ = 8
    };
    (void)messageNumber;
    (void)defaultMessage;
    (void)input;
    (void)reply;

    if (opcode != HT_PROG_READ)
        return HT_TRUE;

    if (dl_forceRawBytesCount == qfalse) {
        Cvar_SetValue("cl_downloadCount", (float)HTRequest_bytesRead(request));
        return HT_TRUE;
    }

    HTNet *const net = HTRequest_net(request);
    if (HTNet_rawBytesCount(net) == HT_FALSE) {
        Com_DPrintf("Force raw byte count on request->net %p\n", (void *)net);
        HTFTP_setRawBytesCount(request);
    }

    Cvar_SetValue("cl_downloadCount", (float)HTFTP_getDNetRawBytesCount(request));
    return HT_TRUE;
}

/* Source: CoDUOMP.exe 0x0043ef20..0x0043ef51, recovered from the executable
 * gap. Name: exact same-module Mac symbol HTAlertCallback_confirm. */
int32_t HTAlertCallback_confirm(HTRequest *request, HTAlertOpcode opcode, int32_t messageNumber, const char *defaultMessage, void *input,
                                HTAlertPar *reply)
{
    enum {
        HT_CONFIRM_REPLACE_EXISTING = 9
    };
    (void)request;
    (void)opcode;
    (void)defaultMessage;
    (void)input;
    (void)reply;

    if (messageNumber == HT_CONFIRM_REPLACE_EXISTING) {
        Com_Printf("Replace existing download target file\n");
        return HT_TRUE;
    }

    Com_Printf("Aborting, unknown libwww confirm message id: %d\n", messageNumber);
    HTEventList_stopLoop();
    return HT_FALSE;
}

/* Source: CoDUOMP.exe 0x0043ef60..0x0043ef79, recovered from the executable
 * gap. Name: exact same-module Mac symbol HTAlertCallback_prompt. */
int32_t HTAlertCallback_prompt(HTRequest *request, HTAlertOpcode opcode, int32_t messageNumber, const char *defaultMessage, void *input,
                               HTAlertPar *reply)
{
    (void)request;
    (void)opcode;
    (void)defaultMessage;
    (void)input;
    (void)reply;

    Com_Printf("Aborting, libwww prompt message id: %d "
               "(prompted for a login/password?)\n",
               messageNumber);
    HTEventList_stopLoop();
    return HT_FALSE;
}

/* Source: CoDUOMP.exe 0x0043ef80..0x0043f018.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0043ef80_0043f019.mcode.
 * Name: exact same-module Mac symbol DL_InitDownload. */
void DL_InitDownload(void)
{
    if (dl_initialized != qfalse)
        return;

    HTProfile_newNoCacheClient("ID_DOWNLOAD", "1.0");
    HTAlertInit();
    HTAlert_setInteractive(HT_TRUE);
    HTPrint_setCallback(Com_VPrintf);
    HTTrace_setCallback(Com_VPrintf);
    (void)HTNet_addAfter(DL_AfterFilter, NULL, NULL, HT_ALL, HT_FILTER_LAST);
    (void)HTAlert_add(HTAlertCallback_progress, HT_A_PROGRESS);
    (void)HTAlert_add(HTAlertCallback_confirm, HT_A_CONFIRM);
    (void)HTAlert_add(HTAlertCallback_prompt, HT_A_PROMPT_MASK);

    Com_Printf("Client download subsystem initialized\n");
    dl_initialized = qtrue;
}

/* Source: CoDUOMP.exe 0x0043f020..0x0043f033.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0043f020_0043f034.mcode.
 * Role name: the Mac traceback table has no symbol for this Windows-retained
 * body. It conditionally clears the download-subsystem initialization flag;
 * the body performs no libwww teardown. */
void DL_Shutdown(void)
{
    if (dl_initialized != qfalse)
        dl_initialized = qfalse;
}

/* Source: CoDUOMP.exe 0x0043f040..0x0043f343.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0043f040_0043f344.mcode, with the
 * same control flow independently checked against the named PowerPC
 * DL_BeginDownload body. Libwww-owned allocations are released through
 * HTMemory_free rather than the host CRT allocator. */
qboolean DL_BeginDownload(char *localFileName, const char *remoteUrl)
{
    enum {
        DL_HTTP_TIMEOUT_MILLISECONDS = 30000,
        DL_HTTP_URL_PREFIX_CAPACITY = 8
    };
    char *requestUrl = NULL;
    char *scheme;

    if (dl_running != qfalse) {
        Com_Printf("ERROR: DL_BeginDownload called with a download "
                   "request already active\n");
        return qfalse;
    }

    dl_requestStatus = -1000;
    if (localFileName == NULL || remoteUrl == NULL) {
        Com_DPrintf("Empty download URL or empty local file name\n");
        return qfalse;
    }

    DL_InitDownload();
    scheme = HTParse(remoteUrl, "", HT_PARSE_ACCESS);
    if (coduo_crt_stricmp(scheme, "ftp") == 0) {
        dl_forceRawBytesCount = qtrue;
        HTHost_setEventTimeout(-1);
    } else {
        dl_forceRawBytesCount = qfalse;
        HTHost_setEventTimeout(DL_HTTP_TIMEOUT_MILLISECONDS);
    }

    dl_request = HTRequest_new();
    if (coduo_crt_stricmp(scheme, "http") == 0) {
        char *host = HTParse(remoteUrl, "", HT_PARSE_HOST);
        char *path = HTParse(remoteUrl, "", HT_PARSE_PATH | HT_PARSE_PUNCTUATION);
        char *hostSeparator = strchr(host, '@');

        if (hostSeparator != NULL) {
            char *password;
            HTBasic *credentials;

            *hostSeparator = '\0';
            password = strchr(host, ':');
            if (password != NULL) {
                *password++ = '\0';
                (void)HTUnEscape(password);
            }
            (void)HTUnEscape(host);

            credentials = HTBasic_new();
            (void)HTSACopy(&credentials->uid, host);
            (void)HTSACopy(&credentials->pw, password);
            basic_credentials(dl_request, credentials);
            (void)HTBasic_delete(credentials);

            ++hostSeparator;
            const size_t requestUrlCapacity = strlen(hostSeparator) + strlen(path) + DL_HTTP_URL_PREFIX_CAPACITY;
            requestUrl = HTMemory_malloc(requestUrlCapacity);
            (void)coduo_crt_snprintf(requestUrl, requestUrlCapacity, "http://%s%s", hostSeparator, path);
            Com_DPrintf("HTTP Basic Auth - %s %s %s\n", host, password, requestUrl);

            HTMemory_free(host);
            HTMemory_free(path);
        } else {
            /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
            (void)HTSACopy(&requestUrl, remoteUrl);
            HTMemory_free(host);
            HTMemory_free(path);
        }
    } else {
        (void)HTSACopy(&requestUrl, remoteUrl);
    }

    HTMemory_free(scheme);
    (void)FS_CreatePath(localFileName);
    if (HTLoadToFile(requestUrl, dl_request, localFileName) != HT_TRUE) {
        Com_DPrintf("HTLoadToFile failed\n");
        HTMemory_free(requestUrl);
        requestUrl = NULL;
        HTProfile_delete();
        return qfalse;
    }

    HTMemory_free(requestUrl);
    requestUrl = NULL;

    scheme = HTParse(remoteUrl, "", HT_PARSE_ACCESS);
    {
        char *host = HTParse(remoteUrl, "", HT_PARSE_HOST);
        char *path = HTParse(remoteUrl, "", HT_PARSE_PATH | HT_PARSE_PUNCTUATION);
        char *hostSeparator = strchr(host, '@');

        if (hostSeparator != NULL) {
            Cvar_Set("cl_downloadName", va("%s://*:*%s%s", scheme, hostSeparator, path));
        } else {
            Cvar_Set("cl_downloadName", remoteUrl);
        }

        HTMemory_free(path);
        HTMemory_free(host);
    }
    HTMemory_free(scheme);

    if (dl_forceRawBytesCount != qfalse)
        HTHost_setEventTimeout(DL_HTTP_TIMEOUT_MILLISECONDS);

    HTEventList_init(dl_request);
    dl_running = qtrue;
    return qtrue;
}

/* Source: CoDUOMP.exe 0x0043f350..0x0043f3d3.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0043f350_0043f3d4.mcode.
 * Name: exact same-module Mac symbol DL_DownloadLoop. Return values 0, 1,
 * and 2 respectively mean still active, finished/no active request, and
 * finished with a negative libwww status. */
dlDownloadResult_t DL_DownloadLoop(void)
{
    if (dl_running == qfalse) {
        Com_DPrintf("DL_DownloadLoop: unexpected call with "
                    "dl_running == qfalse\n");
        return DL_DOWNLOAD_SUCCESS;
    }

    if (HTEventList_pump() != 0)
        return DL_DOWNLOAD_CONTINUE;

    HTEventList_unregisterAll();
    HTHost_setEventTimeout(-1);
    HTRequest_delete(dl_request);
    dl_request = NULL;
    dl_running = qfalse;
    Cvar_Set("ui_dl_running", "0");

    if (dl_requestStatus < 0) {
        Com_DPrintf("DL_DownloadLoop: request terminated with "
                    "failure status %d\n",
                    dl_requestStatus);
        return DL_DOWNLOAD_FAILURE;
    }
    return DL_DOWNLOAD_SUCCESS;
}
