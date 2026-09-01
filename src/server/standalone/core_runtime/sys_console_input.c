#include <string.h>
#if defined(_WIN32)
#include <conio.h>
#include <stdio.h>
#else
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>
#endif

#include "core_runtime_private.h"

enum {
    SYS_STDOUT_FILE_DESCRIPTOR = 1,
    SYS_CONSOLE_INPUT_SINGLE_BYTE = 1,
    SYS_TTY_ESCAPE_INTRO_CSI = '[',
    SYS_TTY_ESCAPE_INTRO_SS3 = 'O',
    SYS_TTY_ESCAPE_UP = 'A',
    SYS_TTY_ESCAPE_DOWN = 'B',
    SYS_TTY_ESCAPE_RIGHT = 'C',
    SYS_TTY_ESCAPE_LEFT = 'D',
    SYS_TTY_ASCII_NUL = '\0',
    SYS_TTY_ASCII_BACKSPACE = '\b',
    SYS_TTY_ASCII_TAB = '\t',
    SYS_TTY_ASCII_NEWLINE = '\n',
    SYS_TTY_ASCII_DELETE = 0x7f,
    SYS_TTY_CONTROL_MAX = 0x1f,
    SYS_TTY_COMMAND_PREFIX = '\\',
    SYS_READ_EOF = 0,
    SYS_SELECT_ERROR = -1,
    SYS_SELECT_STDIN_NFDS = 1
};

