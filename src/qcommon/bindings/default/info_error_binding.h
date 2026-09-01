#ifndef QCOMMON_INFO_ERROR_BINDING_H
#define QCOMMON_INFO_ERROR_BINDING_H

/* Shared qcommon dependency binding.  The Windows game module overrides this
 * header because its retained Info_* bodies call G_Error directly. */
#define INFO_ERROR(level, ...) Com_Error((level), __VA_ARGS__)

#endif
