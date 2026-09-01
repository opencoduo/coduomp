#define _POSIX_C_SOURCE 200809L

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "script_runtime_host.h"

#include "script_error_reporting.h"
#include "script_yy_runtime.h"
#include "script_yy_tokens.h"

/* Recovered generated Flex scanner runtime and game-specific lexer actions.
 * Primary Windows evidence:
 *   yylex                 0x00491fa0..0x00492e96
 *   yy_get_next_buffer    0x00493020..0x004931ba
 *   yy_get_previous_state 0x004931c0..0x00493282
 *   yy_try_NUL_trans      0x00493290..0x00493327
 *   yyrestart             0x00493330..0x004933b7
 *   yy_switch_to_buffer   0x004933c0..0x00493420
 *   yy_load_buffer_state  0x00493430..0x0049345c
 *   yy_create_buffer      0x00493460..0x004934df
 *   yy_delete_buffer      0x004934e0..0x00493510
 *   yy_init_buffer        0x00493520..0x00493555
 *   yy_flush_buffer       0x00493560..0x004935b3
 *   yy_scan_buffer        0x004935c0..0x0049362b
 *   yy_scan_string        0x00493630..0x00493653
 *   yy_scan_bytes         0x00493660..0x004936c5
 *   yy_fatal_error        0x004936d0..0x004936e9
 *   yy_flex_alloc         0x004936f0..0x004936f9
 *   yy_flex_realloc       0x00493700..0x0049370a
 *   yy_flex_free          0x00493710..0x00493717
 *   yyerror               0x00493720..0x0049375c
 * The generated DFA tables remain external data objects. */

enum {
    SCRIPT_YY_CREATE_BUFFER_SIZE = 0x4000,
    SCRIPT_YY_READ_BUFFER_SIZE = 0x2000,
    SCRIPT_YY_BUFFER_TRAILING_NUL_COUNT = 2,
    SCRIPT_YY_NUL_TRANSITION_CHAR = 1,
    SCRIPT_YY_META_THRESHOLD_STATE = 0xf4,
    SCRIPT_YY_END_OF_BUFFER_STATE = 0xf4,
    SCRIPT_YY_JAM_BASE = 0x18b,
    SCRIPT_YY_EOF_TOKEN = 0,
    SCRIPT_YY_BAD_SYNTAX_TOKEN = 0x101,
    SCRIPT_YY_IDENTIFIER_TOKEN = 0x102,
    SCRIPT_YY_ESCAPED_STRING_TOKEN = 0x103,
    SCRIPT_YY_HASH_STRING_TOKEN = 0x104,
    SCRIPT_YY_INT_TOKEN = 0x11f,
    SCRIPT_YY_FLOAT_TOKEN = 0x120,
    SCRIPT_YY_ANIMTREE_IDENTIFIER_TOKEN = 0x148,

    SCRIPT_YY_BUFFER_NEW = 0,
    SCRIPT_YY_BUFFER_NORMAL = 1,
    SCRIPT_YY_BUFFER_EOF_PENDING = 2,

    SCRIPT_YY_EOB_ACT_CONTINUE_SCAN = 0,
    SCRIPT_YY_EOB_ACT_END_OF_FILE = 1,
    SCRIPT_YY_EOB_ACT_LAST_MATCH = 2
};

void yy_fatal_error(const char *message);
void *yy_flex_alloc(size_t size);
void *yy_flex_realloc(void *ptr, size_t size);
void yy_flex_free(void *ptr);
void yyrestart(FILE *inputFile);
void yy_load_buffer_state(void);
void yy_init_buffer(script_yy_buffer_t *buffer, FILE *inputFile);
void yy_flush_buffer(script_yy_buffer_t *buffer);
script_yy_buffer_t *yy_create_buffer(FILE *inputFile,
                                               int32_t size);
void yy_switch_to_buffer(script_yy_buffer_t *buffer);
script_yy_buffer_t *yy_scan_buffer(char *base, uint32_t size);
script_yy_buffer_t *yy_scan_bytes(const char *bytes,
                                            int32_t length);
