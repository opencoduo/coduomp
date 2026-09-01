#include "bg_pmove.h"

/*
 * Shared BG movement state.  The Windows cgame and game modules retain the
 * same three source objects at different image addresses; the Linux game
 * module exports them as pm, pml, and c_pmove (0x001f29d0, 0x001f23a0, and
 * 0x000ab1e0 respectively).  Each module still receives its own instance when
 * this source is compiled into that module.
 */
pmove_t *pm = NULL;
pml_t pml;
int32_t c_pmove;
