#ifndef QCOMMON_PRECOMPILER_TYPES_H
#define QCOMMON_PRECOMPILER_TYPES_H

#include <stddef.h>
#include <stdint.h>

#include "q_shared_types.h"

enum {
#if defined(WINDOWS_BEHAVIOR)
    PC_SCRIPT_FILENAME_CAPACITY = 260,
    PC_SOURCE_FILENAME_CAPACITY = 260,
    PC_SOURCE_INCLUDE_PATH_CAPACITY = 260,
#elif defined(LINUX_BEHAVIOR)
    PC_SCRIPT_FILENAME_CAPACITY = MAX_QPATH,
    PC_SOURCE_FILENAME_CAPACITY = MAX_QPATH,
    PC_SOURCE_INCLUDE_PATH_CAPACITY = MAX_QPATH,
#else
#error "precompiler_types.h requires WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif
    PC_DEFINE_HASH_BUCKET_COUNT = 1024,
    PC_SOURCE_HANDLE_COUNT = 64,
    PC_DEFAULT_PUNCTUATION_COUNT = 53,
    PC_PUNCTUATION_BUCKET_COUNT = 256,
    PC_TOKEN_FLOAT_VALUE_SIZE = 12,
    PC_DIAGNOSTIC_CAPACITY = 1024,
    PC_NUMBER_SUFFIX_CHECK_COUNT = 2,
    PC_DEFINE_FLAG_BUILTIN = 1,
    PC_DEFINE_MAX_PARMS = 128
};

enum pc_token_type_e {
    PC_TOKEN_TYPE_STRING = 1,
    PC_TOKEN_TYPE_LITERAL = 2,
    PC_TOKEN_TYPE_NUMBER = 3,
    PC_TOKEN_TYPE_NAME = 4,
    PC_TOKEN_TYPE_PUNCTUATION = 5
};

enum pc_builtin_define_e {
    PC_BUILTIN_NONE = 0,
    PC_BUILTIN_LINE = 1,
    PC_BUILTIN_FILE = 2,
    PC_BUILTIN_DATE = 3,
    PC_BUILTIN_TIME = 4
};

enum pc_indent_type_e {
    PC_INDENT_TYPE_NONE = 0,
    PC_INDENT_TYPE_IF = 1,
    PC_INDENT_TYPE_ELSE = 2,
    PC_INDENT_TYPE_ELIF = 4,
    PC_INDENT_TYPE_IFDEF = 8,
    PC_INDENT_TYPE_IFNDEF = 16
};

enum pc_punctuation_subtype_e {
    PC_PUNCTUATION_SHIFT_RIGHT_ASSIGN = 1,
    PC_PUNCTUATION_SHIFT_LEFT_ASSIGN = 2,
    PC_PUNCTUATION_ELLIPSIS = 3,
    PC_PUNCTUATION_TOKEN_PASTE = 4,
    PC_OPERATOR_LOGICAL_AND = 5,
    PC_OPERATOR_LOGICAL_OR = 6,
    PC_OPERATOR_GREATER_OR_EQUAL = 7,
    PC_OPERATOR_LESS_OR_EQUAL = 8,
    PC_OPERATOR_EQUAL = 9,
    PC_OPERATOR_NOT_EQUAL = 10,
    PC_PUNCTUATION_MULTIPLY_ASSIGN = 11,
    PC_PUNCTUATION_DIVIDE_ASSIGN = 12,
    PC_PUNCTUATION_MODULO_ASSIGN = 13,
    PC_PUNCTUATION_ADD_ASSIGN = 14,
    PC_PUNCTUATION_SUBTRACT_ASSIGN = 15,
    PC_OPERATOR_INCREMENT = 16,
    PC_OPERATOR_DECREMENT = 17,
    PC_PUNCTUATION_BITWISE_AND_ASSIGN = 18,
    PC_PUNCTUATION_BITWISE_OR_ASSIGN = 19,
    PC_PUNCTUATION_BITWISE_XOR_ASSIGN = 20,
    PC_OPERATOR_SHIFT_RIGHT = 21,
    PC_OPERATOR_SHIFT_LEFT = 22,
    PC_PUNCTUATION_POINTER_MEMBER = 23,
    PC_PUNCTUATION_SCOPE = 24,
    PC_PUNCTUATION_MEMBER_POINTER = 25,
    PC_OPERATOR_MULTIPLY = 26,
    PC_OPERATOR_DIVIDE = 27,
    PC_OPERATOR_MODULO = 28,
    PC_OPERATOR_ADD = 29,
    PC_OPERATOR_SUBTRACT = 30,
    PC_PUNCTUATION_ASSIGN = 31,
    PC_OPERATOR_BITWISE_AND = 32,
    PC_OPERATOR_BITWISE_OR = 33,
    PC_OPERATOR_BITWISE_XOR = 34,
    PC_OPERATOR_BITWISE_NOT = 35,
    PC_OPERATOR_LOGICAL_NOT = 36,
    PC_OPERATOR_GREATER = 37,
    PC_OPERATOR_LESS = 38,
    PC_PUNCTUATION_PERIOD = 39,
    PC_PUNCTUATION_COMMA = 40,
    PC_PUNCTUATION_SEMICOLON = 41,
    PC_OPERATOR_TERNARY_COLON = 42,
    PC_OPERATOR_TERNARY_QUESTION = 43,
    PC_OPERATOR_OPEN_PARENTHESIS = 44,
    PC_OPERATOR_CLOSE_PARENTHESIS = 45,
    PC_PUNCTUATION_OPEN_BRACE = 46,
    PC_PUNCTUATION_CLOSE_BRACE = 47,
    PC_PUNCTUATION_OPEN_BRACKET = 48,
    PC_PUNCTUATION_CLOSE_BRACKET = 49,
    PC_PUNCTUATION_BACKSLASH = 50,
    PC_PUNCTUATION_PREPROCESSOR = 51,
    PC_PUNCTUATION_DOLLAR = 52
};

