#include "console.h"

#include "cgame.h"

/* Exact float stored at CoDUOMP.exe 0x005b9b44 (0x3a83126f), representing
 * the milliseconds-to-seconds scale 0.001f. */
#define CON_MSEC_TO_SECONDS 0.0010000000474974513f

/* Source: CoDUOMP.exe 0x0040b340..0x0040b401.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040b340_0040b402.mcode.
 * Name and signature: exact same-module Mac symbol Con_RunConsole. */
void Con_RunConsole(void)
{
    con.finalFrac = (cls.keyCatchers & KEYCATCH_CONSOLE) != 0 ? 0.5f : 0.0f;

    if (con.finalFrac < con.displayFrac) {
        const float animationStep =
            (float)cls.realFrametime * scr_conspeed->value *
            CON_MSEC_TO_SECONDS;
        con.displayFrac -= animationStep;
        if (con.displayFrac < con.finalFrac)
            con.displayFrac = con.finalFrac;
    } else if (con.finalFrac > con.displayFrac) {
        const float animationStep =
            (float)cls.realFrametime * scr_conspeed->value *
            CON_MSEC_TO_SECONDS;
        con.displayFrac += animationStep;
        if (con.displayFrac > con.finalFrac)
            con.displayFrac = con.finalFrac;
    }
}

/* Source: CoDUOMP.exe 0x0040b2a0..0x0040b33d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040b2a0_0040b33e.mcode.
 * Name and signature: exact same-module Mac symbol Con_DrawConsole. */
void Con_DrawConsole(void)
{
    Con_CheckResize();

    if (cls.state == CA_DISCONNECTED) {
        if ((cls.keyCatchers & (KEYCATCH_UI | KEYCATCH_CGAME)) == 0) {
            Con_DrawSolidConsole(1.0f);
            return;
        }
    } else if (cls.state == CA_LOADING) {
        if (con_debug->integer != 0 &&
            (cls.keyCatchers & KEYCATCH_UI) == 0) {
            Con_DrawSolidConsole(1.0f);
            return;
        }
    } else if (cls.state == CA_ACTIVE) {
        if (con.displayFrac == 0.0f)
            return;
        if (con_debug->integer == 2) {
            Con_DrawSolidConsole(con.displayFrac * 2.0f);
            return;
        }
    }

    if (con.displayFrac != 0.0f)
        Con_DrawSolidConsole(con.displayFrac);
}
