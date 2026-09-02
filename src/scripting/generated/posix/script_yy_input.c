#include "server/standalone/scripting/script_compile_private.h"

#include "scripting/script_yy_runtime.h"

enum {
    SCRIPT_YY_INPUT_EOF = -1,
    SCRIPT_YY_EOB_ACT_CONTINUE_SCAN = 0,
    SCRIPT_YY_EOB_ACT_END_OF_FILE = 1,
    SCRIPT_YY_EOB_ACT_LAST_MATCH = 2
};

/* This generated scanner entry point exists in coduo_lnxded but not in the
 * retained Windows client scanner. Keep that target-exclusive surface beside
 * the Linux parser while the common Flex runtime remains shared. */
int32_t yyinput(void)
{
    *script_yyCBufferPosition = (char)script_yyHoldChar;

    char *yyText = script_yyText;
    char *bufferPosition = script_yyCBufferPosition;
    if (*script_yyCBufferPosition == '\0') {
        if (script_yyCBufferPosition < script_yyCurrentBuffer->chBuf + script_yyNChars) {
            *script_yyCBufferPosition = '\0';
        } else {
            ++script_yyCBufferPosition;
            int32_t action = yy_get_next_buffer();

            if (action == SCRIPT_YY_EOB_ACT_END_OF_FILE) {
                if (yywrap() != 0) {
                    return SCRIPT_YY_INPUT_EOF;
                }
                if (script_yyDidBufferSwitchOnEof == qfalse) {
                    yyrestart(script_yyInputFile);
                }
                return yyinput();
            }

            if (action == SCRIPT_YY_EOB_ACT_CONTINUE_SCAN) {
                script_yyCBufferPosition = script_yyText + (bufferPosition - yyText);
            } else if (action == SCRIPT_YY_EOB_ACT_LAST_MATCH) {
                yyrestart(script_yyInputFile);
                if (yywrap() != 0) {
                    return SCRIPT_YY_INPUT_EOF;
                }
                if (script_yyDidBufferSwitchOnEof == qfalse) {
                    yyrestart(script_yyInputFile);
                }
                return yyinput();
            }
        }
    }

    uint8_t c = (uint8_t)*script_yyCBufferPosition;
    *script_yyCBufferPosition = '\0';
    ++script_yyCBufferPosition;
    script_yyHoldChar = (uint8_t)*script_yyCBufferPosition;

    return c;
}