int32_t yy_get_next_buffer(void);
int32_t yy_get_previous_state(void);
int32_t yy_try_NUL_trans(int32_t state);
int32_t yywrap(void);

/* NOT_FROM_ORIGINAL_SOURCE: compact source representation of the original
 * scanner action switch, whose machine code returns these token constants
 * directly rather than indexing a data table. */
static const int32_t script_yyTokenByAction[] = {
    [0x09] = 0x105, [0x0a] = 0x106, [0x0b] = 0x107, [0x0c] = 0x108,
    [0x0d] = 0x109, [0x0e] = 0x10a, [0x0f] = 0x10b, [0x10] = 0x10c,
    [0x11] = 0x10d, [0x12] = 0x10e, [0x13] = 0x10f, [0x14] = 0x110,
    [0x15] = 0x111, [0x16] = 0x112, [0x17] = 0x113, [0x18] = 0x114,
    [0x19] = 0x115, [0x1a] = 0x116, [0x1b] = 0x117, [0x1c] = 0x118,
    [0x1d] = 0x119, [0x1e] = 0x11a, [0x1f] = 0x11b, [0x20] = 0x11c,
    [0x21] = 0x11d, [0x22] = 0x11e, [0x25] = 0x122, [0x26] = 0x121,
    [0x27] = 0x127, [0x28] = 0x123, [0x29] = 0x126, [0x2a] = 0x124,
    [0x2b] = 0x125, [0x2c] = 0x128, [0x2d] = 0x129, [0x2e] = 0x12a,
    [0x2f] = 0x12b, [0x30] = 0x12c, [0x31] = 0x12d, [0x32] = 0x12e,
    [0x33] = 0x12f, [0x34] = 0x130, [0x35] = 0x131, [0x36] = 0x132,
    [0x37] = 0x133, [0x38] = 0x134, [0x39] = 0x135, [0x3a] = 0x136,
    [0x3b] = 0x137, [0x3c] = 0x138, [0x3d] = 0x139, [0x3e] = 0x13a,
    [0x3f] = 0x13b, [0x40] = 0x13c, [0x41] = 0x13d, [0x42] = 0x13e,
    [0x43] = 0x13f, [0x44] = 0x140, [0x45] = 0x141, [0x46] = 0x142,
    [0x47] = 0x143, [0x48] = 0x144, [0x49] = 0x145, [0x4a] = 0x146,
    [0x4b] = 0x147, [0x4c] = 0x149, [0x4d] = 0x14a, [0x4e] = 0x14b,
    [0x4f] = 0x14c, [0x50] = 0x14d, [0x51] = 0x14e, [0x52] = 0x14f,
    [0x53] = 0x150, [0x54] = 0x151, [0x55] = 0x152, [0x56] = 0x153,
    [0x57] = 0x154, [0x58] = 0x155, [0x59] = 0x156,
};

/* NOT_FROM_ORIGINAL_SOURCE: shared spelling for generated scanner position updates. */
static void coduo_script_compat_note_token_source(void)
{
    script_yylval.source.sourcePos = script_yyCurrentSourcePos;
    script_yyPreviousSourcePos = script_yyCurrentSourcePos;
    script_yyCurrentSourcePos += (uint32_t)script_yyLength;
}

