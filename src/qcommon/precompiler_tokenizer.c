#include "precompiler.h"

#include "compat/coduo_fp_conversion.h"
#include "precompiler_float.h"
#include "precompiler_services.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if defined(WINDOWS_BEHAVIOR)
void *GetMemory(size_t size);
#elif defined(LINUX_BEHAVIOR)
void *Com_ZoneDebugAlloc(size_t size);
#else
#error "precompiler_tokenizer.c requires WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

#define PC_NUMBER_FLOAT_BASE 10.0L

/* Source: CoDUOMP.exe .data 0x005c53d8..0x005c564c. The 52 ordered
 * punctuation/subtype pairs and terminating null entry are present as one
 * contiguous original initializer. PE_RELOCATION_VALUES_VERIFIED: all 52
 * punctuation-string pointers match the original targets; every initial
 * next-pointer and the sentinel are null. */
static punctuation_t pc_defaultPunctuations[] = {
    {">>=", PC_PUNCTUATION_SHIFT_RIGHT_ASSIGN, NULL},
    {"<<=", PC_PUNCTUATION_SHIFT_LEFT_ASSIGN, NULL},
    {"...", PC_PUNCTUATION_ELLIPSIS, NULL},
    {"##", PC_PUNCTUATION_TOKEN_PASTE, NULL},
    {"&&", PC_OPERATOR_LOGICAL_AND, NULL},
    {"||", PC_OPERATOR_LOGICAL_OR, NULL},
    {">=", PC_OPERATOR_GREATER_OR_EQUAL, NULL},
    {"<=", PC_OPERATOR_LESS_OR_EQUAL, NULL},
    {"==", PC_OPERATOR_EQUAL, NULL},
    {"!=", PC_OPERATOR_NOT_EQUAL, NULL},
    {"*=", PC_PUNCTUATION_MULTIPLY_ASSIGN, NULL},
    {"/=", PC_PUNCTUATION_DIVIDE_ASSIGN, NULL},
    {"%=", PC_PUNCTUATION_MODULO_ASSIGN, NULL},
    {"+=", PC_PUNCTUATION_ADD_ASSIGN, NULL},
    {"-=", PC_PUNCTUATION_SUBTRACT_ASSIGN, NULL},
    {"++", PC_OPERATOR_INCREMENT, NULL},
    {"--", PC_OPERATOR_DECREMENT, NULL},
    {"&=", PC_PUNCTUATION_BITWISE_AND_ASSIGN, NULL},
    {"|=", PC_PUNCTUATION_BITWISE_OR_ASSIGN, NULL},
    {"^=", PC_PUNCTUATION_BITWISE_XOR_ASSIGN, NULL},
    {">>", PC_OPERATOR_SHIFT_RIGHT, NULL},
    {"<<", PC_OPERATOR_SHIFT_LEFT, NULL},
    {"->", PC_PUNCTUATION_POINTER_MEMBER, NULL},
    {"::", PC_PUNCTUATION_SCOPE, NULL},
    {".*", PC_PUNCTUATION_MEMBER_POINTER, NULL},
    {"*", PC_OPERATOR_MULTIPLY, NULL},
    {"/", PC_OPERATOR_DIVIDE, NULL},
    {"%", PC_OPERATOR_MODULO, NULL},
    {"+", PC_OPERATOR_ADD, NULL},
    {"-", PC_OPERATOR_SUBTRACT, NULL},
    {"=", PC_PUNCTUATION_ASSIGN, NULL},
    {"&", PC_OPERATOR_BITWISE_AND, NULL},
    {"|", PC_OPERATOR_BITWISE_OR, NULL},
    {"^", PC_OPERATOR_BITWISE_XOR, NULL},
    {"~", PC_OPERATOR_BITWISE_NOT, NULL},
    {"!", PC_OPERATOR_LOGICAL_NOT, NULL},
    {">", PC_OPERATOR_GREATER, NULL},
    {"<", PC_OPERATOR_LESS, NULL},
    {".", PC_PUNCTUATION_PERIOD, NULL},
    {",", PC_PUNCTUATION_COMMA, NULL},
    {";", PC_PUNCTUATION_SEMICOLON, NULL},
    {":", PC_OPERATOR_TERNARY_COLON, NULL},
    {"?", PC_OPERATOR_TERNARY_QUESTION, NULL},
    {"(", PC_OPERATOR_OPEN_PARENTHESIS, NULL},
    {")", PC_OPERATOR_CLOSE_PARENTHESIS, NULL},
    {"{", PC_PUNCTUATION_OPEN_BRACE, NULL},
    {"}", PC_PUNCTUATION_CLOSE_BRACE, NULL},
    {"[", PC_PUNCTUATION_OPEN_BRACKET, NULL},
    {"]", PC_PUNCTUATION_CLOSE_BRACKET, NULL},
    {"\\", PC_PUNCTUATION_BACKSLASH, NULL},
    {"#", PC_PUNCTUATION_PREPROCESSOR, NULL},
    {"$", PC_PUNCTUATION_DOLLAR, NULL},
    {NULL, 0, NULL}
};

/* Source: CoDUOMP.exe 0x00447100..0x00447209.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00447100_00447209.mcode.
 * Name: exact same-module Mac symbol SetScriptPunctuations. */
void SetScriptPunctuations(script_t *script,
                           punctuation_t *punctuations)
{
    if (script->punctuationTable == NULL) {
#if defined(WINDOWS_BEHAVIOR)
        script->punctuationTable = GetMemory(
            PC_PUNCTUATION_BUCKET_COUNT *
            sizeof(*script->punctuationTable));
#else
        script->punctuationTable = Com_ZoneDebugAlloc(
            PC_PUNCTUATION_BUCKET_COUNT *
            sizeof(*script->punctuationTable));
#endif
    }

    memset(script->punctuationTable, 0,
           PC_PUNCTUATION_BUCKET_COUNT *
           sizeof(*script->punctuationTable));

    for (punctuation_t *punctuation = punctuations;
         punctuation->text != NULL; ++punctuation) {
        /* NOT_FROM_ORIGINAL_SOURCE: punctuation buckets cover the complete
         * unsigned-byte domain. */
        const uint32_t bucket = (uint8_t)punctuation->text[0];
        punctuation_t *previous = NULL;
        punctuation_t *scan = script->punctuationTable[bucket];

        while (scan != NULL) {
            if (strlen(scan->text) < strlen(punctuation->text))
                break;
            previous = scan;
            scan = scan->next;
        }

        punctuation->next = scan;
        if (previous == NULL)
            script->punctuationTable[bucket] = punctuation;
        else
            previous->next = punctuation;
    }
}

