#include "com_redirect.h"

#include <stddef.h>

/*
 * Complete common print-redirection state and lifecycle.  The original bodies
 * agree on both authoritative engine targets:
 *
 * Function            Windows       Linux
 * Com_BeginRedirect   0x00439880    0x0806fde8
 * Com_EndRedirect     0x004398b0    0x0806fe21
 *
 * The buffer, size, and flush state reside at Windows 0x00982790,
 * 0x00980228, and 0x00980678, and Linux 0x08253504, 0x08253508, and
 * 0x0825350c, respectively.  Both bodies ignore an incomplete redirect triple.
 *
 * The supporting Mac client also exports both canonical names.  The state is
 * installed around remote-console execution so ordinary common prints can be
 * returned to the requesting address.  Console output and Com_Printf remain
 * target-owned consumers of this common state.
 */
char *com_redirectBuffer;
int32_t com_redirectBufferSize;
com_redirect_flush_t com_redirectFlush;

void Com_BeginRedirect(char *buffer, int32_t bufferSize,
                       com_redirect_flush_t flush)
{
    if (buffer == NULL || bufferSize == 0 || flush == NULL) {
        return;
    }

    com_redirectBuffer = buffer;
    com_redirectBufferSize = bufferSize;
    com_redirectFlush = flush;
    com_redirectBuffer[0] = '\0';
}

void Com_EndRedirect(void)
{
    if (com_redirectFlush != NULL) {
        com_redirectFlush(com_redirectBuffer);
    }

    com_redirectBuffer = NULL;
    com_redirectBufferSize = 0;
    com_redirectFlush = NULL;
}