int32_t yylex(void)
{
    char *yyBp;
    char *yyCp;
    int32_t action;
    int32_t state;

    if (script_yyInit != qfalse) {
        script_yyInit = qfalse;
        if (script_yyStart == 0) {
            script_yyStart = 1;
        }
        if (script_yyInputFile == NULL) {
            script_yyInputFile = stdin;
        }
        if (script_yyOutputFile == NULL) {
            script_yyOutputFile = stdout;
        }
        if (script_yyCurrentBuffer == NULL) {
            script_yyCurrentBuffer =
                yy_create_buffer(script_yyInputFile,
                                     SCRIPT_YY_CREATE_BUFFER_SIZE);
        }
        yy_load_buffer_state();
    }

restart_scan:
    yyCp = script_yyCBufferPosition;
    *yyCp = (char)script_yyHoldChar;
    yyBp = yyCp;
    state = script_yyStart;

scan_state:
    do {
        uint8_t yyChar = (uint8_t)script_yyEc[(uint8_t)*yyCp];
        if (script_yyAccept[state] != 0) {
            script_yyLastAcceptingState = state;
            script_yyLastAcceptingCpos = yyCp;
        }

        while (script_yyChk[script_yyBase[state] + yyChar] != state) {
            state = script_yyDef[state];
            if (state > SCRIPT_YY_META_THRESHOLD_STATE) {
                yyChar = (uint8_t)script_yyMeta[yyChar];
            }
        }

        state = script_yyNxt[script_yyBase[state] + yyChar];
        yyCp++;
    } while (script_yyBase[state] != SCRIPT_YY_JAM_BASE);

accept_action:
    action = script_yyAccept[state];
    if (action == 0) {
        yyCp = script_yyLastAcceptingCpos;
        action = script_yyAccept[script_yyLastAcceptingState];
    }

    script_yyText = yyBp;
    script_yyLength = (int32_t)(yyCp - yyBp);
    script_yyHoldChar = (uint8_t)*yyCp;
    *yyCp = '\0';
    script_yyCBufferPosition = yyCp;

dispatch_action:
    if (action >= 1 && action <= 0x5d) {
        coduo_script_compat_note_token_source();
    }

    switch (action) {
        case 0:
            *yyCp = (char)script_yyHoldChar;
            yyCp = script_yyLastAcceptingCpos;
            state = script_yyLastAcceptingState;
            action = script_yyAccept[state];
            script_yyText = yyBp;
            script_yyLength = (int32_t)(yyCp - yyBp);
            script_yyHoldChar = (uint8_t)*yyCp;
            *yyCp = '\0';
            script_yyCBufferPosition = yyCp;
            goto dispatch_action;

        case 1:
        case 3:
        case 4:
        case 5:
            goto restart_scan;

        case 2:
            script_yyStart = 3;
            goto restart_scan;

        case 6:
            script_yyStart = 5;
            goto restart_scan;

        case 7:
            StringValue(script_yyText + 1, script_yyLength - 2);
            return SCRIPT_YY_ESCAPED_STRING_TOKEN;

        case 8:
            StringValue(script_yyText + 2, script_yyLength - 3);
            return SCRIPT_YY_HASH_STRING_TOKEN;

        case 0x23:
            IntegerValue(script_yyText);
            return SCRIPT_YY_INT_TOKEN;

        case 0x24:
            FloatValue(script_yyText);
            return SCRIPT_YY_FLOAT_TOKEN;

        case 0x5a:
            TextValue(script_yyText, script_yyLength);
            return SCRIPT_YY_IDENTIFIER_TOKEN;

        case 0x5b:
            TextValue(script_yyText, script_yyLength);
            return SCRIPT_YY_ANIMTREE_IDENTIFIER_TOKEN;

        case 0x5c:
            CompileError(script_yyPreviousSourcePos, "bad token '%s'",
                                  script_yyText);
            return SCRIPT_YY_BAD_SYNTAX_TOKEN;

        case 0x5d:
            fwrite(script_yyText, (size_t)script_yyLength, 1,
                   script_yyOutputFile);
            goto restart_scan;

        case 0x5e: {
            int32_t numberToMove = (int32_t)(yyCp - script_yyText) - 1;

            *yyCp = (char)script_yyHoldChar;
            if (script_yyCurrentBuffer->bufferStatus == SCRIPT_YY_BUFFER_NEW) {
                script_yyNChars = script_yyCurrentBuffer->nChars;
                script_yyCurrentBuffer->inputFile = script_yyInputFile;
                script_yyCurrentBuffer->bufferStatus = SCRIPT_YY_BUFFER_NORMAL;
            }

            if (script_yyCBufferPosition <=
                script_yyCurrentBuffer->chBuf + script_yyNChars) {
                script_yyCBufferPosition = script_yyText + numberToMove;
                state = yy_get_previous_state();
                int32_t nextState = yy_try_NUL_trans(state);
                yyBp = script_yyText;
                if (nextState == 0) {
                    yyCp = script_yyCBufferPosition;
                    goto accept_action;
                }
                script_yyCBufferPosition++;
                yyCp = script_yyCBufferPosition;
                state = nextState;
                goto scan_state;
            }

            switch (yy_get_next_buffer()) {
            case SCRIPT_YY_EOB_ACT_END_OF_FILE:
                script_yyDidBufferSwitchOnEof = qfalse;
                if (yywrap() != 0) {
                    script_yyCBufferPosition = script_yyText;
                    action = ((script_yyStart - 1) / 2) + 0x5f;
                    goto dispatch_action;
                }
                if (script_yyDidBufferSwitchOnEof == qfalse) {
                    yyrestart(script_yyInputFile);
                }
                goto restart_scan;

            case SCRIPT_YY_EOB_ACT_CONTINUE_SCAN:
                script_yyCBufferPosition = script_yyText + numberToMove;
                state = yy_get_previous_state();
                yyCp = script_yyCBufferPosition;
                yyBp = script_yyText;
                goto scan_state;

            case SCRIPT_YY_EOB_ACT_LAST_MATCH:
                script_yyCBufferPosition =
                    script_yyCurrentBuffer->chBuf +
                    script_yyNChars;
                state = yy_get_previous_state();
                yyCp = script_yyCBufferPosition;
                yyBp = script_yyText;
                goto accept_action;

            default:
                goto restart_scan;
            }
        }

        case 0x5f:
        case 0x60:
        case 0x61:
            return 0;

        default:
            if (action >= 0 &&
                action < (int32_t)(sizeof(script_yyTokenByAction) /
                                   sizeof(script_yyTokenByAction[0])) &&
                script_yyTokenByAction[action] != 0) {
                return script_yyTokenByAction[action];
            }
            yy_fatal_error(
                "fatal flex scanner internal error--no action found");
            goto restart_scan;
        }
}

