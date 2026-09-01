#include "libvar.h"

#include "memory.h"
#include "qcommon/q_string.h"

#include <string.h>

enum {
    LIBVAR_DECIMAL_RADIX = 10
};

/* Original list head at 0x04927e94. */
static libvar_t *libvarList;

/* Source: CoDUOMP.exe 0x004421d0..0x0044224f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004421d0_00442250.mcode.
 * Role name: the botlib LibVar decimal-string converter. The first byte after
 * a decimal point is consumed immediately, matching the original control
 * flow even when that byte is not a digit. */
float LibVarStringValue(const char *string)
{
    int32_t decimalDivisor = 0;
    float value = 0.0f;

    while (*string != '\0') {
        const int32_t character = (int8_t)*string;
        if (character < '0' || character > '9') {
            if (decimalDivisor != 0 || character != '.')
                return 0.0f;
            decimalDivisor = LIBVAR_DECIMAL_RADIX;
            /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
            ++string;
        }

        const int32_t digit = (int8_t)*string - '0';
        if (decimalDivisor != 0) {
            value += (float)digit / (float)decimalDivisor;
            decimalDivisor *= LIBVAR_DECIMAL_RADIX;
        } else {
            value = value * (float)LIBVAR_DECIMAL_RADIX + (float)digit;
        }
        ++string;
    }

    return value;
}

/* Source: CoDUOMP.exe 0x00442250..0x004422ca.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00442250_004422cb.mcode.
 * Role name: private LibVar record allocator. */
static libvar_t *LibVarAlloc(const char *name)
{
    libvar_t *variable = GetClearedMemory(
        sizeof(*variable) + strlen(name) + 1);
    variable->name = (char *)(variable + 1);
    strcpy(variable->name, name);
    variable->next = libvarList;
    libvarList = variable;
    return variable;
}

/* Source: CoDUOMP.exe 0x004422d0..0x00442303.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_004422d0_00442304.mcode.
 * Role name: private LibVar record destructor. Invalid allocation markers are
 * deliberately ignored instead of being passed to free. */
static void LibVarDeAlloc(libvar_t *variable)
{
    if (variable->string != NULL)
        FreeMemory(variable->string);
    FreeMemory(variable);
}

/* Source: CoDUOMP.exe 0x00442310..0x00442372.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00442310_00442373.mcode. */
void LibVarDeAllocAll(void)
{
    while (libvarList != NULL) {
        libvar_t *variable = libvarList;
        libvarList = variable->next;
        LibVarDeAlloc(variable);
    }
    libvarList = NULL;
}

/* Source: CoDUOMP.exe 0x00442380..0x004423b8.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00442380_004423b9.mcode.
 * Role name and comparison semantics: botlib LibVarGet. */
libvar_t *LibVarGet(const char *name)
{
    for (libvar_t *variable = libvarList; variable != NULL;
         variable = variable->next) {
        if (variable->name != NULL && name != NULL &&
            Q_stricmp(name, variable->name) == 0)
            return variable;
    }
    return NULL;
}

/* Source: CoDUOMP.exe 0x004423c0..0x004423d2.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_004423c0_004423d3.mcode. */
const char *LibVarGetString(const char *name)
{
    libvar_t *variable = LibVarGet(name);
    return variable != NULL ? variable->string : "";
}

/* Source: CoDUOMP.exe 0x004423e0..0x004423f3.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_004423e0_004423f4.mcode. */
float LibVarGetValue(const char *name)
{
    libvar_t *variable = LibVarGet(name);
    return variable != NULL ? variable->value : 0.0f;
}

/* Source: CoDUOMP.exe 0x00442400..0x00442482.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00442400_00442483.mcode.
 * Role name: register a LibVar with a default value, preserving an existing
 * record unchanged when the name is already present. */
libvar_t *LibVar(const char *name, const char *defaultValue)
{
    libvar_t *variable = LibVarGet(name);
    if (variable != NULL)
        return variable;

    variable = LibVarAlloc(name);
    variable->string = GetClearedMemory(strlen(defaultValue) + 1);
    strcpy(variable->string, defaultValue);
    variable->value = LibVarStringValue(variable->string);
    variable->modified = qtrue;
    return variable;
}

/* Source: CoDUOMP.exe 0x00442490..0x004424a0.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00442490_004424a1.mcode. */
const char *LibVarString(const char *name, const char *defaultValue)
{
    return LibVar(name, defaultValue)->string;
}

/* Source: CoDUOMP.exe 0x004424b0..0x004424c0.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_004424b0_004424c1.mcode. */
float LibVarValue(const char *name, const char *defaultValue)
{
    return LibVar(name, defaultValue)->value;
}

/* Source: CoDUOMP.exe 0x004424d0..0x00442570.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_004424d0_00442571.mcode. */
void LibVarSet(const char *name, const char *value)
{
    libvar_t *variable = LibVarGet(name);
    if (variable != NULL)
        FreeMemory(variable->string);
    else
        variable = LibVarAlloc(name);

    variable->string = GetClearedMemory(strlen(value) + 1);
    strcpy(variable->string, value);
    variable->value = LibVarStringValue(variable->string);
    variable->modified = qtrue;
}

/* Source: CoDUOMP.exe 0x00442580..0x0044258f.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00442580_00442590.mcode. */
qboolean LibVarChanged(const char *name)
{
    libvar_t *variable = LibVarGet(name);
    return variable != NULL ? variable->modified : qfalse;
}

/* Source: CoDUOMP.exe 0x00442590..0x004425a0.
 * Evidence: repaired executable-gap record
 * coduomp/mcode/CoDUOMP/FUN_00442590_004425a1.mcode. */
void LibVarSetNotModified(const char *name)
{
    libvar_t *variable = LibVarGet(name);
    if (variable != NULL)
        variable->modified = qfalse;
}
