#include "../module/ui_functions.h"
#include "client/common/client_legacy_crt.h"

#include <string.h>

enum {
    UI_COMPAT_BINDING_TEXT_SIZE = 128,
    UI_COMPAT_KEY_NAME_SIZE = 32,
    UI_COMPAT_BINDING_CONSOLE = 0,
    UI_COMPAT_BINDING_TOGGLE_RECORD = 1,
    UI_COMPAT_BINDING_COUNT = 2,
    UI_KEY_CONSOLE = 96,
    UI_KEY_MOUSE1 = 200,
    UI_KEY_MWHEELUP = 206
};

#define UI_KEY_UNBOUND ((int32_t)-1)

/* NOT_FROM_ORIGINAL_SOURCE: the native UI adds improved configurable actions
 * outside the retail 55-row g_bindings table owned by src/client/menu. */
static bind_t ui_compat_bindings[UI_COMPAT_BINDING_COUNT] = {
    [UI_COMPAT_BINDING_CONSOLE] =
        { "toggleconsole", { -1, -1, -1 }, -1, -1 },
    [UI_COMPAT_BINDING_TOGGLE_RECORD] =
        { "togglerecord", { -1, -1, -1 }, -1, -1 }
};

static char ui_compat_bindingText[UI_COMPAT_BINDING_COUNT]
                                 [UI_COMPAT_BINDING_TEXT_SIZE];
static char ui_compat_secondBindingText[UI_COMPAT_BINDING_COUNT]
                                       [UI_COMPAT_BINDING_TEXT_SIZE];

/* NOT_FROM_ORIGINAL_SOURCE: locate an improved binding without expanding the
 * original shared table's recovered index domain. */
static int32_t ui_compat_binding_index_for_name(const char *command)
{
    if (command == NULL)
        return -1;

    for (int32_t index = 0; index < UI_COMPAT_BINDING_COUNT; ++index) {
        if (Q_stricmp(command, ui_compat_bindings[index].command) == 0)
            return index;
    }
    return -1;
}

/* NOT_FROM_ORIGINAL_SOURCE: recognize the console row whose key-capture rules
 * intentionally differ from all other controls. */
static qboolean ui_compat_is_console_binding(const char *command)
{
    return command != NULL && Q_stricmp(
               command,
               ui_compat_bindings[UI_COMPAT_BINDING_CONSOLE].command) == 0
               ? qtrue : qfalse;
}

/* NOT_FROM_ORIGINAL_SOURCE: restore module load state for the shared retail
 * table and the separate improved binding rows. */
