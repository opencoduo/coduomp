#include "libwww_boundary.h"

#include <curl/curl.h>

/* NOT_FROM_ORIGINAL_SOURCE_STORAGE_FILE: all private globals in this
 * translation unit belong to the native libcurl compatibility adapter. */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    LIBWWW_MILLISECONDS_PER_SECOND = 1000,
    LIBWWW_MINIMUM_ACTIVITY_BYTES_PER_SECOND = 1
};

struct _HTNet {
    HTBool rawBytesCount;
    long bytesRead;
};

struct _HTRequest {
    char *url;
    char *targetPath;
    char *userName;
    char *password;
    long bytesRead;
    HTNet net;
    CURL *easyHandle;
    CURLM *multiHandle;
    FILE *outputFile;
    char errorBuffer[CURL_ERROR_SIZE];
};

struct _HTResponse {
    int32_t unused;
};

struct _HTAlertPar {
    int32_t unused;
};

/* NOT_FROM_ORIGINAL_SOURCE: private state owned by the native libcurl adapter,
 * not recovered objects from the original executable. */
static HTNetAfter *libwwwAfterCallback;
static void *libwwwAfterParameter;
static int32_t libwwwAfterStatus = HT_ALL;
static HTAlertCallback *libwwwProgressCallback;
static HTAlertCallback *libwwwConfirmCallback;
static HTAlertCallback *libwwwPromptCallback;
static HTPrintCallback *libwwwPrintCallback;
static HTPrintCallback *libwwwTraceCallback;
static HTRequest *libwwwEventRequest;
static int32_t libwwwTimeoutMilliseconds = -1;
static HTBool libwwwStopRequested;
static HTBool libwwwCurlInitialized;

static int32_t Libwww_BeginTransfer(HTRequest *request);

/* NOT_FROM_ORIGINAL_SOURCE: diagnostic bridge for the native libcurl
 * compatibility adapter. */
static void coduomp_libwww_print(const char *format, ...)
{
    va_list arguments;

    if (libwwwPrintCallback == NULL)
        return;
    va_start(arguments, format);
    (void)libwwwPrintCallback(format, arguments);
    va_end(arguments);
}

/* NOT_FROM_ORIGINAL_SOURCE: libcurl requires process-wide initialization,
 * while the retained caller can tear down the profile after a setup failure
 * without clearing its own initialized flag. Reinitialize on demand so a
 * later HTTP request remains usable. */
static HTBool coduomp_libwww_ensure_curl_initialized(void)
{
    if (libwwwCurlInitialized != HT_FALSE)
        return HT_TRUE;
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
        return HT_FALSE;
    libwwwCurlInitialized = HT_TRUE;
    return HT_TRUE;
}

/*
 * NOT_FROM_ORIGINAL_SOURCE: libcurl compatibility implementation for the
 * small statically linked libwww surface used by the original download code.
 * It preserves that source-level boundary so the recovered download state
 * machine remains intact. The original event loop advances a transfer over
 * multiple engine frames. This adapter must do the same: CL_Frame pumps the
 * download before sending reliable commands, so completing synchronously
 * would combine "wwwdl ack", "wwwdl done", and the next "download" request
 * into one packet and can associate a repeated redirect with the next target.
 */

void HTProfile_newNoCacheClient(const char *applicationName, const char *applicationVersion)
{
    (void)applicationName;
    (void)applicationVersion;
    (void)coduomp_libwww_ensure_curl_initialized();
}

void HTAlertInit(void)
{
    libwwwProgressCallback = NULL;
    libwwwConfirmCallback = NULL;
    libwwwPromptCallback = NULL;
}

void HTAlert_setInteractive(HTBool interactive)
{
    (void)interactive;
}

void HTPrint_setCallback(HTPrintCallback *callback)
{
    libwwwPrintCallback = callback;
}

void HTTrace_setCallback(HTPrintCallback *callback)
{
    libwwwTraceCallback = callback;
}

