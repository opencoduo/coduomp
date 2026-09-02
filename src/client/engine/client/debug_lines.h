#ifndef CODUOMP_CLIENT_DEBUG_LINES_H
#define CODUOMP_CLIENT_DEBUG_LINES_H

#include "../q_shared.h"

typedef struct client_debug_line_s {
    vec3_t start;
    vec3_t end;
    vec4_t color;
    qboolean depthTest;
} client_debug_line_t;

typedef struct client_debug_string_s {
    vec3_t origin;
    vec4_t color;
    float scale;
    char text[96];
} client_debug_string_t;

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(_Alignof(client_debug_line_t) == 4,
               "i386 client debug-line alignment changed");
_Static_assert(offsetof(client_debug_line_t, start) == 0x00,
               "i386 client debug-line start moved");
_Static_assert(offsetof(client_debug_line_t, end) == 0x0c,
               "i386 client debug-line end moved");
_Static_assert(offsetof(client_debug_line_t, color) == 0x18,
               "i386 client debug-line color moved");
_Static_assert(offsetof(client_debug_line_t, depthTest) == 0x28,
               "i386 client debug-line depth flag moved");
_Static_assert(sizeof(client_debug_line_t) == 0x2c,
               "i386 client debug-line size changed");
_Static_assert(_Alignof(client_debug_string_t) == 4,
               "i386 client debug-string alignment changed");
_Static_assert(offsetof(client_debug_string_t, origin) == 0x00,
               "i386 client debug-string origin moved");
_Static_assert(offsetof(client_debug_string_t, color) == 0x0c,
               "i386 client debug-string color moved");
_Static_assert(offsetof(client_debug_string_t, scale) == 0x1c,
               "i386 client debug-string scale moved");
_Static_assert(offsetof(client_debug_string_t, text) == 0x20,
               "i386 client debug-string text moved");
_Static_assert(sizeof(client_debug_string_t) == 0x80,
               "i386 client debug-string size changed");
#endif

void CL_AddDebugString(const vec3_t origin, const vec4_t color,
                       float scale, const char *text,
                       qboolean fromServer);
void CL_AddDebugLine(const vec3_t start, const vec3_t end,
                     const vec4_t color, qboolean depthTest,
                     int32_t duration, qboolean fromServer);
void CL_UpdateDebugData(void);
void CL_FlushDebugData(qboolean fromServer);

void RE_LocateDebugStrings(const client_debug_string_t *strings,
                           int32_t stringCount);
void RE_LocateDebugLines(const client_debug_line_t *lines,
                         int32_t lineCount);

#endif
