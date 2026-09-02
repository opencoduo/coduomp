#include <stddef.h>
#include <stdint.h>

/* NOT_FROM_ORIGINAL_SOURCE: disabled PunkBuster bridge stubs for default builds. */
void PB_CallServerSbGlobal(int32_t opcode, int32_t clientNum, uint32_t length,
                           const char *text)
{
    (void)opcode;
    (void)clientNum;
    (void)length;
    (void)text;
}

/* NOT_FROM_ORIGINAL_SOURCE: disabled PunkBuster bridge stub for default builds. */
void PB_StartServer(void)
{
}

/* NOT_FROM_ORIGINAL_SOURCE: disabled PunkBuster bridge stub for default builds. */
void PB_RunServerFrame(void)
{
}

/* NOT_FROM_ORIGINAL_SOURCE: disabled PunkBuster bridge stub for default builds. */
void PB_CallServerSaCommandDrain(void)
{
}

/* NOT_FROM_ORIGINAL_SOURCE: disabled PunkBuster bridge stub for default builds. */
void PB_InvokeEventCallback(const char *text, const uint8_t *packetData)
{
    (void)text;
    (void)packetData;
}

/* NOT_FROM_ORIGINAL_SOURCE: disabled PunkBuster bridge stub for default builds. */
const char *PB_InvokeStringQueryCallback(const char *text, intptr_t arg1,
                                         intptr_t arg2)
{
    (void)text;
    (void)arg1;
    (void)arg2;
    return NULL;
}

/* NOT_FROM_ORIGINAL_SOURCE: disabled PunkBuster bridge stub for default builds. */
void PB_NotifyServerEnabled(void)
{
}

/* NOT_FROM_ORIGINAL_SOURCE: disabled PunkBuster bridge stub for default builds. */
void PB_NotifyServerDisabled(void)
{
}

/* NOT_FROM_ORIGINAL_SOURCE: disabled PunkBuster bridge stub for default builds. */
void PB_Print(const char *text, int32_t textLimit)
{
    (void)text;
    (void)textLimit;
}