int32_t yy_get_next_buffer(void)
{
    script_yy_buffer_t *buffer = script_yyCurrentBuffer;
    char *dest = buffer->chBuf;
    char *source = script_yyText;

    if (script_yyNChars + buffer->chBuf + 1 <
        script_yyCBufferPosition) {
        yy_fatal_error(
            "fatal flex scanner internal error--end of buffer missed");
    }

    if (buffer->fillBuffer == qfalse) {
        return script_yyCBufferPosition - script_yyText == 1
                   ? SCRIPT_YY_EOB_ACT_END_OF_FILE
                   : SCRIPT_YY_EOB_ACT_LAST_MATCH;
    }

    int32_t numberToMove =
        (int32_t)(script_yyCBufferPosition - script_yyText) - 1;
    for (int32_t index = 0; index < numberToMove; ++index) {
        dest[index] = source[index];
    }

    if (buffer->bufferStatus == SCRIPT_YY_BUFFER_EOF_PENDING) {
        script_yyNChars = 0;
        buffer->nChars = 0;
    } else {
        int32_t available = buffer->bufSize - numberToMove;
        while (--available < 1) {
            ptrdiff_t cbufOffset =
                script_yyCBufferPosition - buffer->chBuf;

            if (buffer->isOurBuffer == qfalse) {
                buffer->chBuf = NULL;
            } else {
                if (buffer->bufSize * 2 < 1) {
                    buffer->bufSize += (int32_t)((uint32_t)buffer->bufSize >> 3);
                } else {
                    buffer->bufSize *= 2;
                }

                uint32_t allocationSize =
                    (uint32_t)buffer->bufSize +
                    SCRIPT_YY_BUFFER_TRAILING_NUL_COUNT;
                buffer->chBuf = yy_flex_realloc(
                    buffer->chBuf, (size_t)allocationSize);
            }

            if (buffer->chBuf == NULL) {
                yy_fatal_error(
                    "fatal error - scanner input buffer overflow");
            }

            script_yyCBufferPosition = buffer->chBuf + cbufOffset;
            available = buffer->bufSize - numberToMove;
        }

        if (available > SCRIPT_YY_READ_BUFFER_SIZE) {
            available = SCRIPT_YY_READ_BUFFER_SIZE;
        }

        char lastChar = '*';
        int32_t readCount;
        for (readCount = 0; readCount < available; ++readCount) {
            lastChar = *script_yyInputCursor++;
            if (lastChar == '\0' || lastChar == '\n') {
                break;
            }
            buffer->chBuf[numberToMove + readCount] = lastChar;
        }

        if (lastChar == '\n') {
            buffer->chBuf[numberToMove + readCount] = '\n';
            ++readCount;
        } else if (lastChar == '\0') {
            --script_yyInputCursor;
        }

        script_yyNChars = readCount;
        buffer->nChars = readCount;
    }

    int32_t eobAction;
    if (script_yyNChars == 0) {
        if (numberToMove == 0) {
            eobAction = SCRIPT_YY_EOB_ACT_END_OF_FILE;
            yyrestart(script_yyInputFile);
        } else {
            eobAction = SCRIPT_YY_EOB_ACT_LAST_MATCH;
            buffer->bufferStatus = SCRIPT_YY_BUFFER_EOF_PENDING;
        }
    } else {
        eobAction = SCRIPT_YY_EOB_ACT_CONTINUE_SCAN;
    }

    script_yyNChars += numberToMove;
    buffer->chBuf[script_yyNChars] = '\0';
    buffer->chBuf[script_yyNChars + 1] = '\0';
    script_yyText = buffer->chBuf;

    return eobAction;
}

