#include <zlib.h>

#include "fs_private.h"

/* NOT_FROM_ORIGINAL_SOURCE:
 * Functional bridge for the statically linked zlib
 * inflateInit2_ entry point at VA 0x080d0d68. The recovered caller retains
 * the stock i386 z_stream size operand (56 bytes); this host boundary
 * deliberately substitutes sizeof(*stream), because the native zlib record
 * widens with host pointers while its behavior and contents remain the same.
 */
int32_t coduomp_zlib_inflate_init2(z_stream *stream,
                                   int32_t windowBits, const char *version,
                                   int32_t streamSize)
{
    (void)version;
    (void)streamSize;

    return inflateInit2_(stream, windowBits, ZLIB_VERSION,
                         (int)sizeof(*stream));
}

/* NOT_FROM_ORIGINAL_SOURCE:
 * Functional bridge for the statically linked zlib
 * inflate entry point at VA 0x080d0f2a.
 */
int32_t coduomp_zlib_inflate(z_stream *stream, int32_t flush)
{
    return inflate(stream, flush);
}

/* NOT_FROM_ORIGINAL_SOURCE:
 * Functional bridge for the statically linked zlib
 * inflateEnd entry point at VA 0x080d0cec.
 */
int32_t coduomp_zlib_inflate_end(z_stream *stream)
{
    return inflateEnd(stream);
}
