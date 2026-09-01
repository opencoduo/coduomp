#include "client_legacy_crt.h"

/*
 * NOT_FROM_ORIGINAL_SOURCE
 *
 * Reproduce the forward byte-copy loops emitted at the retail client call
 * sites.  In particular, source == destination performs the original pass
 * instead of invoking the overlap checks used by fortified host strcpy.
 */
char *coduo_client_crt_strcpy(char *destination, const char *source)
{
    char *const result = destination;
    char character;

    do {
        character = *source++;
        *destination++ = character;
    } while (character != '\0');

    return result;
}