enum pc_script_flag_e {
    PC_SCRIPT_FLAG_NO_ERRORS = 0x1,
    PC_SCRIPT_FLAG_NO_WARNINGS = 0x2,
    PC_SCRIPT_FLAG_NO_STRING_CONCAT = 0x4,
    PC_SCRIPT_FLAG_NO_STRING_ESCAPE_CHARS = 0x8,
    PC_SCRIPT_FLAG_PRIMITIVE = 0x10
};

enum pc_operator_priority_e {
    PC_OPERATOR_PRIORITY_NONE = 0,
    PC_OPERATOR_PRIORITY_TERNARY = 5,
    PC_OPERATOR_PRIORITY_LOGICAL_OR = 6,
    PC_OPERATOR_PRIORITY_LOGICAL_AND = 7,
    PC_OPERATOR_PRIORITY_BITWISE_OR = 8,
    PC_OPERATOR_PRIORITY_BITWISE_XOR = 9,
    PC_OPERATOR_PRIORITY_BITWISE_AND = 10,
    PC_OPERATOR_PRIORITY_EQUALITY = 11,
    PC_OPERATOR_PRIORITY_RELATIONAL = 12,
    PC_OPERATOR_PRIORITY_SHIFT = 13,
    PC_OPERATOR_PRIORITY_ADDITIVE = 14,
    PC_OPERATOR_PRIORITY_MULTIPLICATIVE = 15,
    PC_OPERATOR_PRIORITY_UNARY = 16
};

enum pc_number_subtype_e {
    PC_TOKEN_SUBTYPE_NONE = 0,
    PC_TOKEN_SUBTYPE_DECIMAL = 0x8,
    PC_TOKEN_SUBTYPE_HEX = 0x100,
    PC_TOKEN_SUBTYPE_OCTAL = 0x200,
    PC_TOKEN_SUBTYPE_BINARY = 0x400,
    PC_TOKEN_SUBTYPE_FLOAT = 0x800,
    PC_TOKEN_SUBTYPE_INTEGER = 0x1000,
    PC_TOKEN_SUBTYPE_LONG = 0x2000,
    PC_TOKEN_SUBTYPE_UNSIGNED = 0x4000,
    PC_TOKEN_SUBTYPE_DECIMAL_INTEGER =
        PC_TOKEN_SUBTYPE_DECIMAL | PC_TOKEN_SUBTYPE_INTEGER,
    PC_TOKEN_SUBTYPE_DECIMAL_FLOAT_LONG =
        PC_TOKEN_SUBTYPE_DECIMAL | PC_TOKEN_SUBTYPE_FLOAT |
        PC_TOKEN_SUBTYPE_LONG,
    PC_TOKEN_SUBTYPE_DECIMAL_INTEGER_LONG =
        PC_TOKEN_SUBTYPE_DECIMAL | PC_TOKEN_SUBTYPE_INTEGER |
        PC_TOKEN_SUBTYPE_LONG
};

typedef struct token_s token_t;
typedef struct punctuation_s punctuation_t;
typedef struct define_s define_t;
typedef struct script_s script_t;
typedef struct indent_s indent_t;
typedef struct source_s source_t;

/* These are the inherited Quake III l_script/l_precomp records.  The field
 * graph is common.  CoD's Windows build uses a 260-byte path domain and an
 * aligned binary64 token value; the Linux build uses 64-byte paths and stores
 * the original twelve-byte x87 long-double object representation. */
struct token_s {
    char string[MAX_TOKEN_CHARS];
    int32_t type;
    int32_t subtype;
    int32_t intValue;
#if defined(WINDOWS_BEHAVIOR)
#if defined(__cplusplus)
    alignas(8) double floatValue;
#else
    _Alignas(8) double floatValue;
#endif
#else
    uint8_t floatValue[PC_TOKEN_FLOAT_VALUE_SIZE];
#endif
    char *whitespaceStart;
    char *whitespaceEnd;
    int32_t line;
    int32_t linesCrossed;
    token_t *next;
};

