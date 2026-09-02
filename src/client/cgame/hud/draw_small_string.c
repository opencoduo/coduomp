#include "../client_recovered.h"
#include "../globals.h"

#include <stddef.h>
#include <string.h>

// Source: uo_cgame_mp_x86.dll 0x3001cff0..0x3001d06f
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001cff0_3001d06f.mcode
//
// CG_DrawSmallString — the small-font cgame 2D text emitter that issues the
// unresolved-service cgame trap 54 (CG_R_TEXT_PAINT) with a text/element draw frame.
// It is one member of the trap-54 emitter family (siblings at 0x30031940,
// 0x30031a00, 0x30031a90, all issuing cgame_syscall(54, ...) cleaned by
// ADD ESP,0x3c). The two callers (script_method_player_setplayerangles-guessed
// 0x3001b2b0 and 0x30018090) draw three of these per frame at evenly spaced
// vertical positions (y = 240.0f, 256.0f, 272.0f in the small caller), forwarding
// a per-line data pointer. The exact engine service behind trap 54 is NOT
// recovered (no cgame syscall-id table), so the function and the trap are named
// by role/id, matching the existing CG_R_TEXT_PAINT documentation in client_recovered.h.
//
// Naming: the .mcode header guess "GScr_NewClientHudElem" (a size 0x7f match) is
// REJECTED. GScr_NewClientHudElem is a server-side script builtin with signature
// `void GScr_NewClientHudElem(void)` (server_name_bank hudelem.c); this is a
// four-argument cgame-client 2D emitter that calls the cgame VM syscall pointer.
// The size match is meaningless (the contract forbids size-based naming).
//
// Signature (proven from both call sites, which build all args and clean the
// stack themselves — caller-cleaned/cdecl, no frame pointer, ESP-relative args):
//   x         ([E+0x04]) : forwarded verbatim as a 32-bit float dword
//                          float bit pattern, e.g. 8.0f); relayed opaquely.
//   yBase     ([E+0x08]) : the float that gets +16.0f-2.0f (= +14.0f) applied.
//   string    ([E+0x0C]) : text pointer forwarded verbatim (arg6 of the trap).
//   scale     ([E+0x10]) : copied verbatim into the local color's alpha word.
//
// Machine-code proof of the outgoing trap frame. Let E be the entry ESP. After
// SUB ESP,0x14 the code builds a 15-dword argument block (cleaned by ADD ESP,0x3c)
// and calls *0x30085e9c == cgame_syscall. Simulating the interleaved PUSH/MOV
// stores register-accurately yields, in ascending memory (= argument order):
//   arg0  = 54                (PUSH 0x36 ; the trap id)
//   arg1  = position          (MOV ECX,[E+0x04] ; forwarded dword)
//   arg2  = yBase + 16.0f - 2.0f  (FLD [E+0x08]; FADD 16.0f; FSUB 2.0f; FSTP)
//   arg3  = 5                 (PUSH 5)
//   arg4  = 1.0f/3.0f         (0x3eaaaaab, materialized as a stack temp then pushed)
//   arg5  = &color            (LEA to the local color[4] below; PUSH)
//   arg6  = data              (the caller dword at [E+0x0C], forwarded)
//   arg7  = 8.0f              (0x41000000, materialized as a stack temp then pushed)
//   arg8  = 0                 (PUSH 0)
//   arg9  = 0                 (PUSH 0)
// The remaining words above the outgoing arguments are local/scratch storage:
// three 1.0f color words and `scale` as color[3]. The 16.0f and 2.0f addends are
// the .rdata float constants at 0x3007bf00 and
// 0x3007bce4 (g_const_float_16 / g_const_float_two); written
// here in natural literal form. The color storage (arg11..13) is the local
// `color` array that arg5's pointer targets — one source-level object, not three
// extra arguments; the compiler placed it inside the outgoing frame. arg10 is a
// leftover scratch dword occupying an alignment slot in that same region.

void CG_DrawSmallString(float x, float yBase, const char *string, float scale)
{
    int32_t scaleBits = CG_FloatBits(scale);              /* 0x3001cff3 */
    long double yCarrier = (long double)yBase;            /* 0x3001cff7 */
    int32_t xBits = CG_FloatBits(x);                       /* 0x3001d001 */
    vec4_t color;

    yCarrier += 16.0L;
    yCarrier -= 2.0L;
    memcpy(&color[3], &scaleBits, sizeof(color[3]));
    float y = (float)yCarrier;
    color[0] = 1.0f;
    color[1] = 1.0f;
    color[2] = 1.0f;

    /* cgame_syscall(54, position, y, 5, 1/3, &color, data, 8.0f, 0, 0, flags).
     * All non-integer args are passed as their raw 32-bit dword bit patterns,
     * matching the VM trap ABI (int32_t command, ...). */
    cgame_syscall(CG_R_TEXT_PAINT, xBits, CG_FloatBits(y), 5, CG_FloatBits(1.0f / 3.0f), (intptr_t)color, (intptr_t)string,
                  CG_FloatBits(8.0f), 0, 0);
}
