#include "core_runtime_private.h"
#include "compat/coduo_x87emu.h" /* defines EMULATE_X87; x87 shim when it is 1 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/*
 * Small engine name/value variable node used by the openlog helper family.
 *
 * Evidence:
 * - The allocator at VA 0x8077a61 reserves strlen(name) + 0x19 bytes, clears
 *   the fixed 0x18-byte prefix, stores name at +0x00 as node + 0x18, and
 *   links nodes through +0x14.
 * - Lookup/get/set helpers at VA 0x8077b40..0x8077d83 use stringValue +0x04,
 *   modified +0x0c, floatValue +0x10, and next +0x14.
 */
typedef struct com_name_value_s {
    char *name;
    char *stringValue;
    uint32_t reserved08;
    qboolean modified;
    float floatValue;
    struct com_name_value_s *next;
    char inlineName[];
} com_name_value_t;

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(com_name_value_t *) == 4,
               "com_name_value_t pointer width changed");
_Static_assert(sizeof(com_name_value_t) == 0x18,
               "com_name_value_t extent changed");
_Static_assert(offsetof(com_name_value_t, name) == 0x00,
               "com_name_value_t.name moved");
_Static_assert(offsetof(com_name_value_t, stringValue) == 0x04,
               "com_name_value_t.stringValue moved");
_Static_assert(offsetof(com_name_value_t, reserved08) == 0x08,
               "com_name_value_t.reserved08 moved");
_Static_assert(offsetof(com_name_value_t, modified) == 0x0c,
               "com_name_value_t.modified moved");
_Static_assert(offsetof(com_name_value_t, floatValue) == 0x10,
               "com_name_value_t.floatValue moved");
_Static_assert(offsetof(com_name_value_t, next) == 0x14,
               "com_name_value_t.next moved");
_Static_assert(offsetof(com_name_value_t, inlineName) == 0x18,
               "com_name_value_t.inlineName moved");
#endif

enum {
    COM_NAME_VALUE_DECIMAL_BASE = 10,
    COM_NAME_VALUE_DIGIT_ZERO = '0',
    COM_NAME_VALUE_DIGIT_NINE = '9',
    COM_NAME_VALUE_DECIMAL_POINT = '.',
    COM_NAME_VALUE_HEADER_EXTRA_NUL = 1
};

#define COM_DEBUG_ZONE_ALLOC_MAGIC UINT32_C(0x12345678)
#define COM_DEBUG_HUNK_ALLOC_MAGIC UINT32_C(0x87654321)

typedef struct com_debug_alloc_header_s {
    uint32_t magic;
#if UINTPTR_MAX > UINT32_MAX
    /* NATIVE64_ADAPTATION: stock advances four bytes on i386.  A four-byte
     * prefix misaligns the pointer-bearing records returned by these allocators
     * on 64-bit hosts, so retain one pointer-width prefix there. */
    uint32_t nativePointerAlignmentPadding;
#endif
    uint8_t payload[];
} com_debug_alloc_header_t;

com_name_value_t *com_nameValueList;
char com_openLogFilename[MAX_STRING_CHARS];
FILE *com_openLogFile;

float Com_ParseNameValueFloat(const char *text)
{
    float value = 0.0f;
    int32_t divisor = 0;

    while (*text != '\0') {
        if (*text < COM_NAME_VALUE_DIGIT_ZERO ||
            *text > COM_NAME_VALUE_DIGIT_NINE) {
            if (divisor == 0 && *text == COM_NAME_VALUE_DECIMAL_POINT) {
                divisor = COM_NAME_VALUE_DECIMAL_BASE;
                ++text;
                continue;
            }

            return 0.0f;
        }

        int32_t digit = *text - COM_NAME_VALUE_DIGIT_ZERO;
        if (divisor != 0) {
            /* value + fild(digit)/fild(divisor), one float store -> shim. */
#if EMULATE_X87
            value = x87f_store_f32(x87f_add(
                x87f_load_f32(value),
                x87f_div(x87f_load_i32(digit), x87f_load_i32(divisor))));
#else
            value = (float)((long double)value +
                            ((long double)digit / (long double)divisor));
#endif
            /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
            divisor *= COM_NAME_VALUE_DECIMAL_BASE;
        } else {
            /* value * 10.0(double) + fild(digit), one float store -> shim. */
#if EMULATE_X87
            value = x87f_store_f32(x87f_add(
                x87f_mul(x87f_load_f32(value),
                         x87f_load_f64((double)COM_NAME_VALUE_DECIMAL_BASE)),
                x87f_load_i32(digit)));
#else
            value = (float)(((long double)value *
                              (double)COM_NAME_VALUE_DECIMAL_BASE) +
                             (long double)digit);
#endif
        }

        ++text;
    }

    return value;
}

void *Com_ZoneDebugAlloc(size_t size)
{
    com_debug_alloc_header_t *header;

    header = Z_MallocInternal(sizeof(*header) + size);
    if (header == NULL) {
        return NULL;
    }

    header->magic = COM_DEBUG_ZONE_ALLOC_MAGIC;
    return header->payload;
}

void *Com_ZoneDebugAllocClear(size_t size)
{
    void *ptr = Com_ZoneDebugAlloc(size);

    memset(ptr, 0, size);
    return ptr;
}

void *Com_HunkDebugAlloc(size_t size)
{
    com_debug_alloc_header_t *header;

    header = Hunk_AllocInternal(sizeof(*header) + size);
    if (header == NULL) {
        return NULL;
    }

    header->magic = COM_DEBUG_HUNK_ALLOC_MAGIC;
    return header->payload;
}

void *Com_HunkDebugAllocClear(size_t size)
{
    void *ptr = Com_HunkDebugAlloc(size);

    memset(ptr, 0, size);
    return ptr;
}

