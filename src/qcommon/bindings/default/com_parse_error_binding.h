#ifndef QCOMMON_COM_PARSE_ERROR_BINDING_H
#define QCOMMON_COM_PARSE_ERROR_BINDING_H

/* Shared qcommon dependency binding. The Windows game module overrides this
 * header because its parser bodies call G_Error/G_Printf directly. */
#define COM_PARSE_ERROR(level, ...) Com_Error((level), __VA_ARGS__)
#define COM_PARSE_PRINT(...) Com_Printf(__VA_ARGS__)

#endif
