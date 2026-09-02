#include "script_runtime_host.h"

#include "script_string.h"
#include "script_yy_tokens.h"

#include <stdio.h>
#include <string.h>

enum {
    SCRIPT_YY_STRING_TOKEN_TYPE = 13
};

/* NOT_FROM_ORIGINAL_SOURCE: explicit spelling of the original 16-bit token
 * semantic store, shared by the inlined lowercase action and the standalone
 * escaped-string builder. */
static void coduomp_script_yy_set_string_handle_token(uint16_t handle)
{
    memcpy(&script_yylval.source.value, &handle, sizeof(handle));
}

/* Source: CoDUOMP.exe 0x00491eb0..0x00491ec5.
 * Name and signature: same-module Mac symbol TextValue. The same action is
 * inlined into yylex at 0x00492de6 and 0x00492e1c. */
void TextValue(const char *text, int32_t length)
{
    coduomp_script_yy_set_string_handle_token(SL_GetLowercaseStringOfLen(text, 0, (size_t)length + 1, SCRIPT_YY_STRING_TOKEN_TYPE));
}

/* Source: CoDUOMP.exe 0x00491ed0..0x00491f5b.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00491ed0_00491f5c.mcode.
 * Name and signature: same-module Mac symbol StringValue. */
void StringValue(const char *text, int32_t length)
{
    char unescaped[(size_t)length + 1];
    int32_t readIndex = 0;
    int32_t writeIndex = 0;

    while (readIndex < length) {
        if (text[readIndex] == '\\') {
            if (readIndex + 1 == length) {
                break;
            }

            char escaped = text[readIndex + 1];
            if (escaped == 'n') {
                unescaped[writeIndex] = '\n';
            } else if (escaped == 'r') {
                unescaped[writeIndex] = '\r';
            } else if (escaped == 't') {
                unescaped[writeIndex] = '\t';
            } else {
                unescaped[writeIndex] = escaped;
            }
            readIndex += 2;
        } else {
            unescaped[writeIndex] = text[readIndex];
            readIndex++;
        }
        writeIndex++;
    }

    unescaped[writeIndex] = '\0';
    coduomp_script_yy_set_string_handle_token(SL_GetString_(unescaped, 0, SCRIPT_YY_STRING_TOKEN_TYPE));
}

/* Source: CoDUOMP.exe 0x00491f60..0x00491f73. The same source action is
 * inlined into yylex near 0x004926a9. Name: same-module Mac symbol
 * IntegerValue. */
void IntegerValue(const char *text)
{
    int32_t value;

    sscanf(text, "%d", &value);
    script_yylval.source.value = (uintptr_t)(uint32_t)value;
}

/* Source: CoDUOMP.exe 0x00491f80..0x00491f93. The same source action is
 * inlined into yylex at 0x004926dd. Name: same-module Mac symbol
 * FloatValue. */
void FloatValue(const char *text)
{
    float value;

    sscanf(text, "%f", &value);
    script_yylval.source.value = 0;
    memcpy(&script_yylval.source.value, &value, sizeof(value));
}
