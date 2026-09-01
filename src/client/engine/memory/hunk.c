#include "qcommon/hunk.h"

/* Original contiguous hunk state at CoDUOMP.exe 0x0096786c..0x0096789b.
 * High allocation state precedes the logging fields at 0x00967884/0x00967888;
 * low allocation state follows them. Keeping that order is required for the
 * original i386 layout; native size_t and pointer fields widen naturally on
 * 64-bit hosts. */
hunk_state_t hunk;

/* Original aligned hunk base at 0x0389fd18 and unaligned allocation owner at
 * 0x0389fd1c. */
uint8_t *hunk_data;
uint8_t *hunk_allocData;