void Com_DebugFree(void *ptr)
{
    com_debug_alloc_header_t *header;

    header = ((com_debug_alloc_header_t *)ptr) - 1;
    if (header->magic == COM_DEBUG_ZONE_ALLOC_MAGIC) {
        Z_FreeInternal(header);
    }
}

com_name_value_t *Com_AllocNameValue(const char *name)
{
    size_t nameLength = strlen(name) + COM_NAME_VALUE_HEADER_EXTRA_NUL;
    com_name_value_t *entry =
        Com_ZoneDebugAlloc(sizeof(*entry) + nameLength);

    memset(entry, 0, sizeof(*entry));
    entry->name = entry->inlineName;
    strcpy(entry->name, name);
    entry->next = com_nameValueList;
    com_nameValueList = entry;

    return entry;
}

void Com_FreeNameValue(com_name_value_t *entry)
{
    if (entry->stringValue != NULL) {
        Com_DebugFree(entry->stringValue);
    }

    Com_DebugFree(entry);
}

void Com_ClearNameValues(void)
{
    com_name_value_t *entry = com_nameValueList;

    while (entry != NULL) {
        com_nameValueList = com_nameValueList->next;
        Com_FreeNameValue(entry);
        entry = com_nameValueList;
    }

    com_nameValueList = NULL;
}

com_name_value_t *Com_FindNameValue(const char *name)
{
    for (com_name_value_t *entry = com_nameValueList; entry != NULL;
         entry = entry->next) {
        if (Q_stricmp(entry->name, name) == 0) {
            return entry;
        }
    }

    return NULL;
}

const char *Com_GetNameValueString(const char *name)
{
    com_name_value_t *entry = Com_FindNameValue(name);

    if (entry != NULL) {
        return entry->stringValue;
    }

    return "";
}

float Com_GetNameValueFloat(const char *name)
{
    com_name_value_t *entry = Com_FindNameValue(name);

    if (entry != NULL) {
        return entry->floatValue;
    }

    return 0.0f;
}

com_name_value_t *Com_GetOrCreateNameValue(const char *name,
                                                    const char *value)
{
    com_name_value_t *entry = Com_FindNameValue(name);

    if (entry != NULL) {
        return entry;
    }

    entry = Com_AllocNameValue(name);
    entry->stringValue =
        Com_ZoneDebugAlloc(strlen(value) + 1);
    strcpy(entry->stringValue, value);
    entry->floatValue = Com_ParseNameValueFloat(entry->stringValue);
    entry->modified = qtrue;

    return entry;
}

const char *Com_GetOrCreateNameValueString(const char *name,
                                           const char *value)
{
    com_name_value_t *entry =
        Com_GetOrCreateNameValue(name, value);

    return entry->stringValue;
}

float Com_GetOrCreateNameValueFloat(const char *name, const char *value)
{
    com_name_value_t *entry =
        Com_GetOrCreateNameValue(name, value);

    return entry->floatValue;
}

void Com_SetNameValueString(const char *name, const char *value)
{
    com_name_value_t *entry = Com_FindNameValue(name);

    if (entry != NULL) {
        Com_DebugFree(entry->stringValue);
    } else {
        entry = Com_AllocNameValue(name);
    }

    entry->stringValue =
        Com_ZoneDebugAlloc(strlen(value) + 1);
    strcpy(entry->stringValue, value);
    entry->floatValue = Com_ParseNameValueFloat(entry->stringValue);
    entry->modified = qtrue;
}

qboolean Com_GetNameValueModified(const char *name)
{
    com_name_value_t *entry = Com_FindNameValue(name);

    if (entry != NULL) {
        return entry->modified;
    }

    return qfalse;
}

void Com_ClearNameValueModified(const char *name)
{
    com_name_value_t *entry = Com_FindNameValue(name);

    if (entry != NULL) {
        entry->modified = qfalse;
    }
}

void Com_OpenLog(const char *filename)
{
    if (filename == NULL || filename[0] == '\0') {
        Com_Printf("openlog <filename>\n");
        return;
    }

    if (com_openLogFile != NULL) {
        Com_Printf("^1Error: log file %s is already opened\n",
                   com_openLogFilename);
        return;
    }

    com_openLogFile = fopen(filename, "wb");
    if (com_openLogFile == NULL) {
        Com_Printf("^1Error: can't open the log file %s\n", filename);
        return;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    Q_strncpyz(com_openLogFilename, filename, sizeof(com_openLogFilename));
    Com_Printf("Opened log %s\n", com_openLogFilename);
}

void Com_OpenLogIfEnabled(const char *filename)
{
    if (Com_GetOrCreateNameValueFloat("log", "0") != 0.0f) {
        Com_OpenLog(filename);
    }
}

void Com_CloseLog(void)
{
    if (com_openLogFile == NULL) {
        return;
    }

    if (fclose(com_openLogFile) != 0) {
        Com_Printf("^1Error: can't close log file %s\n", com_openLogFilename);
        return;
    }

    com_openLogFile = NULL;
    Com_Printf("Closed log %s\n", com_openLogFilename);
}

void Com_CloseLogIfOpen(void)
{
    if (com_openLogFile != NULL) {
        Com_CloseLog();
    }
}

void Com_LogPrintf(const char *format, ...)
{
    va_list ap;

    if (com_openLogFile == NULL) {
        return;
    }

    va_start(ap, format);
    vfprintf(com_openLogFile, format, ap);
    va_end(ap);
    fflush(com_openLogFile);
}

FILE *Com_LogFile(void)
{
    return com_openLogFile;
}

void Com_FlushLog(void)
{
    if (com_openLogFile != NULL) {
        fflush(com_openLogFile);
    }
}