HTBool HTNet_addAfter(HTNetAfter *callback, const char *urlTemplate, void *parameter, int32_t status, HTFilterOrder order)
{
    (void)urlTemplate;
    (void)order;
    libwwwAfterCallback = callback;
    libwwwAfterParameter = parameter;
    libwwwAfterStatus = status;
    return HT_TRUE;
}

HTBool HTAlert_add(HTAlertCallback *callback, HTAlertOpcode opcodeMask)
{
    if (opcodeMask == HT_A_PROGRESS)
        libwwwProgressCallback = callback;
    else if (opcodeMask == HT_A_CONFIRM)
        libwwwConfirmCallback = callback;
    else if (opcodeMask == HT_A_PROMPT_MASK)
        libwwwPromptCallback = callback;
    return HT_TRUE;
}

static char *Libwww_CopyRange(const char *start, size_t length)
{
    char *copy = malloc(length + 1);

    if (copy == NULL)
        return NULL;
    memcpy(copy, start, length);
    copy[length] = '\0';
    return copy;
}

char *HTParse(const char *address, const char *base, int32_t wanted)
{
    const char *schemeEnd;
    const char *authority;
    const char *path;

    (void)base;
    if (address == NULL)
        return Libwww_CopyRange("", 0);

    schemeEnd = strstr(address, "://");
    if (wanted == HT_PARSE_ACCESS) {
        if (schemeEnd == NULL)
            return Libwww_CopyRange("", 0);
        return Libwww_CopyRange(address, (size_t)(schemeEnd - address));
    }

    authority = schemeEnd != NULL ? schemeEnd + 3 : address;
    path = strchr(authority, '/');
    if (wanted == HT_PARSE_HOST) {
        const char *authorityEnd = path != NULL ? path : address + strlen(address);
        return Libwww_CopyRange(authority, (size_t)(authorityEnd - authority));
    }

    if ((wanted & HT_PARSE_PATH) != 0) {
        if (path == NULL)
            return Libwww_CopyRange("/", 1);
        return Libwww_CopyRange(path, strlen(path));
    }

    return Libwww_CopyRange(address, strlen(address));
}

HTRequest *HTRequest_new(void)
{
    return calloc(1, sizeof(HTRequest));
}

static int Libwww_HexDigit(uint8_t digit)
{
    if (digit >= '0' && digit <= '9')
        return digit - '0';
    if (digit >= 'a' && digit <= 'f')
        return digit - 'a' + 10;
    if (digit >= 'A' && digit <= 'F')
        return digit - 'A' + 10;
    return -1;
}

char *HTUnEscape(char *text)
{
    char *source = text;
    char *destination = text;

    if (text == NULL)
        return NULL;

    while (*source != '\0') {
        if (source[0] == '%' && Libwww_HexDigit((uint8_t)source[1]) >= 0 && Libwww_HexDigit((uint8_t)source[2]) >= 0) {
            *destination++ = (char)(Libwww_HexDigit((uint8_t)source[1]) * 16 + Libwww_HexDigit((uint8_t)source[2]));
            source += 3;
        } else {
            *destination++ = *source++;
        }
    }
    *destination = '\0';
    return text;
}

HTBasic *HTBasic_new(void)
{
    return calloc(1, sizeof(HTBasic));
}

HTBool HTSACopy(char **destination, const char *source)
{
    const char *text = source != NULL ? source : "";
    char *copy = Libwww_CopyRange(text, strlen(text));

    if (copy == NULL)
        return HT_FALSE;
    free(*destination);
    *destination = copy;
    return HT_TRUE;
}

void basic_credentials(HTRequest *request, HTBasic *credentials)
{
    if (request == NULL || credentials == NULL)
        return;
    (void)HTSACopy(&request->userName, credentials->uid);
    (void)HTSACopy(&request->password, credentials->pw);
}

HTBool HTBasic_delete(HTBasic *credentials)
{
    if (credentials == NULL)
        return HT_FALSE;
    free(credentials->uid);
    free(credentials->pw);
    free(credentials);
    return HT_TRUE;
}

void *HTMemory_malloc(size_t size)
{
    return malloc(size);
}

