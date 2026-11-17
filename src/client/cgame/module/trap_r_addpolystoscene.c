// Source: uo_cgame_mp_x86.dll 0x3003df80..0x3003df93

#include "client/cgame/client_recovered.h"

/* Add several polygons that share a shader to the scene. The original wrapper
 * forwards hShader (stack), numVerts (EDX), verts (ECX) and numPolys (EAX)
 * with syscall 0x41 (CG_R_ADD_POLYS_TO_SCENE). */
void trap_R_AddPolysToScene(qhandle_t hShader, int32_t numVerts, const polyVert_t *verts, int32_t numPolys)
{
    cgame_syscall(CG_R_ADD_POLYS_TO_SCENE, hShader, numVerts, (intptr_t)verts, numPolys);
}
