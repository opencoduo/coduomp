#include "ui_memory_config.h"

_Alignas(16)
/* Source: uo_ui_mp_x86.dll 0x40063560..0x4016355f. */
unsigned char memoryPool[UI_MEMORY_POOL_CAPACITY];

/* Source: uo_ui_mp_x86.dll 0x40061560..0x4006355f. */
stringDef_t *strHandle[UI_STRING_HASH_SIZE];

/* Source: uo_ui_mp_x86.dll 0x401635e0..0x401c35df. */
char strPool[UI_STRING_POOL_CAPACITY];

/* Source: uo_ui_mp_x86.dll 0x401c35e0 and 0x401c35e4. */
int32_t allocPoint;
qboolean outOfMemory;

/* Source: uo_ui_mp_x86.dll 0x401c46e8 and 0x401c46ec. */
int32_t strPoolIndex;
int32_t strHandleCount;

/* Source: uo_ui_mp_x86.dll data 0x400405b0..0x400405b3.
 * PE_RELOCATION_VALUES_VERIFIED: the pointer targets the NUL separator at
 * 0x400329b7, yielding the original shared empty string. */
const char *emptyStr = "";