int32_t yy_get_previous_state(void)
{
    int32_t state = script_yyStart;

    for (char *cursor = script_yyText; cursor < script_yyCBufferPosition;
         ++cursor) {
        uint8_t yyChar = *cursor == '\0'
                             ? SCRIPT_YY_NUL_TRANSITION_CHAR
                             : (uint8_t)script_yyEc[(uint8_t)*cursor];

        if (script_yyAccept[state] != 0) {
            script_yyLastAcceptingState = state;
            script_yyLastAcceptingCpos = cursor;
        }

        while (script_yyChk[script_yyBase[state] + yyChar] != state) {
            state = script_yyDef[state];
            if (state > SCRIPT_YY_META_THRESHOLD_STATE) {
                yyChar = (uint8_t)script_yyMeta[yyChar];
            }
        }

        state = script_yyNxt[script_yyBase[state] + yyChar];
    }

    return state;
}

int32_t yy_try_NUL_trans(int32_t state)
{
    uint8_t yyChar = SCRIPT_YY_NUL_TRANSITION_CHAR;

    if (script_yyAccept[state] != 0) {
        script_yyLastAcceptingState = state;
        script_yyLastAcceptingCpos = script_yyCBufferPosition;
    }

    while (script_yyChk[script_yyBase[state] + yyChar] != state) {
        state = script_yyDef[state];
        if (state > SCRIPT_YY_META_THRESHOLD_STATE) {
            yyChar = (uint8_t)script_yyMeta[yyChar];
        }
    }

    state = script_yyNxt[script_yyBase[state] + yyChar];
    return state == SCRIPT_YY_END_OF_BUFFER_STATE ? 0 : state;
}

void yyrestart(FILE *inputFile)
{
    if (script_yyCurrentBuffer == NULL) {
        script_yyCurrentBuffer =
            yy_create_buffer(script_yyInputFile,
                                 SCRIPT_YY_CREATE_BUFFER_SIZE);
    }

    yy_init_buffer(script_yyCurrentBuffer, inputFile);
    yy_load_buffer_state();
}

