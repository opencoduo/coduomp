// Source: uo_cgame_mp_x86.dll 0x30029b70..0x30029bfc
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30029b70_30029bfc.mcode
//
// CG_ConsolidateHudElemText — expand the single "%s" in a HUD element's label
// string with its text string into a bounded output buffer, then adopt the
// expanded buffer as the displayed text and clear the label.
//
// The .mcode header name "PM_FootstepForSurface" is a SIZE GUESS (win size 0x8c
// matched size 0x8c) and is REJECTED: the body performs bounded string
// %s-substitution and two float-lane updates on a cgame HUD draw item; it
// does no player-movement footstep-surface classification and returns nothing.
// Producer 0x30029c00 references "hudelem string" / CG_ConfigString and the
// value formats "%i:%02i.%i" / "%g", establishing the HUD-element subsystem.
// The exact name is anchored by the same-module Mac traceback symbol order.
//
// Register ABI (custom, verified against the call site at 0x30029d41):
//   EDI = self (cgAlignedDrawItem *)   [passed as ESI copied to EDI]
//   EDX = maxlen (int)              [output buffer capacity]
//   [ESP+0xc] = out (char *)        [single pushed stack argument]
//   RET with caller cleanup of the one stack argument (`add esp,4` at caller).
// EBX/EBP/ESI are callee-saved (pushed/popped); their use below is local.

#include "client/cgame/client_recovered.h"

void CG_ConsolidateHudElemText(cgAlignedDrawItem *self, int maxlen, char *out)
{
    // ESI = maxlen; DEC ESI  -> limit = maxlen - 1 (the last writable index,
    // reserved for the terminating NUL). All copy loops run while dst < limit.
    int32_t limit = coduo_int32_from_bits((uint32_t)maxlen - 1u);

    int dst = 0;  // EAX — number of characters written to `out`
    int src = 0;  // ECX — read cursor into self->label

    // Phase 1 (0x30029b82): copy the format prefix up to a "%s" or end/limit.
    // Guard TEST ESI,ESI / JLE skips the loop entirely when limit <= 0.
    // Note the phase-1 store uses index `src` (MOV [ECX+EBP],DL), but in this
    // path `dst` and `src` advance in lockstep from 0, so out[src] == out[dst]
    // until a "%s" is found (which bumps src by 2 without writing).
    if (limit > 0) {
        for (;;) {
            const char *format = self->label;   // MOV EDX,[EDI+0x10]
            char c = format[src];                // MOV DL,[EDX+ECX]
            if (c == '\0') {                     // TEST DL,DL / JZ 0x30029ba7
                break;
            }
            if (c == '%' && format[src + 1] == 's') { // CMP DL,'%' / CMP [EBX+1],'s'
                src += 2;                        // ADD ECX,2 — skip "%s", stop copying
                break;
            }
            dst++;                               // INC EAX
            out[src] = c;                        // MOV [ECX+EBP],DL
            src++;                               // INC ECX
            if (dst >= limit) {                  // CMP EAX,ESI / JL 0x30029b82
                break;
            }
        }
    }

    // Phase 2 (0x30029bb0): copy the substitution argument for the "%s".
    // Loop condition CMP EAX,ESI / JGE 0x30029bda before entry and after each
    // char, so it runs only while dst < limit.
    if (dst < limit) {
        int a = 0;  // EBX — read cursor into self->text
        for (;;) {
            const char *arg = self->text;         // MOV EDX,[EDI+0x18]
            char c = arg[a];                     // MOV DL,[EBX+EDX]
            if (c == '\0') {                     // TEST DL,DL / JZ 0x30029bc3
                break;
            }
            out[dst] = c;                        // MOV [EAX+EBP],DL
            dst++;                               // INC EAX
            a++;                                 // INC EBX
            if (dst >= limit) {                  // CMP EAX,ESI / JL 0x30029bb0
                break;
            }
        }
    }

    // Phase 3 (0x30029bc7): copy the remaining format tail after the "%s".
    // `src` continues from where phase 1 left off (past the "%s").
    if (dst < limit) {                           // CMP EAX,ESI / JGE 0x30029bda
        for (;;) {
            const char *format = self->label;   // MOV EDX,[EDI+0x10]
            char c = format[src];                // MOV DL,[ECX+EDX]
            if (c == '\0') {                     // TEST DL,DL / JZ 0x30029bda
                break;
            }
            out[dst] = c;                        // MOV [EAX+EBP],DL
            dst++;                               // INC EAX
            src++;                               // INC ECX
            if (dst >= limit) {                  // CMP EAX,ESI / JL 0x30029bc7
                break;
            }
        }
    }

    out[dst] = '\0';                             // MOV byte [EAX+EBP],0

    // Preserve the exact float-lane operation and rebind the item to the
    // consolidated text.
    // FLD [EDI+0x1c] / FADD [EDI+0x14] / (store) / FSTP [EDI+0x1c] — the add is
    // evaluated before self->text / self->textWidth are written, matching the
    // instruction order (FADD reads +0x14 before the +0x14 store below).
    long double combinedWidth = (long double)self->textWidth + (long double)self->labelWidth; // FLD +0x1c; FADD +0x14
    self->text = out;              // MOV [EDI+0x18],EBP — expanded buffer becomes the arg
    self->textWidth = (float)combinedWidth;  // FSTP [EDI+0x1c]
    self->label = (char *)g_str_empty; // MOV [EDI+0x10],0x30074a0c; shared empty string
    self->labelWidthBits = 0;       // MOV dword [EDI+0x14],0 (raw float-zero bits)
}