struct punctuation_s {
    const char *text;
    int32_t subtype;
    punctuation_t *next;
};

struct script_s {
    char filename[PC_SCRIPT_FILENAME_CAPACITY];
    char *buffer;
    char *scriptCursor;
    char *endCursor;
    char *lastScriptCursor;
    char *whitespaceStart;
    char *whitespaceEnd;
    /* File/compressed length is stored but not read by either retained body. */
    int32_t length;
    int32_t line;
    int32_t lastLine;
    qboolean tokenAvailable;
    int32_t flags;
    punctuation_t *punctuations;
    punctuation_t **punctuationTable;
    token_t token;
    script_t *next;
};

struct indent_s {
    int32_t type;
    qboolean skip;
    script_t *script;
    indent_t *next;
};

struct define_s {
    char *name;
    int32_t flags;
    int32_t builtin;
    int32_t numParms;
    token_t *parms;
    token_t *tokens;
    define_t *next;
    define_t *hashNext;
    char nameStorage[];
};

struct source_s {
    char filename[PC_SOURCE_FILENAME_CAPACITY];
    char includePath[PC_SOURCE_INCLUDE_PATH_CAPACITY];
    /* Set by PC_SetPunctuations but not read by either retained body. */
    punctuation_t *punctuations;
    script_t *scriptStack;
    token_t *tokens;
    /* Checked by failure cleanup; no retained producer makes it non-NULL. */
    define_t *defines;
    define_t **defineHash;
    indent_t *indentStack;
    int32_t skip;
    token_t token;
};

#if UINTPTR_MAX == UINT32_MAX
#define PC_LAYOUT_ASSERT(name, expression) \
    typedef char name[(expression) ? 1 : -1]

PC_LAYOUT_ASSERT(pc_punctuation_text_offset,
                 offsetof(punctuation_t, text) == 0x00);
PC_LAYOUT_ASSERT(pc_punctuation_subtype_offset,
                 offsetof(punctuation_t, subtype) == 0x04);
PC_LAYOUT_ASSERT(pc_punctuation_size, sizeof(punctuation_t) == 0x0c);
PC_LAYOUT_ASSERT(pc_indent_size, sizeof(indent_t) == 0x10);
PC_LAYOUT_ASSERT(pc_define_name_storage_offset,
                 offsetof(define_t, nameStorage) == 0x20);
PC_LAYOUT_ASSERT(pc_define_size, sizeof(define_t) == 0x20);

#if defined(WINDOWS_BEHAVIOR)
PC_LAYOUT_ASSERT(pc_windows_token_float_offset,
                 offsetof(token_t, floatValue) == 0x410);
PC_LAYOUT_ASSERT(pc_windows_token_next_offset,
                 offsetof(token_t, next) == 0x428);
PC_LAYOUT_ASSERT(pc_windows_token_size, sizeof(token_t) == 0x430);
PC_LAYOUT_ASSERT(pc_windows_script_buffer_offset,
                 offsetof(script_t, buffer) == 0x104);
PC_LAYOUT_ASSERT(pc_windows_script_token_offset,
                 offsetof(script_t, token) == 0x138);
PC_LAYOUT_ASSERT(pc_windows_script_size, sizeof(script_t) == 0x570);
PC_LAYOUT_ASSERT(pc_windows_source_include_path_offset,
                 offsetof(source_t, includePath) == 0x104);
PC_LAYOUT_ASSERT(pc_windows_source_token_offset,
                 offsetof(source_t, token) == 0x228);
PC_LAYOUT_ASSERT(pc_windows_source_size, sizeof(source_t) == 0x658);
#else
PC_LAYOUT_ASSERT(pc_linux_token_float_offset,
                 offsetof(token_t, floatValue) == 0x40c);
PC_LAYOUT_ASSERT(pc_linux_token_next_offset,
                 offsetof(token_t, next) == 0x428);
PC_LAYOUT_ASSERT(pc_linux_token_size, sizeof(token_t) == 0x42c);
PC_LAYOUT_ASSERT(pc_linux_script_buffer_offset,
                 offsetof(script_t, buffer) == 0x040);
PC_LAYOUT_ASSERT(pc_linux_script_token_offset,
                 offsetof(script_t, token) == 0x074);
PC_LAYOUT_ASSERT(pc_linux_script_size, sizeof(script_t) == 0x4a4);
PC_LAYOUT_ASSERT(pc_linux_source_include_path_offset,
                 offsetof(source_t, includePath) == 0x040);
PC_LAYOUT_ASSERT(pc_linux_source_token_offset,
                 offsetof(source_t, token) == 0x09c);
PC_LAYOUT_ASSERT(pc_linux_source_size, sizeof(source_t) == 0x4c8);
#endif

#undef PC_LAYOUT_ASSERT
#endif

#endif