/* Source: CoDUOMP.exe 0x00447210..0x00447247.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00447210_00447247.mcode.
 * Name: same-module family name PS_PunctuationStringForSubtype. */
const char *PS_PunctuationStringForSubtype(script_t *script,
                                           int32_t subtype)
{
    for (punctuation_t *punctuation = script->punctuations;
         punctuation->text != NULL; ++punctuation) {
        if (punctuation->subtype == subtype)
            return punctuation->text;
    }
    return "unkown punctuation";
}

/* Source: CoDUOMP.exe 0x00447250..0x004472b2.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00447250_004472b2.mcode.
 * Name: exact same-module Mac symbol ScriptError. */
void ScriptError(script_t *script, const char *format, ...)
{
    if ((script->flags & PC_SCRIPT_FLAG_NO_ERRORS) != 0)
        return;

    char message[PC_DIAGNOSTIC_CAPACITY];
    va_list args;
    va_start(args, format);
    /* NOT_FROM_ORIGINAL_SOURCE: bound the formatted script diagnostic to its
     * fixed destination. */
    (void)vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    Com_Printf("^1Error: file %s, line %d: %s\n",
               script->filename, script->line, message);
}

/* Source: CoDUOMP.exe 0x004472c0..0x00447322.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004472c0_00447322.mcode.
 * Name: exact same-module Mac symbol ScriptWarning. */
void ScriptWarning(script_t *script, const char *format, ...)
{
    if ((script->flags & PC_SCRIPT_FLAG_NO_WARNINGS) != 0)
        return;

    char message[PC_DIAGNOSTIC_CAPACITY];
    va_list args;
    va_start(args, format);
    /* NOT_FROM_ORIGINAL_SOURCE: bound the formatted script diagnostic to its
     * fixed destination. */
    (void)vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    Com_Printf("file %s, line %d: %s\n",
               script->filename, script->line, message);
}

/* Source: CoDUOMP.exe 0x00447330..0x0044735e.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00447330_0044735e.mcode.
 * Name: exact same-module Mac symbol PS_CreatePunctuationTable. */
void PS_CreatePunctuationTable(script_t *script,
                               punctuation_t *punctuations)
{
    if (punctuations != NULL) {
        SetScriptPunctuations(script, punctuations);
        script->punctuations = punctuations;
        return;
    }

    SetScriptPunctuations(script, pc_defaultPunctuations);
    script->punctuations = pc_defaultPunctuations;
}

/* Source: CoDUOMP.exe 0x00447360..0x0044741e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00447360_0044741e.mcode.
 * Name: exact same-module Mac symbol PS_ReadWhiteSpace. */
qboolean PS_ReadWhiteSpace(script_t *script)
{
    while (*script->scriptCursor != '\0') {
        /* 0x00447369/0x00447391 use signed JG/JLE byte ordering. */
        while ((int8_t)*script->scriptCursor <= ' ') {
            if (*script->scriptCursor == '\0')
                return qfalse;
            if (*script->scriptCursor == '\n')
                ++script->line;
            ++script->scriptCursor;
        }

        if (*script->scriptCursor != '/')
            return qtrue;

        if (script->scriptCursor[1] == '/') {
            ++script->scriptCursor;
            do {
                ++script->scriptCursor;
                if (*script->scriptCursor == '\0')
                    return qfalse;
            } while (*script->scriptCursor != '\n');

            ++script->line;
            ++script->scriptCursor;
            continue;
        }

        if (script->scriptCursor[1] != '*')
            return qtrue;

        ++script->scriptCursor;
        do {
            ++script->scriptCursor;
            if (*script->scriptCursor == '\0')
                return qfalse;
            if (*script->scriptCursor == '\n')
                ++script->line;
        } while (*script->scriptCursor != '*' ||
                 script->scriptCursor[1] != '/');

        ++script->scriptCursor;
        if (*script->scriptCursor == '\0')
            return qfalse;
        ++script->scriptCursor;
        if (*script->scriptCursor == '\0')
            return qfalse;
    }

    return qfalse;
}

/* Source: CoDUOMP.exe 0x00447420..0x004475ad.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00447420_004475ad.mcode and its
 * compiler-owned switch tables at 0x004475b0..0x0044763b.
 * Name: exact same-module Mac symbol PS_ReadEscapeCharacter. */
qboolean PS_ReadEscapeCharacter(script_t *script, char *out)
{
    uint32_t value = 0;
    ++script->scriptCursor;

    switch (*script->scriptCursor) {
    case '"': value = '"'; break;
    case '\'': value = '\''; break;
    case '?': value = '?'; break;
    case '\\': value = '\\'; break;
    case 'a': value = '\a'; break;
    case 'b': value = '\b'; break;
    case 'f': value = '\f'; break;
    case 'n': value = '\n'; break;
    case 'r': value = '\r'; break;
    case 't': value = '\t'; break;
    case 'v': value = '\v'; break;
    case 'x':
        ++script->scriptCursor;
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        for (;;) {
            const int32_t character = *script->scriptCursor;
            uint32_t digit;
            if (character >= '0' && character <= '9')
                digit = character - '0';
            else if (character >= 'A' && character <= 'Z')
                digit = character - 'A' + 10;
            else if (character >= 'a' && character <= 'z')
                digit = character - 'a' + 10;
            else
                break;

            value = value * 16 + digit;
            ++script->scriptCursor;
        }
        --script->scriptCursor;
        break;
    default:
        if (*script->scriptCursor < '0' ||
            *script->scriptCursor > '9') {
            ScriptError(script, "unknown escape char");
        }

        while (*script->scriptCursor >= '0' &&
               *script->scriptCursor <= '9') {
            value = value * 10 + *script->scriptCursor - '0';
            ++script->scriptCursor;
        }
        --script->scriptCursor;
        break;
    }

    /* The PE accumulates in a 32-bit register and performs a signed compare
     * against 255, so overflow wraps before this clamp decision. */
    if ((int32_t)value > 255) {
        ScriptWarning(script, "too large value in escape character");
        value = 255;
    }

    ++script->scriptCursor;
    *out = (char)value;
    return qtrue;
}