void HTMemory_free(void *memory)
{
    free(memory);
}

HTBool HTLoadToFile(const char *url, HTRequest *request, const char *localFileName)
{
    if (url == NULL || request == NULL || localFileName == NULL)
        return HT_FALSE;
    if (HTSACopy(&request->url, url) == HT_FALSE || HTSACopy(&request->targetPath, localFileName) == HT_FALSE) {
        return HT_FALSE;
    }
    return Libwww_BeginTransfer(request) == HT_OK ? HT_TRUE : HT_FALSE;
}

void HTProfile_delete(void)
{
    if (libwwwCurlInitialized != HT_FALSE) {
        curl_global_cleanup();
        libwwwCurlInitialized = HT_FALSE;
    }
}

void HTEventList_stopLoop(void)
{
    libwwwStopRequested = HT_TRUE;
}

void HTEventList_init(HTRequest *request)
{
    libwwwEventRequest = request;
    libwwwStopRequested = HT_FALSE;
}

static size_t Libwww_WriteFile(char *data, size_t elementSize, size_t elementCount, void *userData)
{
    return fwrite(data, elementSize, elementCount, (FILE *)userData) * elementSize;
}

static int Libwww_ReportProgress(void *clientData, curl_off_t downloadTotal, curl_off_t downloadNow, curl_off_t uploadTotal,
                                 curl_off_t uploadNow)
{
    HTRequest *request = clientData;

    (void)downloadTotal;
    (void)uploadTotal;
    (void)uploadNow;
    request->bytesRead = (long)downloadNow;
    request->net.bytesRead = request->bytesRead;
    if (libwwwProgressCallback != NULL) {
        (void)libwwwProgressCallback(request, 8, 0, NULL, NULL, NULL);
    }
    return libwwwStopRequested != HT_FALSE ? 1 : 0;
}

