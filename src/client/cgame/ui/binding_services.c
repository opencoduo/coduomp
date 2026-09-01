#include "client/menu/ui_runtime.h"

#include <stddef.h>

enum {
    BIND_KEY_CONSOLE = 96
};

/* NOT_FROM_ORIGINAL_SOURCE: cgame owns only the original 55-row binding
 * table, so it has no target-private binding row to expose. */
bind_t *client_ui_compat_extra_binding_for_name(const char *command)
{
    (void)command;
    return NULL;
}

/* NOT_FROM_ORIGINAL_SOURCE: cgame has no target-private binding row from
 * which a duplicate key must be removed. */
void client_ui_compat_remove_key_from_extra_bindings(int32_t key)
{
    (void)key;
}

/* NOT_FROM_ORIGINAL_SOURCE: bridge the common binding handler to cgame's
 * original table commit routine. */
void client_ui_compat_controls_set_config(void)
{
    Controls_SetConfig();
}

/* NOT_FROM_ORIGINAL_SOURCE: cgame renders only the original binding table. */
const char *client_ui_compat_binding_from_name(const char *command,
                                               qboolean firstKeyOnly)
{
    return BindingFromName(command, firstKeyOnly);
}

/* NOT_FROM_ORIGINAL_SOURCE: cgame needs no engine-side capture override. */
void client_ui_compat_bind_capture_started(itemDef_t *item)
{
    (void)item;
}

/* NOT_FROM_ORIGINAL_SOURCE: cgame needs no engine-side capture override. */
void client_ui_compat_bind_capture_finished(void)
{
}

/* NOT_FROM_ORIGINAL_SOURCE: preserve the original handler's rejection of
 * the console key on cgame's stock binding rows. */
qboolean client_ui_compat_bind_key_is_ignored(itemDef_t *item, int32_t key)
{
    (void)item;
    return key == BIND_KEY_CONSOLE ? qtrue : qfalse;
}
