// Source: uo_cgame_mp_x86.dll 0x3003df70..0x3003df7e

#include "client/cgame/client_recovered.h"

/* Add a polygon to the scene. The original wrapper forwards hShader (EDX),
 * numVerts (ECX) and verts (EAX) with syscall 0x40 (CG_R_ADDPOLYTOSCENE). */
void trap_R_AddPolyToScene(qhandle_t hShader, int32_t numVerts, const polyVert_t *verts)
{
    cgame_syscall(CG_R_ADDPOLYTOSCENE, hShader, numVerts, (intptr_t)verts);
}