/* Source: CoDUOMP.exe 0x00447640..0x0044779b.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00447640_0044779b.mcode.
 * Name: exact same-module Mac symbol PS_ReadString. */
qboolean PS_ReadString(script_t *script, token_t *token,
                       int32_t quote)
{
    token->type = quote == '"' ? PC_TOKEN_TYPE_STRING
                               : PC_TOKEN_TYPE_LITERAL;
    token->string[0] = *script->scriptCursor;
    ++script->scriptCursor;

    int32_t length = 1;
    for (;;) {
        if (*script->scriptCursor == '\\' &&
            (script->flags & PC_SCRIPT_FLAG_NO_STRING_ESCAPE_CHARS) == 0) {
            if (PS_ReadEscapeCharacter(script, &token->string[length]) ==
                qfalse) {
                token->string[length] = '\0';
                return qfalse;
            }
            ++length;
        } else if (*script->scriptCursor == quote) {
            ++script->scriptCursor;
            if ((script->flags & PC_SCRIPT_FLAG_NO_STRING_CONCAT) != 0)
                break;

            char *savedCursor = script->scriptCursor;
            const int32_t savedLine = script->line;
            if (PS_ReadWhiteSpace(script) == qfalse ||
                *script->scriptCursor != quote) {
                script->scriptCursor = savedCursor;
                script->line = savedLine;
                break;
            }

            ++script->scriptCursor;
        } else if (*script->scriptCursor == '\0') {
            token->string[length] = '\0';
            ScriptError(script, "missing trailing quote");
            return qfalse;
        } else if (*script->scriptCursor == '\n') {
            token->string[length] = '\0';
            ScriptError(script, "newline inside string %s", token->string);
            return qfalse;
        } else {
            token->string[length] = *script->scriptCursor;
            ++script->scriptCursor;
            ++length;
        }

        if (length >= MAX_TOKEN_CHARS - 2) {
            ScriptError(script, "string longer than MAX_TOKEN = %d",
                        MAX_TOKEN_CHARS);
            return qfalse;
        }
    }

    token->string[length++] = (char)quote;
    token->string[length] = '\0';
    token->subtype = length;
    return qtrue;
}

/* Source: CoDUOMP.exe 0x004477a0..0x00447816.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004477a0_00447816.mcode.
 * Name: exact same-module Mac symbol PS_ReadName. */
qboolean PS_ReadName(script_t *script, token_t *token)
{
    int32_t length = 0;
    token->type = PC_TOKEN_TYPE_NAME;

    do {
        /* NOT_FROM_ORIGINAL_SOURCE: reserve the token field's final byte for
         * its NUL before copying another name byte. */
        if (length >= MAX_TOKEN_CHARS - 1) {
            token->string[MAX_TOKEN_CHARS - 1] = '\0';
            ScriptError(script, "name longer than MAX_TOKEN = %d",
                        MAX_TOKEN_CHARS);
            return qfalse;
        }
        token->string[length++] = *script->scriptCursor;
        ++script->scriptCursor;
    } while ((*script->scriptCursor >= 'a' &&
              *script->scriptCursor <= 'z') ||
             (*script->scriptCursor >= 'A' &&
              *script->scriptCursor <= 'Z') ||
             (*script->scriptCursor >= '0' &&
              *script->scriptCursor <= '9') ||
             *script->scriptCursor == '_');

    token->string[length] = '\0';
    token->subtype = length;
    return qtrue;
}

#if defined(WINDOWS_BEHAVIOR)
/* Source: CoDUOMP.exe 0x00447820..0x00447977.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00447820_00447977.mcode.
 * Name: exact same-module Mac symbol NumberValue. */
void NumberValue(const char *string, int32_t subtype,
                 int32_t *intValue, double *floatValue)
{
    *intValue = 0;
    *floatValue = 0.0;

    if ((subtype & PC_TOKEN_SUBTYPE_FLOAT) != 0) {
        double numberValue = 0.0;
        uint32_t divisor = 0;

        for (; *string != '\0'; ++string) {
            if (*string == '.') {
                if (divisor != 0) {
                    *floatValue = numberValue;
                    return;
                }
                divisor = 10;
                /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
                ++string;
            }

            if (divisor == 0) {
                numberValue = numberValue * 10.0 + (*string - '0');
            } else {
                numberValue += (double)(*string - '0') / (double)divisor;
                divisor *= 10;
            }
        }

        *intValue = coduo_fp_to_i32_f64(numberValue);
        *floatValue = numberValue;
        return;
    }

    uint32_t value = 0;
    if ((subtype & PC_TOKEN_SUBTYPE_DECIMAL) != 0) {
        for (; *string != '\0'; ++string)
            value = value * 10 + (uint32_t)(*string - '0');
    } else if ((subtype & PC_TOKEN_SUBTYPE_HEX) != 0) {
        for (string += 2; *string != '\0'; ++string) {
            value <<= 4;
            if (*string >= 'a' && *string <= 'f')
                value += (uint32_t)(*string - 'a' + 10);
            else if (*string >= 'A' && *string <= 'F')
                value += (uint32_t)(*string - 'A' + 10);
            else
                value += (uint32_t)(*string - '0');
        }
    } else if ((subtype & PC_TOKEN_SUBTYPE_OCTAL) != 0) {
        while (*++string != '\0')
            value = value * 8 + (uint32_t)(*string - '0');
    } else if ((subtype & PC_TOKEN_SUBTYPE_BINARY) != 0) {
        for (string += 2; *string != '\0'; ++string)
            value = value * 2 + (uint32_t)(*string - '0');
    } else {
        return;
    }

    *intValue = (int32_t)value;
    *floatValue = (double)value;
}