char *Sys_ConsoleInput(void)
{
#if defined(_WIN32)
    /*
     * NOT_FROM_ORIGINAL_SOURCE: MinGW has no POSIX nonblocking tty read;
     * retain the recovered line/history buffers while polling the Win32
     * console through the C runtime.
     */
    if (dedicated == NULL || dedicated->value == 0.0f ||
        sys_stdinActive == SYS_STDIN_INACTIVE || _kbhit() == 0) {
        return NULL;
    }

    int input = _getch();
    if (input == 0 || input == 0xe0) {
        int extendedInput = _getch();
        console_input_field_t *historyLine = NULL;

        if (extendedInput == 72) {
            historyLine = Sys_TTYPreviousHistoryLine();
        } else if (extendedInput == 80) {
            historyLine = Sys_TTYNextHistoryLine();
        }

        if (historyLine != NULL) {
            Sys_TTYHideInputLine();
            sys_ttyCurrentLine = *historyLine;
            Sys_TTYShowInputLine();
        }
        return NULL;
    }

    if (input == '\r' || input == SYS_TTY_ASCII_NEWLINE) {
        Sys_TTYStoreHistoryLine(&sys_ttyCurrentLine);
        strcpy(sys_ttyInputReturnBuffer, sys_ttyCurrentLine.buffer);
        Sys_TTYResetLine(&sys_ttyCurrentLine);
        fputc(SYS_TTY_ASCII_NEWLINE, stdout);
        fflush(stdout);
        return sys_ttyInputReturnBuffer;
    }

    if (input == sys_ttyEraseChar || input == SYS_TTY_ASCII_DELETE ||
        input == SYS_TTY_ASCII_BACKSPACE) {
        if (sys_ttyCurrentLine.cursor > 0) {
            sys_ttyCurrentLine.cursor--;
            sys_ttyCurrentLine.buffer[sys_ttyCurrentLine.cursor] =
                SYS_TTY_ASCII_NUL;
            Sys_TTYErasePreviousChar();
        }
        return NULL;
    }

    if (input == SYS_TTY_ASCII_TAB) {
        Sys_TTYHideInputLine();
        Sys_TTYCompleteLine(&sys_ttyCurrentLine);
        sys_ttyCurrentLine.cursor = strlen(sys_ttyCurrentLine.buffer);
        Sys_TTYShowInputLine();
        return NULL;
    }

    if (input <= SYS_TTY_CONTROL_MAX ||
        sys_ttyCurrentLine.cursor >= CON_INPUT_BUFFER_SIZE - 1) {
        return NULL;
    }

    sys_ttyCurrentLine.buffer[sys_ttyCurrentLine.cursor] = (char)input;
    sys_ttyCurrentLine.cursor++;
    sys_ttyCurrentLine.buffer[sys_ttyCurrentLine.cursor] = SYS_TTY_ASCII_NUL;
    fputc(input, stdout);
    fflush(stdout);
    return NULL;
#else
    char input;
    ssize_t readCount;

    input = SYS_TTY_ASCII_NUL;
    if (ttycon != NULL && ttycon->value != 0.0f) {
        readCount = read(SYS_STDIN_FILE_DESCRIPTOR, &input,
                         SYS_CONSOLE_INPUT_SINGLE_BYTE);
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        if (readCount == SYS_CONSOLE_INPUT_SINGLE_BYTE) {
            if (input == sys_ttyEraseChar ||
                input == SYS_TTY_ASCII_DELETE ||
                input == SYS_TTY_ASCII_BACKSPACE) {
                if (sys_ttyCurrentLine.cursor > 0) {
                    sys_ttyCurrentLine.cursor--;
                    sys_ttyCurrentLine.buffer[sys_ttyCurrentLine.cursor] =
                        SYS_TTY_ASCII_NUL;
                    Sys_TTYErasePreviousChar();
                }
                return NULL;
            }

            if (input != SYS_TTY_ASCII_NUL &&
                (signed char)input <= SYS_TTY_CONTROL_MAX) {
                if (input == SYS_TTY_ASCII_NEWLINE) {
                    Sys_TTYStoreHistoryLine(&sys_ttyCurrentLine);
                    strcpy(sys_ttyInputReturnBuffer, sys_ttyCurrentLine.buffer);
                    Sys_TTYResetLine(&sys_ttyCurrentLine);
                    input = SYS_TTY_ASCII_NEWLINE;
                    write(SYS_STDOUT_FILE_DESCRIPTOR, &input,
                          SYS_CONSOLE_INPUT_SINGLE_BYTE);
                    return sys_ttyInputReturnBuffer;
                }

                if (input == SYS_TTY_ASCII_TAB) {
                    Sys_TTYHideInputLine();
                    Sys_TTYCompleteLine(&sys_ttyCurrentLine);
                    sys_ttyCurrentLine.cursor =
                        strlen(sys_ttyCurrentLine.buffer);
                    if (sys_ttyCurrentLine.cursor > 0 &&
                        sys_ttyCurrentLine.buffer[0] == SYS_TTY_COMMAND_PREFIX) {
                        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
                        memmove(sys_ttyCurrentLine.buffer,
                                sys_ttyCurrentLine.buffer + 1,
                                (size_t)sys_ttyCurrentLine.cursor);
                        sys_ttyCurrentLine.cursor--;
                    }
                    Sys_TTYShowInputLine();
                    return NULL;
                }

                readCount = read(SYS_STDIN_FILE_DESCRIPTOR, &input,
                                 SYS_CONSOLE_INPUT_SINGLE_BYTE);
                if (readCount == SYS_CONSOLE_INPUT_SINGLE_BYTE &&
                    (input == SYS_TTY_ESCAPE_INTRO_CSI ||
                     input == SYS_TTY_ESCAPE_INTRO_SS3)) {
                    readCount = read(SYS_STDIN_FILE_DESCRIPTOR, &input,
                                     SYS_CONSOLE_INPUT_SINGLE_BYTE);
                    if (readCount == SYS_CONSOLE_INPUT_SINGLE_BYTE) {
                        if (input == SYS_TTY_ESCAPE_DOWN) {
                            console_input_field_t *historyLine;

                            historyLine = Sys_TTYNextHistoryLine();
                            Sys_TTYHideInputLine();
                            if (historyLine == NULL) {
                                Sys_TTYResetLine(&sys_ttyCurrentLine);
                            } else {
                                sys_ttyCurrentLine = *historyLine;
                            }
                            Sys_TTYShowInputLine();
                            Sys_TTYDrainInput();
                            return NULL;
                        }

                        if (input < SYS_TTY_ESCAPE_RIGHT) {
                            if (input == SYS_TTY_ESCAPE_UP) {
                                console_input_field_t *historyLine;

                                historyLine = Sys_TTYPreviousHistoryLine();
                                if (historyLine != NULL) {
                                    Sys_TTYHideInputLine();
                                    sys_ttyCurrentLine = *historyLine;
                                    Sys_TTYShowInputLine();
                                }
                                Sys_TTYDrainInput();
                                return NULL;
                            }
                        } else {
                            if (input == SYS_TTY_ESCAPE_RIGHT) {
                                return NULL;
                            }
                            if (input == SYS_TTY_ESCAPE_LEFT) {
                                return NULL;
                            }
                        }
                    }
                }
                Com_DPrintf("droping ISCTL sequence: %d, tty_erase: %d\n",
                            (int)input, sys_ttyEraseChar);
                Sys_TTYDrainInput();
                return NULL;
            }

            /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
            if (sys_ttyCurrentLine.cursor < 0 ||
                sys_ttyCurrentLine.cursor >= CON_INPUT_BUFFER_SIZE - 1) {
                return NULL;
            }
            sys_ttyCurrentLine.buffer[sys_ttyCurrentLine.cursor] = input;
            sys_ttyCurrentLine.cursor++;
            sys_ttyCurrentLine.buffer[sys_ttyCurrentLine.cursor] =
                SYS_TTY_ASCII_NUL;
            write(SYS_STDOUT_FILE_DESCRIPTOR, &input,
                  SYS_CONSOLE_INPUT_SINGLE_BYTE);
        }
        return NULL;
    }

    if (dedicated == NULL || dedicated->value == 0.0f) {
        return NULL;
    }
    if (sys_stdinActive == SYS_STDIN_INACTIVE) {
        return NULL;
    }

    {
        fd_set stdinSet;
        struct timeval timeout;
        int selectResult;

        FD_ZERO(&stdinSet);
        FD_SET(SYS_STDIN_FILE_DESCRIPTOR, &stdinSet);
        timeout.tv_sec = 0;
        timeout.tv_usec = 0;
        selectResult = select(SYS_SELECT_STDIN_NFDS, &stdinSet, NULL, NULL,
                              &timeout);
        if (selectResult != SYS_SELECT_ERROR &&
            FD_ISSET(SYS_STDIN_FILE_DESCRIPTOR, &stdinSet)) {
            readCount = read(SYS_STDIN_FILE_DESCRIPTOR,
                             sys_ttyInputReturnBuffer,
                             SYS_TTY_INPUT_RETURN_BUFFER_SIZE);
            if (readCount == SYS_READ_EOF) {
                sys_stdinActive = SYS_STDIN_INACTIVE;
                return NULL;
            }
            if (readCount < 1) {
                return NULL;
            }
            /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
            sys_ttyInputReturnBuffer[readCount - 1] = SYS_TTY_ASCII_NUL;
            return sys_ttyInputReturnBuffer;
        }
    }

    return NULL;
#endif
}