static void Libwww_CloseTransfer(HTRequest *request)
{
    if (request == NULL)
        return;

    if (request->multiHandle != NULL && request->easyHandle != NULL) {
        (void)curl_multi_remove_handle(request->multiHandle, request->easyHandle);
    }
    if (request->easyHandle != NULL) {
        curl_easy_cleanup(request->easyHandle);
        request->easyHandle = NULL;
    }
    if (request->multiHandle != NULL) {
        curl_multi_cleanup(request->multiHandle);
        request->multiHandle = NULL;
    }
    if (request->outputFile != NULL) {
        fclose(request->outputFile);
        request->outputFile = NULL;
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: a successful network result is not a successful
 * file download until buffered output has reached the host file API. */
static HTBool coduomp_libwww_finalize_output_file(HTRequest *request)
{
    int flushResult;
    int streamError;
    int closeResult;
    int savedError;

    if (request == NULL || request->outputFile == NULL)
        return HT_FALSE;

    errno = 0;
    flushResult = fflush(request->outputFile);
    streamError = ferror(request->outputFile);
    savedError = flushResult != 0 || streamError != 0 ? errno : 0;
    closeResult = fclose(request->outputFile);
    if (closeResult != 0 && savedError == 0)
        savedError = errno;
    request->outputFile = NULL;

    if (flushResult == 0 && streamError == 0 && closeResult == 0)
        return HT_TRUE;

    coduomp_libwww_print("HTTP download output failed: %s\n", savedError != 0 ? strerror(savedError) : "stream write failure");
    return HT_FALSE;
}

static int32_t Libwww_BeginTransfer(HTRequest *request)
{
    int32_t failureStatus = -(int32_t)CURLE_FAILED_INIT;
    CURLMcode multiResult;

#define CODUOMP_LIBWWW_SETOPT(option_, value_) \
    do { \
        const CURLcode optionResult_ = curl_easy_setopt(request->easyHandle, (option_), (value_)); \
        if (optionResult_ != CURLE_OK) { \
            coduomp_libwww_print("HTTP download setup failed for %s: %s\n", #option_, curl_easy_strerror(optionResult_)); \
            failureStatus = -(int32_t)optionResult_; \
            goto setup_failed; \
        } \
    } while (0)

    if (request == NULL || coduomp_libwww_ensure_curl_initialized() == HT_FALSE) {
        return failureStatus;
    }

    request->easyHandle = curl_easy_init();
    request->multiHandle = curl_multi_init();
    if (request->easyHandle == NULL || request->multiHandle == NULL) {
        coduomp_libwww_print("HTTP download setup failed: libcurl initialization failure\n");
        Libwww_CloseTransfer(request);
        return -(int32_t)CURLE_FAILED_INIT;
    }

    request->outputFile = fopen(request->targetPath, "wb");
    if (request->outputFile == NULL) {
        const int openError = errno;

        coduomp_libwww_print("HTTP download setup failed: %s\n", openError != 0 ? strerror(openError) : "file open failure");
        Libwww_CloseTransfer(request);
        return -(int32_t)CURLE_WRITE_ERROR;
    }

    request->errorBuffer[0] = '\0';
    CODUOMP_LIBWWW_SETOPT(CURLOPT_ERRORBUFFER, request->errorBuffer);
    CODUOMP_LIBWWW_SETOPT(CURLOPT_URL, request->url);
    CODUOMP_LIBWWW_SETOPT(CURLOPT_PROTOCOLS_STR, "http,https");
    CODUOMP_LIBWWW_SETOPT(CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
    CODUOMP_LIBWWW_SETOPT(CURLOPT_FOLLOWLOCATION, 1L);
    CODUOMP_LIBWWW_SETOPT(CURLOPT_FAILONERROR, 1L);
    CODUOMP_LIBWWW_SETOPT(CURLOPT_WRITEFUNCTION, Libwww_WriteFile);
    CODUOMP_LIBWWW_SETOPT(CURLOPT_WRITEDATA, request->outputFile);
    CODUOMP_LIBWWW_SETOPT(CURLOPT_NOPROGRESS, 0L);
    CODUOMP_LIBWWW_SETOPT(CURLOPT_XFERINFOFUNCTION, Libwww_ReportProgress);
    CODUOMP_LIBWWW_SETOPT(CURLOPT_XFERINFODATA, request);
    CODUOMP_LIBWWW_SETOPT(CURLOPT_USERAGENT, "ID_DOWNLOAD/1.0");
    CODUOMP_LIBWWW_SETOPT(CURLOPT_NOSIGNAL, 1L);
    if (libwwwTimeoutMilliseconds > 0) {
        const long inactivitySeconds =
            (long)(((int64_t)libwwwTimeoutMilliseconds + LIBWWW_MILLISECONDS_PER_SECOND - 1) / LIBWWW_MILLISECONDS_PER_SECOND);

        /* HTHost_setEventTimeout configures libwww's per-socket event timer,
         * which its event manager refreshes whenever activity occurs. A curl
         * whole-transfer deadline is not equivalent: it aborts a continuously
         * active download merely because the file takes more than 30 seconds.
         * Bound connection establishment separately and use curl's low-speed
         * timer as the maintained inactivity-timeout approximation. */
        CODUOMP_LIBWWW_SETOPT(CURLOPT_CONNECTTIMEOUT_MS, (long)libwwwTimeoutMilliseconds);
        CODUOMP_LIBWWW_SETOPT(CURLOPT_LOW_SPEED_LIMIT, (long)LIBWWW_MINIMUM_ACTIVITY_BYTES_PER_SECOND);
        CODUOMP_LIBWWW_SETOPT(CURLOPT_LOW_SPEED_TIME, inactivitySeconds);
    }
    if (request->userName != NULL) {
        CODUOMP_LIBWWW_SETOPT(CURLOPT_USERNAME, request->userName);
        CODUOMP_LIBWWW_SETOPT(CURLOPT_PASSWORD, request->password != NULL ? request->password : "");
    }

    multiResult = curl_multi_add_handle(request->multiHandle, request->easyHandle);
    if (multiResult != CURLM_OK) {
        coduomp_libwww_print("HTTP download setup failed: %s\n", curl_multi_strerror(multiResult));
        goto setup_failed;
    }
    return HT_OK;

setup_failed:
    Libwww_CloseTransfer(request);
    return failureStatus;

#undef CODUOMP_LIBWWW_SETOPT
}

static void Libwww_FinishTransfer(HTRequest *request, int32_t status)
{
    if (status >= 0 && coduomp_libwww_finalize_output_file(request) == HT_FALSE) {
        status = -(int32_t)CURLE_WRITE_ERROR;
    }
    Libwww_CloseTransfer(request);
    if (libwwwAfterCallback != NULL && (libwwwAfterStatus == HT_ALL || libwwwAfterStatus == status)) {
        (void)libwwwAfterCallback(request, NULL, libwwwAfterParameter, status);
    }
    libwwwEventRequest = NULL;
}

int32_t HTEventList_pump(void)
{
    HTRequest *const request = libwwwEventRequest;
    CURLMcode multiResult;
    int runningHandles = 0;
    int messageCount = 0;
    CURLMsg *message;

    if (request == NULL)
        return 0;

    if (request->multiHandle == NULL) {
        Libwww_FinishTransfer(request, -(int32_t)CURLE_FAILED_INIT);
        return 0;
    }

    multiResult = curl_multi_perform(request->multiHandle, &runningHandles);
    if (multiResult != CURLM_OK) {
        coduomp_libwww_print("HTTP download event failure: %s\n", curl_multi_strerror(multiResult));
        Libwww_FinishTransfer(request, -(int32_t)CURLE_RECV_ERROR);
        return 0;
    }

    while ((message = curl_multi_info_read(request->multiHandle, &messageCount)) != NULL) {
        if (message->msg != CURLMSG_DONE)
            continue;

        int32_t status;

        if (message->data.result == CURLE_OK) {
            long responseCode = 0;
            const CURLcode infoResult = curl_easy_getinfo(request->easyHandle, CURLINFO_RESPONSE_CODE, &responseCode);

            status = infoResult == CURLE_OK && responseCode >= 200 && responseCode < 300 ? HT_LOADED : -(int32_t)CURLE_HTTP_RETURNED_ERROR;
            if (status < 0) {
                coduomp_libwww_print("HTTP download failed: response status %ld\n", responseCode);
            }
        } else {
            status = -(int32_t)message->data.result;
            coduomp_libwww_print("HTTP download failed: %s\n",
                                 request->errorBuffer[0] != '\0' ? request->errorBuffer : curl_easy_strerror(message->data.result));
        }
        Libwww_FinishTransfer(request, status);
        return 0;
    }

    if (runningHandles != 0)
        return 1;

    Libwww_FinishTransfer(request, -(int32_t)CURLE_RECV_ERROR);
    return 0;
}

void HTEventList_unregisterAll(void)
{
    Libwww_CloseTransfer(libwwwEventRequest);
    libwwwEventRequest = NULL;
    libwwwStopRequested = HT_FALSE;
}

void HTHost_setEventTimeout(int32_t timeoutMilliseconds)
{
    libwwwTimeoutMilliseconds = timeoutMilliseconds;
}

void HTRequest_delete(HTRequest *request)
{
    if (request == NULL)
        return;
    Libwww_CloseTransfer(request);
    free(request->url);
    free(request->targetPath);
    free(request->userName);
    free(request->password);
    free(request);
}

long HTRequest_bytesRead(HTRequest *request)
{
    return request != NULL ? request->bytesRead : 0;
}

HTNet *HTRequest_net(HTRequest *request)
{
    return request != NULL ? &request->net : NULL;
}

HTBool HTNet_rawBytesCount(HTNet *net)
{
    return net != NULL ? net->rawBytesCount : HT_FALSE;
}

void HTFTP_setRawBytesCount(HTRequest *request)
{
    if (request != NULL)
        request->net.rawBytesCount = HT_TRUE;
}

long HTFTP_getDNetRawBytesCount(HTRequest *request)
{
    return request != NULL ? request->net.bytesRead : 0;
}