#else
/* coduo_lnxded 0x0807de30..0x0807e0d0 carries the numeric accumulator as an
 * x87 TBYTE and stores that ten-byte payload in the token's twelve-byte slot.
 * The whole body remains separate because changing only its field access
 * would silently change the arithmetic precision and conversion width. */
void NumberValue(const char *string, int32_t subtype,
                 int32_t *intValue,
                 uint8_t floatValue[PC_TOKEN_FLOAT_VALUE_SIZE])
{
#if EMULATE_X87
    x87f numberValue = x87f_load_f32(0.0f);
#else
    long double numberValue = 0.0L;
    size_t numberValueCopySize = sizeof(numberValue);
    if (numberValueCopySize > PC_X87_EXTENDED_TBYTE_SIZE) {
        numberValueCopySize = PC_X87_EXTENDED_TBYTE_SIZE;
    }
#endif

    *intValue = 0;
    memset(floatValue, 0, PC_TOKEN_FLOAT_VALUE_SIZE);

    if ((subtype & PC_TOKEN_SUBTYPE_FLOAT) != 0) {
        uint32_t divisor = 0;

        for (; *string != '\0'; ++string) {
            if (*string == '.') {
                if (divisor != 0) {
#if EMULATE_X87
                    coduo_pc_store_token_float80(floatValue,
                                                 numberValue);
#else
                    /* Stock clears the 12-byte token slot, then fstp writes
                     * only the 10-byte x87 TBYTE payload.  Relaxed non-x87
                     * hosts copy only their valid native long-double bytes. */
                    memcpy(floatValue, &numberValue,
                           numberValueCopySize);
#endif
                    return;
                }
                /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
                divisor = 10;
                string++;
            }

            if (divisor == 0) {
#if EMULATE_X87
                /* fild(digit) + fld(10.0L)*numberValue */
                numberValue = x87f_add(
                    x87f_load_i32(*string - '0'),
                    x87f_mul(x87f_load_f64((double)PC_NUMBER_FLOAT_BASE),
                             numberValue));
#else
                numberValue =
                    (long double)(*string - '0') +
                    PC_NUMBER_FLOAT_BASE * numberValue;
#endif
            } else {
#if EMULATE_X87
                /* numberValue += fild(digit) / (uint32->80)divisor */
                numberValue = x87f_add(
                    numberValue,
                    x87f_div(x87f_load_i32(*string - '0'),
                             x87f_load_f64((double)divisor)));
#else
                numberValue += (long double)(*string - '0') /
                               (long double)divisor;
#endif
                divisor *= 10;
            }
        }

        /* 0x807df16: fistp QWORD + low dword = unsigned-int conversion,
         * not the fistp DWORD a direct (int32_t) cast would emit. */
#if EMULATE_X87
        *intValue = (int32_t)(uint32_t)extF80_to_i64(
            numberValue, softfloat_round_minMag, false);
        coduo_pc_store_token_float80(floatValue, numberValue);
#else
        *intValue = (int32_t)(uint32_t)numberValue;
        memcpy(floatValue, &numberValue, numberValueCopySize);
#endif
        return;
    }

    if ((subtype & PC_TOKEN_SUBTYPE_DECIMAL) != 0) {
        for (; *string != '\0'; ++string) {
            uint32_t value = (uint32_t)*intValue;
            value = value * 10 + (uint32_t)(*string - '0');
            *intValue = (int32_t)value;
        }
#if EMULATE_X87
        numberValue = x87f_load_f64((double)(uint32_t)*intValue);
        coduo_pc_store_token_float80(floatValue, numberValue);
#else
        numberValue = (long double)(uint32_t)*intValue;
        memcpy(floatValue, &numberValue, numberValueCopySize);
#endif
        return;
    }

    if ((subtype & PC_TOKEN_SUBTYPE_HEX) != 0) {
        for (string += 2; *string != '\0'; ++string) {
            uint32_t value = (uint32_t)*intValue << 4;
            if (*string >= 'a' && *string <= 'f') {
                value += (uint32_t)(*string - 'a' + 10);
            } else if (*string >= 'A' && *string <= 'F') {
                value += (uint32_t)(*string - 'A' + 10);
            } else {
                value += (uint32_t)(*string - '0');
            }
            *intValue = (int32_t)value;
        }
#if EMULATE_X87
        numberValue = x87f_load_f64((double)(uint32_t)*intValue);
        coduo_pc_store_token_float80(floatValue, numberValue);
#else
        numberValue = (long double)(uint32_t)*intValue;
        memcpy(floatValue, &numberValue, numberValueCopySize);
#endif
        return;
    }

    if ((subtype & PC_TOKEN_SUBTYPE_OCTAL) != 0) {
        while (*++string != '\0') {
            uint32_t value = (uint32_t)*intValue;
            value = value * 8 + (uint32_t)(*string - '0');
            *intValue = (int32_t)value;
        }
#if EMULATE_X87
        numberValue = x87f_load_f64((double)(uint32_t)*intValue);
        coduo_pc_store_token_float80(floatValue, numberValue);
#else
        numberValue = (long double)(uint32_t)*intValue;
        memcpy(floatValue, &numberValue, numberValueCopySize);
#endif
        return;
    }

    if ((subtype & PC_TOKEN_SUBTYPE_BINARY) != 0) {
        for (string += 2; *string != '\0'; ++string) {
            uint32_t value = (uint32_t)*intValue;
            value = value * 2 + (uint32_t)(*string - '0');
            *intValue = (int32_t)value;
        }
#if EMULATE_X87
        numberValue = x87f_load_f64((double)(uint32_t)*intValue);
        coduo_pc_store_token_float80(floatValue, numberValue);
#else
        numberValue = (long double)(uint32_t)*intValue;
        memcpy(floatValue, &numberValue, numberValueCopySize);
#endif
    }
}
#endif

/* Source: CoDUOMP.exe 0x00447980..0x00447c4b.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00447980_00447c4b.mcode.
 * Name: exact same-module Mac symbol PS_ReadNumber. */
