#include "../module/ui_functions.h"
#include "client/common/client_legacy_crt.h"

#include <string.h>

enum {
    UI_COMPAT_BINDING_TEXT_SIZE = 128,
    UI_COMPAT_KEY_NAME_SIZE = 32,
    UI_KEY_CONSOLE = 96,
    UI_KEY_MOUSE1 = 200,
    UI_KEY_MWHEELUP = 206
};

#define UI_KEY_UNBOUND ((int32_t)-1)


/* NOT_FROM_ORIGINAL_SOURCE: restore module load state for the shared retail
 * table and, when enabled, the separate UI-only console row. */
void ui_compat_reset_control_binding_state(void)
{
    int32_t index;

    for (index = 0; index < CONTROL_BINDING_COUNT; ++index) {
        g_bindings[index].bind1 = UI_KEY_UNBOUND;
        g_bindings[index].bind2 = 0;
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: refresh the retail table through its original
 * function, then refresh the optional UI-only row. */
void ui_compat_controls_get_config(void)
{
    Controls_GetConfig();
}

/* NOT_FROM_ORIGINAL_SOURCE: apply the optional UI-only console row, then commit
 * the retail table so its original in_restart command remains the final action. */
void client_ui_compat_controls_set_config(void)
{
    Controls_SetConfig();
}

/* NOT_FROM_ORIGINAL_SOURCE: retain the original reset bug for the retail
 * rows and reset the non-original console row to its unbound default. */
void ui_compat_controls_set_defaults(void)
{
    Controls_SetDefaults();
}

/* NOT_FROM_ORIGINAL_SOURCE: expose exactly one target-private row without
 * changing BindingIDFromName's retail index domain. */
bind_t *client_ui_compat_extra_binding_for_name(const char *command)
{
    (void)command;
    return NULL;
}

/* NOT_FROM_ORIGINAL_SOURCE: extend the common handler's original duplicate-key
 * removal to the optional target-private console row. */
void client_ui_compat_remove_key_from_extra_bindings(int32_t key)
{
    (void)key;
}

/* NOT_FROM_ORIGINAL_SOURCE: render the optional console row using the same UI
 * presentation as BindingFromName while leaving the original function stock. */
const char *client_ui_compat_binding_from_name(const char *command,
                                               qboolean firstKeyOnly)
{
    return BindingFromName(command, firstKeyOnly);
}

/* NOT_FROM_ORIGINAL_SOURCE: ask the engine to forward the console key while
 * the optional console-binding row is capturing it. */
void client_ui_compat_bind_capture_started(itemDef_t *item)
{
    (void)item;
}

/* NOT_FROM_ORIGINAL_SOURCE: stop the engine-side console-key forwarding after
 * a binding is accepted or capture is cancelled. */
void client_ui_compat_bind_capture_finished(void)
{
}

/* NOT_FROM_ORIGINAL_SOURCE: the optional console row accepts the console key
 * but rejects pointer buttons and wheel input. All stock rows retain the
 * original console-key rejection. */
qboolean client_ui_compat_bind_key_is_ignored(itemDef_t *item, int32_t key)
{
    (void)item;
    return key == UI_KEY_CONSOLE ? qtrue : qfalse;
}
