#ifndef CODUOMP_BOTLIB_LIBVAR_H
#define CODUOMP_BOTLIB_LIBVAR_H

#include <stddef.h>
#include <stdint.h>

#include "../q_shared.h"

typedef struct libvar_s {
    char *name;
    char *string;
    int32_t unused08; /* Original +0x08; zero-initialized by LibVarAlloc but
                       * never otherwise read or written by CoDUOMP.exe. */
    qboolean modified;
    float value;
    struct libvar_s *next;
} libvar_t;

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(_Alignof(libvar_t) == 4, "i386 libvar alignment changed");
_Static_assert(offsetof(libvar_t, name) == 0x00, "original libvar name offset");
_Static_assert(offsetof(libvar_t, string) == 0x04, "original libvar string offset");
_Static_assert(offsetof(libvar_t, unused08) == 0x08, "original libvar unused lane offset");
_Static_assert(offsetof(libvar_t, modified) == 0x0c, "original libvar modified offset");
_Static_assert(offsetof(libvar_t, value) == 0x10, "original libvar value offset");
_Static_assert(offsetof(libvar_t, next) == 0x14, "original libvar next offset");
_Static_assert(sizeof(libvar_t) == 0x18, "original libvar extent");
#endif

#ifdef __cplusplus
extern "C" {
#endif

float LibVarStringValue(const char *string);
libvar_t *LibVarGet(const char *name);
const char *LibVarGetString(const char *name);
float LibVarGetValue(const char *name);
libvar_t *LibVar(const char *name, const char *defaultValue);
const char *LibVarString(const char *name, const char *defaultValue);
float LibVarValue(const char *name, const char *defaultValue);
void LibVarSet(const char *name, const char *value);
qboolean LibVarChanged(const char *name);
void LibVarSetNotModified(const char *name);
void LibVarDeAllocAll(void);

#ifdef __cplusplus
}
#endif

#endif