qboolean PS_ReadNumber(script_t *script, token_t *token)
{
    int32_t length = 0;
    token->type = PC_TOKEN_TYPE_NUMBER;
    /* CoDUOMP.exe 0x00447983 and coduo_lnxded 0x0807e0e5 write only type.
     * PS_ReadToken's full-record clear is the normal initializer; this body
     * ORs its result into the incoming subtype exactly as both retail helpers
     * do.  The former Linux reconstruction's explicit subtype clear was not
     * present in the machine code. */

    if (script->scriptCursor[0] == '0' &&
        (script->scriptCursor[1] == 'x' ||
         script->scriptCursor[1] == 'X')) {
        token->string[length++] = *script->scriptCursor++;
        token->string[length++] = *script->scriptCursor++;

        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        while ((*script->scriptCursor >= '0' &&
                *script->scriptCursor <= '9') ||
               (*script->scriptCursor >= 'a' &&
                *script->scriptCursor <= 'f') ||
               *script->scriptCursor == 'A') {
            /* NOT_FROM_ORIGINAL_SOURCE: reserve the token field's final byte
             * for its NUL before copying another hexadecimal digit. */
            if (length >= MAX_TOKEN_CHARS - 1) {
                token->string[MAX_TOKEN_CHARS - 1] = '\0';
                ScriptError(script,
                            "hexadecimal number longer than MAX_TOKEN = %d",
                            MAX_TOKEN_CHARS);
                return qfalse;
            }
            token->string[length++] = *script->scriptCursor++;
        }
        token->subtype |= PC_TOKEN_SUBTYPE_HEX;
    } else if (script->scriptCursor[0] == '0' &&
               (script->scriptCursor[1] == 'b' ||
                script->scriptCursor[1] == 'B')) {
        token->string[length++] = *script->scriptCursor++;
        token->string[length++] = *script->scriptCursor++;

        while (*script->scriptCursor == '0' ||
               *script->scriptCursor == '1') {
            /* NOT_FROM_ORIGINAL_SOURCE: reserve the token field's final byte
             * for its NUL before copying another binary digit. */
            if (length >= MAX_TOKEN_CHARS - 1) {
                token->string[MAX_TOKEN_CHARS - 1] = '\0';
                ScriptError(script,
                            "binary number longer than MAX_TOKEN = %d",
                            MAX_TOKEN_CHARS);
                return qfalse;
            }
            token->string[length++] = *script->scriptCursor++;
        }
        token->subtype |= PC_TOKEN_SUBTYPE_BINARY;
    } else {
        qboolean octal = *script->scriptCursor == '0' ? qtrue : qfalse;
        qboolean hasDot = qfalse;

        for (;;) {
            const char character = *script->scriptCursor;
            if (character == '.') {
                hasDot = qtrue;
            } else if (character == '8' || character == '9') {
                octal = qfalse;
            } else if (character < '0' || character > '9') {
                token->subtype |= octal != qfalse
                    ? PC_TOKEN_SUBTYPE_OCTAL
                    : PC_TOKEN_SUBTYPE_DECIMAL;
                if (hasDot != qfalse)
                    token->subtype |= PC_TOKEN_SUBTYPE_FLOAT;
                break;
            }

            token->string[length++] = *script->scriptCursor++;
            if (length >= MAX_TOKEN_CHARS - 1) {
                ScriptError(script, "number longer than MAX_TOKEN = %d",
                            MAX_TOKEN_CHARS);
                return qfalse;
            }
        }
    }

    for (int32_t suffix = 0; suffix < PC_NUMBER_SUFFIX_CHECK_COUNT;
         ++suffix) {
        const char character = *script->scriptCursor;
        if ((character == 'l' || character == 'L') &&
            (token->subtype & PC_TOKEN_SUBTYPE_LONG) == 0) {
            ++script->scriptCursor;
            token->subtype |= PC_TOKEN_SUBTYPE_LONG;
        } else if ((character == 'u' || character == 'U') &&
                   (token->subtype & (PC_TOKEN_SUBTYPE_UNSIGNED |
                                      PC_TOKEN_SUBTYPE_FLOAT)) == 0) {
            ++script->scriptCursor;
            token->subtype |= PC_TOKEN_SUBTYPE_UNSIGNED;
        }
    }

    token->string[length] = '\0';
    NumberValue(token->string, token->subtype, &token->intValue,
#if defined(WINDOWS_BEHAVIOR)
                &token->floatValue);
#else
                token->floatValue);
#endif
    if ((token->subtype & PC_TOKEN_SUBTYPE_FLOAT) == 0)
        token->subtype |= PC_TOKEN_SUBTYPE_INTEGER;
    return qtrue;
}

/* Source: CoDUOMP.exe 0x00447c50..0x00447d2b.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00447c50_00447d2b.mcode.
 * Name: same-family Mac symbol PS_ReadLiteral. */
qboolean PS_ReadLiteral(script_t *script, token_t *token)
{
    token->type = PC_TOKEN_TYPE_LITERAL;
    token->string[0] = *script->scriptCursor++;

    if (*script->scriptCursor == '\0') {
        ScriptError(script, "end of file before trailing '");
        return qfalse;
    }

    if (*script->scriptCursor == '\\') {
        if (PS_ReadEscapeCharacter(script, &token->string[1]) == qfalse)
            return qfalse;
    } else {
        token->string[1] = *script->scriptCursor++;
    }

    if (*script->scriptCursor != '\'') {
        ScriptWarning(script, "too many characters in literal, ignored");
        while (*script->scriptCursor != '\0' &&
               *script->scriptCursor != '\'' &&
               *script->scriptCursor != '\n') {
            ++script->scriptCursor;
        }
        /* NOT_FROM_ORIGINAL_SOURCE: recovery leaves the cursor on a closing
         * quote for the common copy and rejects a source boundary without one. */
        if (*script->scriptCursor != '\'') {
            ScriptError(script, "missing trailing '");
            return qfalse;
        }
    }

    token->string[2] = *script->scriptCursor++;
    token->string[3] = '\0';
    token->subtype = (int8_t)token->string[1];
    return qtrue;
}