/* Source: CoDUOMP.exe 0x004933c0..0x00493421.
 * Name: generated Flex function yy_switch_to_buffer. */
void yy_switch_to_buffer(script_yy_buffer_t *buffer)
{
    if (script_yyCurrentBuffer == buffer) {
        return;
    }

    if (script_yyCurrentBuffer != NULL) {
        *script_yyCBufferPosition = (char)script_yyHoldChar;
        script_yyCurrentBuffer->bufPos = script_yyCBufferPosition;
        script_yyCurrentBuffer->nChars = script_yyNChars;
    }

    script_yyCurrentBuffer = buffer;
    yy_load_buffer_state();
    script_yyDidBufferSwitchOnEof = qtrue;
}

/* Source: CoDUOMP.exe 0x00493430..0x0049345d.
 * Name: generated Flex function yy_load_buffer_state. */
void yy_load_buffer_state(void)
{
    script_yyNChars = script_yyCurrentBuffer->nChars;
    script_yyCBufferPosition = script_yyCurrentBuffer->bufPos;
    script_yyText = script_yyCBufferPosition;
    script_yyInputFile = script_yyCurrentBuffer->inputFile;
    script_yyHoldChar = (uint8_t)*script_yyCBufferPosition;
}

script_yy_buffer_t *yy_create_buffer(FILE *inputFile, int32_t size)
{
    script_yy_buffer_t *buffer = yy_flex_alloc(sizeof(*buffer));
    if (buffer == NULL) {
        yy_fatal_error("out of dynamic memory in yy_create_buffer()");
    }

    buffer->bufSize = size;
    uint32_t allocationSize =
        (uint32_t)size + SCRIPT_YY_BUFFER_TRAILING_NUL_COUNT;
    buffer->chBuf = yy_flex_alloc((size_t)allocationSize);
    if (buffer->chBuf == NULL) {
        yy_fatal_error("out of dynamic memory in yy_create_buffer()");
    }

    buffer->isOurBuffer = qtrue;
    yy_init_buffer(buffer, inputFile);

    return buffer;
}

/* Source: CoDUOMP.exe 0x004934e0..0x00493511.
 * Name: generated Flex function yy_delete_buffer. */
void yy_delete_buffer(script_yy_buffer_t *buffer)
{
    if (buffer == NULL) {
        return;
    }

    if (buffer == script_yyCurrentBuffer) {
        script_yyCurrentBuffer = NULL;
    }

    if (buffer->isOurBuffer != qfalse) {
        yy_flex_free(buffer->chBuf);
    }

    yy_flex_free(buffer);
}

/* Source: CoDUOMP.exe 0x00493520..0x00493556.
 * Name: generated Flex function yy_init_buffer. */
void yy_init_buffer(script_yy_buffer_t *buffer, FILE *inputFile)
{
    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    yy_flush_buffer(buffer);
    buffer->inputFile = inputFile;
    buffer->fillBuffer = qtrue;
    buffer->isInteractive =
        inputFile == NULL ? qfalse : (isatty(fileno(inputFile)) > 0);
}

void yy_flush_buffer(script_yy_buffer_t *buffer)
{
    if (buffer == NULL) {
        return;
    }

    buffer->nChars = 0;
    buffer->chBuf[0] = '\0';
    buffer->chBuf[1] = '\0';
    buffer->bufPos = buffer->chBuf;
    buffer->atBol = qtrue;
    buffer->bufferStatus = SCRIPT_YY_BUFFER_NEW;

    if (buffer == script_yyCurrentBuffer) {
        yy_load_buffer_state();
    }
}

/* Source: CoDUOMP.exe 0x004935c0..0x0049362c.
 * Name: generated Flex function yy_scan_buffer. */
