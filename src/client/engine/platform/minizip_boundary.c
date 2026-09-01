#include "compression_boundary.h"

#include <stddef.h>

/*
 * Minizip 1.3 exposes this operation as unzSetOffset. The original statically
 * linked CoDUOMP function at 0x004ba670 stores the saved central-directory
 * position and reparses that entry, which is the same public operation.
 */
extern int unzSetOffset(unzFile file, unsigned long position);

enum {
    CODUOMP_UNZ_OK = 0,
    CODUOMP_UNZ_PARAMERROR = -102
};

/*
 * Source: CoDUOMP.exe 0x004b9f30..0x004b9f75.
 * The original patched Minizip reopens the FILE backing an archive and copies
 * the 0x80-byte archive state so simultaneous game file handles do not share a
 * decompression cursor. FS_FOpenFileRead immediately restores the selected
 * central-directory position after this call, so opening a fresh modern
 * Minizip handle preserves the same observable contract without copying
 * private, host-width-dependent library state.
 */
unzFile unzReOpen(const char *path, unzFile source)
{
    if (path == NULL || source == NULL)
        return NULL;
    return unzOpen(path);
}

/*
 * Source: CoDUOMP.exe 0x004ba670..0x004ba6a9.
 * Preserve the original engine-facing name while routing the operation
 * through modern Minizip's public central-directory positioning API. Retail
 * reports only a null archive as an error; it reparses a nonnull position but
 * returns success even when that internal reparse fails.
 */
int unzSetCurrentFileInfoPosition(unzFile file, unsigned long position)
{
    if (file == NULL)
        return CODUOMP_UNZ_PARAMERROR;

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    return unzSetOffset(file, position);
}
