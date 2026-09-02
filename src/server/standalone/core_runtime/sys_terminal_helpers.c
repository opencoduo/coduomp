#include <unistd.h>

#include "core_runtime_private.h"

enum {
    SYS_STDOUT_FILE_DESCRIPTOR = 1,
    SYS_TERMINAL_SINGLE_BYTE = 1,
    SYS_TERMINAL_SUPPRESSION_INACTIVE = 0,
    SYS_TERMINAL_EMPTY_CURSOR = 0,
    SYS_TERMINAL_ERASE_BACKSPACE = '\b',
    SYS_TERMINAL_ERASE_SPACE = ' '
};

void Sys_TTYDrainInput(void)
{
    char input;
    ssize_t readCount;

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    do {
        readCount = read(SYS_STDIN_FILE_DESCRIPTOR, &input, SYS_TERMINAL_SINGLE_BYTE);
    } while (readCount == SYS_TERMINAL_SINGLE_BYTE);
}

void Sys_TTYErasePreviousChar(void)
{
    char output;

    output = SYS_TERMINAL_ERASE_BACKSPACE;
    write(SYS_STDOUT_FILE_DESCRIPTOR, &output, SYS_TERMINAL_SINGLE_BYTE);
    output = SYS_TERMINAL_ERASE_SPACE;
    write(SYS_STDOUT_FILE_DESCRIPTOR, &output, SYS_TERMINAL_SINGLE_BYTE);
    output = SYS_TERMINAL_ERASE_BACKSPACE;
    write(SYS_STDOUT_FILE_DESCRIPTOR, &output, SYS_TERMINAL_SINGLE_BYTE);
}

void Sys_TTYHideInputLine(void)
{
    int index;

    if (sys_ttyOutputSuppressionDepth != SYS_TERMINAL_SUPPRESSION_INACTIVE) {
        sys_ttyOutputSuppressionDepth++;
        return;
    }

    if (sys_ttyCurrentLine.cursor > SYS_TERMINAL_EMPTY_CURSOR) {
        for (index = 0; index < sys_ttyCurrentLine.cursor; index++) {
            Sys_TTYErasePreviousChar();
        }
    }

    sys_ttyOutputSuppressionDepth++;
}

void Sys_TTYShowInputLine(void)
{
    int index;

    sys_ttyOutputSuppressionDepth--;
    if (sys_ttyOutputSuppressionDepth == SYS_TERMINAL_SUPPRESSION_INACTIVE && sys_ttyCurrentLine.cursor != SYS_TERMINAL_EMPTY_CURSOR) {
        for (index = 0; index < sys_ttyCurrentLine.cursor; index++) {
            write(SYS_STDOUT_FILE_DESCRIPTOR, &sys_ttyCurrentLine.buffer[index], SYS_TERMINAL_SINGLE_BYTE);
        }
    }
}
