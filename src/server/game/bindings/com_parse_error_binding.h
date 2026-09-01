#ifndef QCOMMON_COM_PARSE_ERROR_BINDING_H
#define QCOMMON_COM_PARSE_ERROR_BINDING_H

/*
 * ORIGINAL_PLATFORM_DIFFERENCE: the Windows game-module Com parser routes
 * fatal diagnostics directly through G_Error and warnings through G_Printf.
 * The Linux game module uses Com_Error/Com_Printf like the other parser
 * bodies. Preserve that original dependency edge without splitting the
 * parser implementation.
 */
#if defined(WINDOWS_BEHAVIOR)
void G_Error(const char *format, ...);
void G_Printf(const char *format, ...);
#define COM_PARSE_ERROR(level, ...) G_Error(__VA_ARGS__)
#define COM_PARSE_PRINT(...) G_Printf(__VA_ARGS__)
#else
#define COM_PARSE_ERROR(level, ...) Com_Error((level), __VA_ARGS__)
#define COM_PARSE_PRINT(...) Com_Printf(__VA_ARGS__)
#endif

#endif