/* Source: CoDUOMP.exe 0x00447d30..0x00447dc6.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00447d30_00447dc6.mcode.
 * Name: exact same-module Mac symbol PS_ReadPunctuation. */
qboolean PS_ReadPunctuation(script_t *script,
                            token_t *token)
{
    /* NOT_FROM_ORIGINAL_SOURCE: punctuation lookup uses the same complete
     * unsigned-byte bucket domain as table construction. */
    const uint32_t bucket = (uint8_t)*script->scriptCursor;
    for (punctuation_t *punctuation =
             script->punctuationTable[bucket];
         punctuation != NULL; punctuation = punctuation->next) {
        const size_t length = strlen(punctuation->text);
        const size_t remaining =
            (size_t)(script->endCursor - script->scriptCursor);
        if (length > remaining ||
            strncmp(script->scriptCursor, punctuation->text, length) != 0) {
            continue;
        }

        strncpy(token->string, punctuation->text,
                MAX_TOKEN_CHARS);
        script->scriptCursor += length;
        token->type = PC_TOKEN_TYPE_PUNCTUATION;
        token->subtype = punctuation->subtype;
        return qtrue;
    }
    return qfalse;
}

/* Source: CoDUOMP.exe 0x00447dd0..0x00447e42.
 * Evidence: repaired Ghidra function-boundary record
 * coduomp/mcode/CoDUOMP/FUN_00447dd0_00447e42.mcode.
 * Name: exact same-module Mac symbol PS_ReadPrimitive. */
qboolean PS_ReadPrimitive(script_t *script,
                          token_t *token)
{
    int32_t length = 0;
    while ((int8_t)*script->scriptCursor > ' ' &&
           *script->scriptCursor != ';') {
        /* NOT_FROM_ORIGINAL_SOURCE: reserve the token field's final byte for
         * its NUL before copying another primitive byte. */
        if (length >= MAX_TOKEN_CHARS - 1) {
            token->string[MAX_TOKEN_CHARS - 1] = '\0';
            ScriptError(script,
                        "primitive token longer than MAX_TOKEN = %d",
                        MAX_TOKEN_CHARS);
            return qfalse;
        }
        token->string[length++] = *script->scriptCursor++;
    }

    token->string[length] = '\0';
    memcpy(&script->token, token, sizeof(script->token));
    return qtrue;
}

/* Source: CoDUOMP.exe 0x00447e50..0x00447fbe.
 * Evidence: repaired Ghidra function-boundary record
 * coduomp/mcode/CoDUOMP/FUN_00447e50_00447fbe.mcode.
 * Name: exact same-module Mac symbol PS_ReadToken. */
qboolean PS_ReadToken(script_t *script, token_t *token)
{
    if (script->tokenAvailable != qfalse) {
        script->tokenAvailable = qfalse;
        memcpy(token, &script->token, sizeof(*token));
        return qtrue;
    }

    script->lastScriptCursor = script->scriptCursor;
    script->lastLine = script->line;
    memset(token, 0, sizeof(*token));

    script->whitespaceStart = script->scriptCursor;
    token->whitespaceStart = script->scriptCursor;
    if (PS_ReadWhiteSpace(script) == qfalse)
        return qfalse;

    script->whitespaceEnd = script->scriptCursor;
    token->whitespaceEnd = script->scriptCursor;
    token->line = script->line;
    token->linesCrossed = script->line - script->lastLine;

    const char character = *script->scriptCursor;
    qboolean read;
    if (character == '"') {
        read = PS_ReadString(script, token, '"');
    } else if (character == '\'') {
        read = PS_ReadString(script, token, '\'');
    } else if ((character >= '0' && character <= '9') ||
               (character == '.' && script->scriptCursor[1] >= '0' &&
                script->scriptCursor[1] <= '9')) {
        read = PS_ReadNumber(script, token);
    } else if ((script->flags & PC_SCRIPT_FLAG_PRIMITIVE) != 0) {
        return PS_ReadPrimitive(script, token);
    } else if ((character >= 'a' && character <= 'z') ||
               (character >= 'A' && character <= 'Z') ||
               character == '_') {
        read = PS_ReadName(script, token);
    } else {
        read = PS_ReadPunctuation(script, token);
        if (read == qfalse)
            ScriptError(script, "can't read token");
    }

    if (read == qfalse)
        return qfalse;

    memcpy(&script->token, token, sizeof(script->token));
    return qtrue;
}

/* Source: CoDUOMP.exe 0x00447fc0..0x0044808e.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00447fc0_0044808e.mcode.
 * Name: exact same-module Mac symbol PS_ExpectTokenString. */
qboolean PS_ExpectTokenString(script_t *script, const char *string)
{
    token_t token;
    if (PS_ReadToken(script, &token) == qfalse) {
        ScriptError(script, "couldn't find expected %s", string);
        return qfalse;
    }

    if (strcmp(token.string, string) == 0)
        return qtrue;

    ScriptError(script, "expected %s, found %s", string, token.string);
    return qfalse;
}

/* Source: CoDUOMP.exe 0x00448090..0x004483aa.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00448090_004483aa.mcode.
 * Name: exact same-module Mac symbol PS_ExpectTokenType. */
