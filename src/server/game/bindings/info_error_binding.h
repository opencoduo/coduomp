#ifndef QCOMMON_INFO_ERROR_BINDING_H
#define QCOMMON_INFO_ERROR_BINDING_H

/*
 * ORIGINAL_PLATFORM_DIFFERENCE: the Windows game-module Info_ValueForKey,
 * Info_RemoveKey, Info_RemoveKey_Big, and ParseConfigStringToStruct bodies call
 * G_Error directly.  The Linux game module calls Com_Error with level 1.
 * Preserve that dependency edge while keeping the complete subsystem shared.
 */
#if defined(WINDOWS_BEHAVIOR)
void G_Error(const char *format, ...);
#define INFO_ERROR(level, ...) G_Error(__VA_ARGS__)
#else
#define INFO_ERROR(level, ...) Com_Error((level), __VA_ARGS__)
#endif

#endif
