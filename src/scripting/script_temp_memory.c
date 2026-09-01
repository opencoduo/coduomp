#include "script_runtime_host.h"
#include "script_temp_memory.h"

/* Sources: CoDUOMP.exe 0x00482ed0..0x00482edb and coduo_lnxded
 * 0x080a5448..0x080a5456. */
void TempMemoryReset(void)
{
    script_codeTempSize = 0;
}

/* Sources: CoDUOMP.exe 0x00482ee0..0x00482eff and coduo_lnxded
 * 0x080a5458..0x080a548a. */
uint8_t *TempMalloc(size_t size)
{
    size_t oldSize = script_codeTempSize;

    script_codeTempSize += size;
    return (uint8_t *)Hunk_ReallocateTempMemory(script_codeTempSize) + oldSize;
}

/* Sources: CoDUOMP.exe 0x00482f00..0x00482f2b and coduo_lnxded
 * 0x080a548c..0x080a54bd. */
void TempMemorySetPos(uint8_t *pos)
{
    uint8_t *end = TempMalloc(0);

    script_codeTempSize -= (size_t)(end - pos);
    Hunk_ReallocateTempMemory(script_codeTempSize);
}
