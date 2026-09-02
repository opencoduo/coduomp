// Source: uo_cgame_mp_x86.dll 0x3000fd90..0x3000fdd2
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3000fd90_3000fdd2.mcode
//
// CG_CopyString — duplicate a NUL-terminated string into an engine-allocated
// buffer, returning the empty-string singleton when the source is empty.
//
// The .mcode header's assigned name `BG_GetAnimString` is REJECTED. It is a pure
// size guess (`win size 0x42, matched size 0x42`); AGENTS.md forbids identifying a
// function by size. The real BG_GetAnimString (server_name_bank.txt) is
// `char *BG_GetAnimString(int unusedClientNum, uint32_t animationIndex)` — a
// table-indexing lookup with a completely different shape (two int args, indexes an
// animation-name table). This function takes a string pointer, measures its length,
// allocates via the engine allocator, and byte-copies the string. It is the general
// CopyString helper, not a BG anim-table accessor. The mechanical
// owner=bg_getanimstring label on cg_emptyString (0x300a7e34) is likewise the
// first-touching-function guess and is already annotated as such in globals.h.
//
// Role proven by behavior:
//   - empty-source fast path returns cg_emptyString (0x300a7e34), the cached ""
//     pointer that globals.h already documents the CopyString helpers returning;
//   - otherwise strlen(src), then cgame_syscall(CG_HUNK_ALLOC_LOW_ALIGN_EXPLICIT /*208*/, len+1, 1)
//     — the exact CopyString allocation idiom documented at CG_HUNK_ALLOC_LOW_ALIGN_EXPLICIT in
//     client_recovered.h (which names 0x3000fd90 among the CopyString helpers);
//   - then a byte-by-byte strcpy of src (including the NUL) into the buffer.
//
// Calling convention (register in-args, out-param, no stack args at entry):
//   EDI = src (source string), EBX = out (pointer to a char* result slot).
// The result string pointer is stored through *out (MOV [EBX],EAX at both exits)
// and also returned in EAX. ESI is callee-saved (pushed 0x3000fda9 / popped
// 0x3000fdce) only around the allocating branch. Plain RET, no callee stack cleanup;
// the internal cgame_syscall call is cdecl (ADD ESP,0xc cleans its three dwords).

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

char *CG_CopyString(const char *src, char **out)
{
    /* 0x3000fd90: CMP byte ptr [EDI],0 / JNE — empty-source fast path. */
    if (*src == '\0') {
        /* 0x3000fd95: EAX = cg_emptyString; 0x3000fd9a: *out = EAX; RET. */
        *out = (char *)cg_emptyString;
        return (char *)cg_emptyString;
    }

    /* 0x3000fd9d..fda7: strlen(src).
     * EAX walks from src; EDX = src+1; loop reads CL=*EAX, INC EAX, until the byte
     * is 0. Then EAX - EDX = (ptr past NUL) - (src+1) = strlen; INC EAX -> strlen+1
     * (space for the terminator). */
    const char *p = src;
    while (*p != '\0') {
        ++p;
    }
    /* (p - src) == strlen; +1 for the NUL. Length math is pointer difference,
     * matching SUB EAX,EDX then INC EAX (unsigned byte-count arithmetic). */
    uint32_t allocLen = (uint32_t)(p - src) + 1u;

    /* 0x3000fdb0..fdb5: cgame_syscall(CG_HUNK_ALLOC_LOW_ALIGN_EXPLICIT, len+1, 1); the trap id is
     * 0xd0 (208). Args pushed 1, then len+1, then 0xd0; ADD ESP,0xc cleans three
     * dwords (cdecl). */
    char *buf = (char *)(intptr_t)cgame_syscall(CG_HUNK_ALLOC_LOW_ALIGN_EXPLICIT, (int32_t)allocLen, 1);

    /* 0x3000fdc0..fdcc: strcpy(buf, src), copying the terminating NUL too.
     * The machine code keeps ESI = buf - src and stores through ESI+ECX (ECX walks
     * src), which reconstructs the destination cursor; expressed here as a plain
     * source-cursor / dest-cursor copy. */
    const char *s = src;
    char *d = buf;
    char c;
    do {
        c = *s;
        *d = c;
        ++s;
        ++d;
    } while (c != '\0');

    /* 0x3000fdcf: *out = buf (EAX still holds the raw allocator result); RET. */
    *out = buf;
    return buf;
}
