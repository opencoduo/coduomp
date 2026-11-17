// Source: uo_cgame_mp_x86.dll 0x3003ebc0..0x3003ebcf

#include "client/cgame/client_recovered.h"

/* Return the entity's DObj, or NULL when it has none. The original wrapper
 * pushes the handle (EAX) and syscall 0xa5 (CG_DOBJ_GET_HANDLE). */
struct DObj_s *trap_GetDObj(int32_t handle)
{
    return (struct DObj_s *)(uintptr_t)cgame_syscall(CG_DOBJ_GET_HANDLE, handle);
}
