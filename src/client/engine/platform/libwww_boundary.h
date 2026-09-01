#ifndef CODUOMP_LIBWWW_BOUNDARY_H
#define CODUOMP_LIBWWW_BOUNDARY_H

#include "../q_shared.h"

#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CoDUOMP statically links the W3C libwww client. These opaque declarations
 * describe only the library surface reached by maintained engine source.
 * Modern targets can satisfy the same boundary with a compatibility layer
 * without recovering libwww's internal implementation into the engine.
 */
typedef struct _HTAlertPar HTAlertPar;
typedef struct _HTNet HTNet;
typedef struct _HTRequest HTRequest;
typedef struct _HTResponse HTResponse;

typedef int32_t HTBool;
typedef int32_t HTAlertOpcode;
typedef int32_t HTFilterOrder;

/* Public libwww credential carrier. HTBasic_new in the embedded Windows
 * library allocates the complete four-field object, initializes retry, and
 * DL_BeginDownload populates uid and pw before passing it to
 * basic_credentials. */
typedef struct _HTBasic {
    char *uid;
    char *pw;
    HTBool retry;
    HTBool proxy;
} HTBasic;

typedef int32_t HTAlertCallback(
    HTRequest *request, HTAlertOpcode opcode, int32_t messageNumber,
    const char *defaultMessage, void *input, HTAlertPar *reply);
typedef int32_t HTNetAfter(
    HTRequest *request, HTResponse *response, void *parameter,
    int32_t status);
typedef int32_t HTPrintCallback(const char *format, va_list arguments);

enum {
    HT_FALSE = 0,
    HT_TRUE = 1,
    HT_OK = 0,
    HT_ALL = 1,
    HT_LOADED = 200,
    HT_FILTER_LAST = 65535,
    HT_A_PROGRESS = 65535,
    HT_A_CONFIRM = 131072,
    /* Combined prompt, password, and username/password alert classes. */
    HT_A_PROMPT_MASK = 1835008,
    HT_PARSE_PUNCTUATION = 1,
    HT_PARSE_PATH = 4,
    HT_PARSE_HOST = 8,
    HT_PARSE_ACCESS = 16
};

void HTProfile_newNoCacheClient(
    const char *applicationName, const char *applicationVersion);
void HTAlertInit(void);
void HTAlert_setInteractive(HTBool interactive);
void HTPrint_setCallback(HTPrintCallback *callback);
void HTTrace_setCallback(HTPrintCallback *callback);
HTBool HTNet_addAfter(
    HTNetAfter *callback, const char *urlTemplate, void *parameter,
    int32_t status, HTFilterOrder order);
HTBool HTAlert_add(
    HTAlertCallback *callback, HTAlertOpcode opcodeMask);

char *HTParse(const char *address, const char *base, int32_t wanted);
HTRequest *HTRequest_new(void);
char *HTUnEscape(char *text);
HTBasic *HTBasic_new(void);
HTBool HTSACopy(char **destination, const char *source);
void basic_credentials(HTRequest *request, HTBasic *credentials);
HTBool HTBasic_delete(HTBasic *credentials);
void *HTMemory_malloc(size_t size);
void HTMemory_free(void *memory);
HTBool HTLoadToFile(
    const char *url, HTRequest *request, const char *localFileName);
void HTProfile_delete(void);

void HTEventList_stopLoop(void);
void HTEventList_init(HTRequest *request);
int32_t HTEventList_pump(void);
void HTEventList_unregisterAll(void);
void HTHost_setEventTimeout(int32_t timeoutMilliseconds);
void HTRequest_delete(HTRequest *request);
long HTRequest_bytesRead(HTRequest *request);
HTNet *HTRequest_net(HTRequest *request);
HTBool HTNet_rawBytesCount(HTNet *net);
void HTFTP_setRawBytesCount(HTRequest *request);
long HTFTP_getDNetRawBytesCount(HTRequest *request);

#ifdef __cplusplus
}
#endif

#endif