qboolean PS_ExpectTokenType(script_t *script, int32_t type,
                            int32_t subtype,
                            token_t *token)
{
    char expected[MAX_TOKEN_CHARS];

    if (PS_ReadToken(script, token) == qfalse) {
        ScriptError(script, "couldn't read expected token");
        return qfalse;
    }

    if (token->type != type) {
        /* The PE does not initialize this buffer for an invalid type outside
         * the parser's five token kinds. Valid callers always select one. */
        if (type == PC_TOKEN_TYPE_STRING)
            strcpy(expected, "string");
        if (type == PC_TOKEN_TYPE_LITERAL)
            strcpy(expected, "literal");
        if (type == PC_TOKEN_TYPE_NUMBER)
            strcpy(expected, "number");
        if (type == PC_TOKEN_TYPE_NAME)
            strcpy(expected, "name");
        if (type == PC_TOKEN_TYPE_PUNCTUATION)
            strcpy(expected, "punctuation");

        ScriptError(script, "expected a %s, found %s", expected,
                    token->string);
        return qfalse;
    }

    if (token->type == PC_TOKEN_TYPE_NUMBER &&
        (token->subtype & subtype) != subtype) {
        /* NOT_FROM_ORIGINAL_SOURCE: initialize the expected-description text
         * before appending requested subtype labels in their established order. */
        expected[0] = '\0';
        if ((subtype & PC_TOKEN_SUBTYPE_DECIMAL) != 0)
            strcpy(expected, "decimal");
        if ((subtype & PC_TOKEN_SUBTYPE_HEX) != 0)
            strcpy(expected, "hex");
        if ((subtype & PC_TOKEN_SUBTYPE_OCTAL) != 0)
            strcpy(expected, "octal");
        if ((subtype & PC_TOKEN_SUBTYPE_BINARY) != 0)
            strcpy(expected, "binary");
        if ((subtype & PC_TOKEN_SUBTYPE_LONG) != 0) {
            if (expected[0] != '\0')
                strcat(expected, " ");
            strcat(expected, "long");
        }
        if ((subtype & PC_TOKEN_SUBTYPE_UNSIGNED) != 0) {
            if (expected[0] != '\0')
                strcat(expected, " ");
            strcat(expected, "unsigned");
        }
        if ((subtype & PC_TOKEN_SUBTYPE_FLOAT) != 0) {
            if (expected[0] != '\0')
                strcat(expected, " ");
            strcat(expected, "float");
        }
        if ((subtype & PC_TOKEN_SUBTYPE_INTEGER) != 0) {
            if (expected[0] != '\0')
                strcat(expected, " ");
            strcat(expected, "integer");
        }

        ScriptError(script, "expected %s, found %s", expected,
                    token->string);
        return qfalse;
    }

    if (token->type == PC_TOKEN_TYPE_PUNCTUATION) {
        if (subtype <= 0 || subtype >= PC_DEFAULT_PUNCTUATION_COUNT) {
            ScriptError(script, "BUG: wrong punctuation subtype");
            return qfalse;
        }
        if (token->subtype != subtype) {
            /* NOT_FROM_ORIGINAL_SOURCE: punctuation subtypes are one-based;
             * select the matching zero-based row and pass only its text. */
            const punctuation_t punctuation =
                script->punctuations[subtype - 1];
            ScriptError(script, "expected %s, found %s",
                        punctuation.text, token->string);
            return qfalse;
        }
    }

    return qtrue;
}

/* Source: CoDUOMP.exe 0x004483b0..0x004483d4.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004483b0_004483d4.mcode.
 * Name: same-family symbol PS_ReadTokenOrError. */
qboolean PS_ReadTokenOrError(script_t *script,
                             token_t *token)
{
    if (PS_ReadToken(script, token) == qfalse) {
        ScriptError(script, "couldn't read expected token");
        return qfalse;
    }
    return qtrue;
}

/* Source: CoDUOMP.exe 0x004483e0..0x0044847c.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_004483e0_0044847c.mcode.
 * Name: same-family symbol PS_CheckTokenString. */
qboolean PS_CheckTokenString(script_t *script, const char *string)
{
    token_t token;
    if (PS_ReadToken(script, &token) == qfalse)
        return qfalse;

    if (strcmp(token.string, string) == 0)
        return qtrue;

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    script->scriptCursor = script->lastScriptCursor;
    return qfalse;
}

/* Source: CoDUOMP.exe 0x00448480..0x00448513.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00448480_00448513.mcode.
 * Name: same-family symbol PS_CheckTokenType. */
qboolean PS_CheckTokenType(script_t *script, int32_t type,
                           int32_t subtype,
                           token_t *token)
{
    token_t readToken;
    if (PS_ReadToken(script, &readToken) == qfalse)
        return qfalse;

    if (readToken.type == type &&
        (readToken.subtype & subtype) == subtype) {
        memcpy(token, &readToken, sizeof(*token));
        return qtrue;
    }

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    script->scriptCursor = script->lastScriptCursor;
    return qfalse;
}

/* Source: CoDUOMP.exe 0x00448520..0x004485c9.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00448520_004485c9.mcode.
 * Name: same-family symbol PS_SkipUntilString. */
qboolean PS_SkipUntilString(script_t *script, const char *string)
{
    token_t token;
    do {
        if (PS_ReadToken(script, &token) == qfalse)
            return qfalse;
    } while (strcmp(token.string, string) != 0);
    return qtrue;
}

/* Source: CoDUOMP.exe 0x004485d0..0x004485db.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_004485d0_004485db.mcode.
 * Name: same-family symbol PS_UnreadLastToken. */
void PS_UnreadLastToken(script_t *script)
{
    script->tokenAvailable = qtrue;
}

/* Source: CoDUOMP.exe 0x004485e0..0x004485fe.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_004485e0_004485fe.mcode.
 * Name: same-family symbol PS_UnreadToken. */
void PS_UnreadToken(script_t *script,
                    const token_t *token)
{
    memcpy(&script->token, token, sizeof(script->token));
    script->tokenAvailable = qtrue;
}

/* Source: CoDUOMP.exe 0x00448600..0x0044861b.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00448600_0044861b.mcode.
 * Name: same-family symbol PS_ReadWhitespaceChar. The PE writes only AL on
 * both returns, proving that this client build's return type is one byte. */
#if defined(WINDOWS_BEHAVIOR)
char PS_ReadWhitespaceChar(script_t *script)
#else
int32_t PS_ReadWhitespaceChar(script_t *script)
#endif
{
    if (script->whitespaceStart == script->whitespaceEnd)
        return '\0';
    return *script->whitespaceStart++;
}

/* Source: CoDUOMP.exe 0x00448620..0x00448664.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00448620_00448664.mcode.
 * Name: exact same-module Mac symbol StripDoubleQuotes. */
void StripDoubleQuotes(char *string)
{
    if (string[0] == '"') {
        /* The PE's inline forward copy includes the terminator. memmove keeps
         * that proven overlapping-copy behavior portable. */
        memmove(string, string + 1, strlen(string));
    }

    const size_t length = strlen(string);
    /* NOT_FROM_ORIGINAL_SOURCE: an empty string has no trailing quote slot. */
    if (length != 0 && string[length - 1] == '"')
        string[length - 1] = '\0';
}

