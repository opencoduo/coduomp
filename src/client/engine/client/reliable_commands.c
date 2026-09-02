#include "cgame.h"

#include "../platform/crt_boundary.h"
#include "../system_platform.h"

#include <string.h>

enum {
    CL_RELIABLE_COMMAND_MASK = CODUO_RELIABLE_COMMAND_COUNT - 1,
    CL_RELIABLE_COMMAND_MAX_CHANGED_LENGTH = CODUO_RELIABLE_COMMAND_CAPACITY - 2,
    CL_MONKEY_FRAME_MASK = 255
};

#define CL_CRT_RANDOM_UNIT 0.000030517578125f /* exact 1.0f / 32768.0f */
#define CL_MONKEY_PROBABILITY 0.10000000000000001

/* Source: CoDUOMP.exe 0x0040f930..0x0040f97d.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040f930_0040f97e.mcode.
 * Name: exact same-module Mac symbol CL_AddReliableCommand. */
void CL_AddReliableCommand(const char *command)
{
    if (clc.reliableSequence - clc.reliableAcknowledge > CODUO_RELIABLE_COMMAND_COUNT) {
        Com_Error(ERR_DROP, "EXE_ERR_CLIENT_CMD_OVERFLOW");
    }

    ++clc.reliableSequence;
    MSG_WriteReliableCommandToBuffer(command, clc.reliableCommands[clc.reliableSequence & CL_RELIABLE_COMMAND_MASK],
                                     CODUO_RELIABLE_COMMAND_CAPACITY);
}

/* Source: CoDUOMP.exe 0x0040f980..0x0040f9c6.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040f980_0040f9c7.mcode.
 * Name: exact same-module Mac symbol CL_MakeMonkeyDoLaundry.
 *
 * The original advances the MSVC CRT random-number state before changing the
 * command but discards the generated number. Preserve that observable state
 * change even though it does not select the command or the inserted byte. */
void CL_MakeMonkeyDoLaundry(void)
{
    (void)coduo_crt_rand();

    char *const command = clc.reliableCommands[clc.reliableSequence & CL_RELIABLE_COMMAND_MASK];
    size_t length = strlen(command);
    if (length >= CODUO_RELIABLE_COMMAND_CAPACITY - 1) {
        length = CL_RELIABLE_COMMAND_MAX_CHANGED_LENGTH;
    }

    command[length] = '\n';
    command[length + 1] = '\0';
}

/* Source: CoDUOMP.exe 0x0040f9d0..0x0040fa0a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040f9d0_0040fa0b.mcode.
 * Name: exact same-module Mac symbol CL_ChangeReliableCommand. The Windows
 * compiler retains this body and separately inlines it into CL_Frame at
 * 0x00413986. Only the low frame-count byte is tested, exactly expressing
 * the original once-per-256-frames gate. */
void CL_ChangeReliableCommand(void)
{
    if (sysCheckCrashOrRerun == qfalse || ((uint32_t)cls.frameCount & (uint32_t)CL_MONKEY_FRAME_MASK) != 0u) {
        return;
    }

    const double randomFraction = (double)((float)coduo_crt_rand() * CL_CRT_RANDOM_UNIT);
    if (randomFraction < CL_MONKEY_PROBABILITY)
        CL_MakeMonkeyDoLaundry();
}