script_yy_buffer_t *yy_scan_buffer(char *base, uint32_t size)
{
    if (size < SCRIPT_YY_BUFFER_TRAILING_NUL_COUNT ||
        base[size - 2] != '\0' || base[size - 1] != '\0') {
        return NULL;
    }

    script_yy_buffer_t *buffer = yy_flex_alloc(sizeof(*buffer));
    if (buffer == NULL) {
        yy_fatal_error("out of dynamic memory in yy_scan_buffer()");
    }

    buffer->bufSize = (int32_t)size - SCRIPT_YY_BUFFER_TRAILING_NUL_COUNT;
    buffer->chBuf = base;
    buffer->bufPos = buffer->chBuf;
    buffer->isOurBuffer = qfalse;
    buffer->inputFile = NULL;
    buffer->nChars = buffer->bufSize;
    buffer->isInteractive = qfalse;
    buffer->atBol = qtrue;
    buffer->fillBuffer = qfalse;
    buffer->bufferStatus = SCRIPT_YY_BUFFER_NEW;

    yy_switch_to_buffer(buffer);
    return buffer;
}

/* Source: CoDUOMP.exe 0x00493630..0x00493654.
 * Name: generated Flex function yy_scan_string. */
script_yy_buffer_t *yy_scan_string(const char *text)
{
    int32_t length = 0;
    while (text[length] != '\0') {
        ++length;
    }

    return yy_scan_bytes(text, length);
}

/* Source: CoDUOMP.exe 0x00493660..0x004936c6.
 * Name: generated Flex function yy_scan_bytes. */
script_yy_buffer_t *yy_scan_bytes(const char *bytes, int32_t length)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (length < 0) {
        yy_fatal_error("bad buffer in yy_scan_bytes()");
    }

    uint32_t allocationSize =
        (uint32_t)length + SCRIPT_YY_BUFFER_TRAILING_NUL_COUNT;
    char *buffer = yy_flex_alloc((size_t)allocationSize);
    if (buffer == NULL) {
        yy_fatal_error("out of dynamic memory in yy_scan_bytes()");
    }

    for (int32_t index = 0; index < length; ++index) {
        buffer[index] = bytes[index];
    }
    buffer[length] = '\0';
    buffer[length + 1] = '\0';

    script_yy_buffer_t *state =
        yy_scan_buffer(buffer, (uint32_t)length +
                                   SCRIPT_YY_BUFFER_TRAILING_NUL_COUNT);
    if (state == NULL) {
        yy_fatal_error("bad buffer in yy_scan_bytes()");
    }

    state->isOurBuffer = qtrue;
    return state;
}

void yy_fatal_error(const char *message)
{
    fprintf(stderr, "%s\n", message);
    exit(2);
}

/* Source: CoDUOMP.exe 0x004936f0..0x004936fa.
 * Name: generated Flex function yy_flex_alloc. */
void *yy_flex_alloc(size_t size)
{
    return malloc(size);
}

/* Source: CoDUOMP.exe 0x00493700..0x0049370b.
 * Name: generated Flex function yy_flex_realloc. */
void *yy_flex_realloc(void *ptr, size_t size)
{
    return realloc(ptr, size);
}

/* Source: CoDUOMP.exe 0x00493710..0x00493718.
 * Name: generated Flex function yy_flex_free. */
void yy_flex_free(void *ptr)
{
    free(ptr);
}

/* Source: CoDUOMP.exe 0x00493720..0x0049375d.
 * Name: generated parser callback yyerror. */
int32_t yyerror(void)
{
    if (script_yychar == SCRIPT_YY_EOF_TOKEN) {
        CompileError(script_yyPreviousSourcePos,
                     "unexpected end of file found");
    } else if (script_yychar != SCRIPT_YY_BAD_SYNTAX_TOKEN) {
        CompileError(script_yyPreviousSourcePos, "bad syntax");
    }

    return 0;
}

/* Source: CoDUOMP.exe 0x00493810..0x00493815.
 * Name and return type: same-module Mac symbol yywrap. */
int32_t yywrap(void)
{
    return 1;
}