/* Source: CoDUOMP.exe 0x00448670..0x004486b4.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00448670_004486b4.mcode.
 * Name: same-family symbol StripSingleQuotes. */
void StripSingleQuotes(char *string)
{
    if (string[0] == '\'')
        memmove(string, string + 1, strlen(string));

    const size_t length = strlen(string);
    /* NOT_FROM_ORIGINAL_SOURCE: an empty string has no trailing quote slot. */
    if (length != 0 && string[length - 1] == '\'')
        string[length - 1] = '\0';
}

#if defined(WINDOWS_BEHAVIOR)
/* Source: CoDUOMP.exe 0x004486c0..0x0044877b.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_004486c0_0044877b.mcode.
 * Name: same-family symbol PS_ReadFloat. */
double PS_ReadFloat(script_t *script)
{
    token_t token;
    double sign = 1.0;

    (void)PS_ReadTokenOrError(script, &token);
    if (strcmp(token.string, "-") == 0) {
        sign = -1.0;
        (void)PS_ExpectTokenType(script, PC_TOKEN_TYPE_NUMBER,
                                 PC_TOKEN_SUBTYPE_NONE, &token);
    } else if (token.type != PC_TOKEN_TYPE_NUMBER) {
        ScriptError(script, "expected float value, found %s\n",
                    token.string);
    }

    return sign * token.floatValue;
}

#else
long double PS_ReadFloat(script_t *script)
{
    token_t token;
    /* 0x807f0b8/0x807f105: sign is an x87 long double constant
     * ({0,0x80000000,0x3fff/0xbfff} immediate stores), not an int. */
#if EMULATE_X87
    x87f sign = x87f_load_f32(1.0f);
#else
    long double sign = 1.0L;
#endif

    PS_ReadTokenOrError(script, &token);
    if (strcmp(token.string, "-") == 0) {
#if EMULATE_X87
        sign = x87f_load_f32(-1.0f);
#else
        sign = -1.0L;
#endif
        PS_ExpectTokenType(script, PC_TOKEN_TYPE_NUMBER, PC_TOKEN_SUBTYPE_NONE,
                     &token);
    } else if (token.type != PC_TOKEN_TYPE_NUMBER) {
        ScriptError(script, "expected float value, found %s\n",
                     token.string);
    }

#if EMULATE_X87
    /* fld TBYTE(token) * sign(±1.0), returned narrowed to the long double ABI
     * (double off x87). No in-tree callers; kept faithful for completeness. */
    x87f value = coduo_pc_load_token_float80(token.floatValue);
    return (long double)x87f_store_f64(x87f_mul(sign, value));
#else
    long double value = 0.0L;
    size_t valueSize = sizeof(value);
    if (PC_TOKEN_FLOAT_VALUE_SIZE < valueSize) {
        valueSize = PC_TOKEN_FLOAT_VALUE_SIZE;
    }
    memcpy(&value, token.floatValue, valueSize);
    return sign * value;
#endif
}
#endif

/* Source: CoDUOMP.exe 0x00448780..0x00448847.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00448780_00448847.mcode.
 * Name: same-family symbol PS_ReadInteger. */
int32_t PS_ReadInteger(script_t *script)
{
    token_t token;
    int32_t sign = 1;

    (void)PS_ReadTokenOrError(script, &token);
    if (strcmp(token.string, "-") == 0) {
        sign = -1;
        (void)PS_ExpectTokenType(script, PC_TOKEN_TYPE_NUMBER,
                                 PC_TOKEN_SUBTYPE_INTEGER, &token);
    } else if (token.type != PC_TOKEN_TYPE_NUMBER ||
               token.subtype == PC_TOKEN_SUBTYPE_FLOAT) {
        /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
        ScriptError(script, "expected integer value, found %s\n",
                    token.string);
    }

    return sign * token.intValue;
}

/* Source: CoDUOMP.exe 0x00448850..0x00448857.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00448850_00448857.mcode.
 * Name: same-family symbol SetScriptFlags. */
void SetScriptFlags(script_t *script, int32_t flags)
{
    script->flags = flags;
}

/* Source: CoDUOMP.exe 0x00448860..0x00448867.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00448860_00448867.mcode.
 * Name: same-family symbol GetScriptFlags. */
int32_t GetScriptFlags(const script_t *script)
{
    return script->flags;
}

/* Source: CoDUOMP.exe 0x00448870..0x004488b9.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00448870_004488b9.mcode.
 * Name: same-family symbol ResetScript. */
void ResetScript(script_t *script)
{
    script->scriptCursor = script->buffer;
    script->lastScriptCursor = script->buffer;
    script->whitespaceStart = NULL;
    script->whitespaceEnd = NULL;
    script->tokenAvailable = qfalse;
    script->line = 1;
    script->lastLine = 1;
    memset(&script->token, 0, sizeof(script->token));
}

/* Source: CoDUOMP.exe 0x004488c0..0x004488d0.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_004488c0_004488d0.mcode.
 * Name: exact same-module Mac symbol EndOfScript. */
qboolean EndOfScript(script_t *script)
{
    return script->scriptCursor >= script->endCursor ? qtrue : qfalse;
}

/* Source: CoDUOMP.exe 0x004488d0..0x004488dd.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_004488d0_004488dd.mcode.
 * Name: same-family symbol PS_LinesCrossed. */
int32_t PS_LinesCrossed(const script_t *script)
{
    return script->line - script->lastLine;
}

/* Source: CoDUOMP.exe 0x004488e0..0x00448944.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_004488e0_00448944.mcode.
 * Name: same-family symbol PS_FindStringInScript. */
qboolean PS_FindStringInScript(script_t *script, const char *string)
{
    const char first = string[0];
    const size_t length = strlen(string);

    while (PS_ReadWhiteSpace(script) != qfalse) {
        if (*script->scriptCursor == first &&
            strncmp(script->scriptCursor, string, length) == 0) {
            return qtrue;
        }
        ++script->scriptCursor;
    }
    return qfalse;
}