void ui_compat_reset_control_binding_state(void)
{
    int32_t index;

    for (index = 0; index < CONTROL_BINDING_COUNT; ++index) {
        g_bindings[index].bind1 = UI_KEY_UNBOUND;
        g_bindings[index].bind2 = 0;
    }
    for (index = 0; index < UI_COMPAT_BINDING_COUNT; ++index) {
        ui_compat_bindings[index].bind1 = UI_KEY_UNBOUND;
        ui_compat_bindings[index].bind2 = UI_KEY_UNBOUND;
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: refresh the retail table through its original
 * function, then refresh the improved UI-only rows. */
void ui_compat_controls_get_config(void)
{
    Controls_GetConfig();
    for (int32_t index = 0; index < UI_COMPAT_BINDING_COUNT; ++index) {
        int32_t keys[2];

        Controls_GetKeyAssignment(ui_compat_bindings[index].command, keys);
        ui_compat_bindings[index].bind1 = keys[0];
        ui_compat_bindings[index].bind2 = keys[1];
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: apply the improved UI-only rows, then commit the
 * retail table so its original in_restart command remains the final action. */
void client_ui_compat_controls_set_config(void)
{
    for (int32_t index = 0; index < UI_COMPAT_BINDING_COUNT; ++index) {
        bind_t *const binding = &ui_compat_bindings[index];

        if (binding->bind1 != UI_KEY_UNBOUND) {
            DC->setBinding(binding->bind1, binding->command);
            if (binding->bind2 != UI_KEY_UNBOUND)
                DC->setBinding(binding->bind2, binding->command);
        }
    }
    Controls_SetConfig();
}

/* NOT_FROM_ORIGINAL_SOURCE: retain the original reset bug for the retail
 * rows and reset the non-original improved rows to their unbound defaults. */
void ui_compat_controls_set_defaults(void)
{
    Controls_SetDefaults();
    for (int32_t index = 0; index < UI_COMPAT_BINDING_COUNT; ++index) {
        ui_compat_bindings[index].bind1 = UI_KEY_UNBOUND;
        ui_compat_bindings[index].bind2 = UI_KEY_UNBOUND;
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: expose target-private rows without changing
 * BindingIDFromName's retail index domain. */
bind_t *client_ui_compat_extra_binding_for_name(const char *command)
{
    const int32_t index = ui_compat_binding_index_for_name(command);

    return index >= 0 ? &ui_compat_bindings[index] : NULL;
}

/* NOT_FROM_ORIGINAL_SOURCE: extend the common handler's original duplicate-key
 * removal to every improved target-private row. */
void client_ui_compat_remove_key_from_extra_bindings(int32_t key)
{
    for (int32_t index = 0; index < UI_COMPAT_BINDING_COUNT; ++index) {
        bind_t *const binding = &ui_compat_bindings[index];

        if (binding->bind2 == key)
            binding->bind2 = UI_KEY_UNBOUND;
        if (binding->bind1 == key) {
            binding->bind1 = binding->bind2;
            binding->bind2 = UI_KEY_UNBOUND;
        }
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: render improved rows using the same UI presentation
 * as BindingFromName while leaving the original function stock. */
const char *client_ui_compat_binding_from_name(const char *command,
                                               qboolean firstKeyOnly)
{
    const int32_t index = ui_compat_binding_index_for_name(command);
    bind_t *binding;
    char *bindingText;
    char *secondBindingText;

    if (index >= 0) {
        binding = &ui_compat_bindings[index];
        bindingText = ui_compat_bindingText[index];
        secondBindingText = ui_compat_secondBindingText[index];
        if (binding->bind1 == UI_KEY_UNBOUND) {
            coduo_client_crt_strcpy(
                bindingText,
                DC->getLocalizedString("KEY_UNBOUND"));
            return bindingText;
        }

        DC->keynumToStringBuf(binding->bind1,
                              bindingText,
                              UI_COMPAT_KEY_NAME_SIZE);
        coduo_client_crt_strcpy(
            bindingText, DC->getLocalizedString(bindingText));
        if (binding->bind2 == UI_KEY_UNBOUND || firstKeyOnly != qfalse) {
            return bindingText;
        }

        DC->keynumToStringBuf(binding->bind2,
                              secondBindingText,
                              UI_COMPAT_KEY_NAME_SIZE);
        coduo_client_crt_strcpy(
            secondBindingText,
            DC->getLocalizedString(secondBindingText));
        strcat(bindingText,
               va(" %s ", DC->getLocalizedString("KEY_OR")));
        strcat(bindingText, secondBindingText);
        return bindingText;
    }
    return BindingFromName(command, firstKeyOnly);
}

/* NOT_FROM_ORIGINAL_SOURCE: ask the engine to forward the console key while
 * the optional console-binding row is capturing it. */
void client_ui_compat_bind_capture_started(itemDef_t *item)
{
    const qboolean isConsoleBinding =
        item != NULL &&
        ui_compat_is_console_binding(item->cvar) != qfalse;

    trap_Cvar_Set(UI_COMPAT_CONSOLE_BIND_CAPTURE_CVAR,
                  isConsoleBinding != qfalse ? "1" : "0");
}

/* NOT_FROM_ORIGINAL_SOURCE: stop the engine-side console-key forwarding after
 * a binding is accepted or capture is cancelled. */
void client_ui_compat_bind_capture_finished(void)
{
    trap_Cvar_Set(UI_COMPAT_CONSOLE_BIND_CAPTURE_CVAR, "0");
}

/* NOT_FROM_ORIGINAL_SOURCE: the optional console row accepts the console key
 * but rejects pointer buttons and wheel input. All stock rows retain the
 * original console-key rejection. */
qboolean client_ui_compat_bind_key_is_ignored(itemDef_t *item, int32_t key)
{
    const qboolean isConsoleBinding =
        item != NULL &&
        ui_compat_is_console_binding(item->cvar) != qfalse;

    if (isConsoleBinding != qfalse) {
        return key >= UI_KEY_MOUSE1 && key <= UI_KEY_MWHEELUP
                   ? qtrue : qfalse;
    }
    return key == UI_KEY_CONSOLE ? qtrue : qfalse;
}
