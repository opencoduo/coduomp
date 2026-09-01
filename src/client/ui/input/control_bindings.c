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

/* NOT_FROM_ORIGINAL_SOURCE: the native UI adds one configurable console row
 * outside the retail 55-row g_bindings table owned by src/client/menu. */
bind_t ui_compat_consoleBinding = {
    "toggleconsole", { -1, -1, -1 }, -1, -1
};

static char ui_compat_consoleBindingText[UI_COMPAT_BINDING_TEXT_SIZE];
static char ui_compat_consoleSecondBindingText[UI_COMPAT_BINDING_TEXT_SIZE];

/* NOT_FROM_ORIGINAL_SOURCE: recognize the one UI-only compatibility row. */
static qboolean ui_compat_is_console_binding(const char *command)
{
    return command != NULL &&
           Q_stricmp(command, ui_compat_consoleBinding.command) == 0
               ? qtrue : qfalse;
}

/* NOT_FROM_ORIGINAL_SOURCE: restore module load state for the shared retail
 * table and, when enabled, the separate UI-only console row. */
void ui_compat_reset_control_binding_state(void)
{
    int32_t index;

    for (index = 0; index < CONTROL_BINDING_COUNT; ++index) {
        g_bindings[index].bind1 = UI_KEY_UNBOUND;
        g_bindings[index].bind2 = 0;
    }
    ui_compat_consoleBinding.bind1 = UI_KEY_UNBOUND;
    ui_compat_consoleBinding.bind2 = UI_KEY_UNBOUND;
}

/* NOT_FROM_ORIGINAL_SOURCE: refresh the retail table through its original
 * function, then refresh the optional UI-only row. */
void ui_compat_controls_get_config(void)
{
    Controls_GetConfig();
    {
        int32_t keys[2];

        Controls_GetKeyAssignment(ui_compat_consoleBinding.command, keys);
        ui_compat_consoleBinding.bind1 = keys[0];
        ui_compat_consoleBinding.bind2 = keys[1];
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: apply the optional UI-only console row, then commit
 * the retail table so its original in_restart command remains the final action. */
void client_ui_compat_controls_set_config(void)
{
    if (ui_compat_consoleBinding.bind1 != UI_KEY_UNBOUND) {
        DC->setBinding(ui_compat_consoleBinding.bind1,
                       ui_compat_consoleBinding.command);
        if (ui_compat_consoleBinding.bind2 != UI_KEY_UNBOUND) {
            DC->setBinding(ui_compat_consoleBinding.bind2,
                           ui_compat_consoleBinding.command);
        }
    }
    Controls_SetConfig();
}

/* NOT_FROM_ORIGINAL_SOURCE: retain the original reset bug for the retail
 * rows and reset the non-original console row to its unbound default. */
void ui_compat_controls_set_defaults(void)
{
    Controls_SetDefaults();
    ui_compat_consoleBinding.bind1 = UI_KEY_UNBOUND;
    ui_compat_consoleBinding.bind2 = UI_KEY_UNBOUND;
}

/* NOT_FROM_ORIGINAL_SOURCE: expose exactly one target-private row without
 * changing BindingIDFromName's retail index domain. */
bind_t *client_ui_compat_extra_binding_for_name(const char *command)
{
    if (ui_compat_is_console_binding(command) != qfalse) {
        return &ui_compat_consoleBinding;
    }
    return NULL;
}

/* NOT_FROM_ORIGINAL_SOURCE: extend the common handler's original duplicate-key
 * removal to the optional target-private console row. */
void client_ui_compat_remove_key_from_extra_bindings(int32_t key)
{
    if (ui_compat_consoleBinding.bind2 == key) {
        ui_compat_consoleBinding.bind2 = UI_KEY_UNBOUND;
    }
    if (ui_compat_consoleBinding.bind1 == key) {
        ui_compat_consoleBinding.bind1 = ui_compat_consoleBinding.bind2;
        ui_compat_consoleBinding.bind2 = UI_KEY_UNBOUND;
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: render the optional console row using the same UI
 * presentation as BindingFromName while leaving the original function stock. */
const char *client_ui_compat_binding_from_name(const char *command,
                                               qboolean firstKeyOnly)
{
    bind_t *binding;

    if (ui_compat_is_console_binding(command) != qfalse) {
        binding = &ui_compat_consoleBinding;
        if (binding->bind1 == UI_KEY_UNBOUND) {
            coduo_client_crt_strcpy(
                ui_compat_consoleBindingText,
                DC->getLocalizedString("KEY_UNBOUND"));
            return ui_compat_consoleBindingText;
        }

        DC->keynumToStringBuf(binding->bind1,
                              ui_compat_consoleBindingText,
                              UI_COMPAT_KEY_NAME_SIZE);
        coduo_client_crt_strcpy(
            ui_compat_consoleBindingText,
            DC->getLocalizedString(ui_compat_consoleBindingText));
        if (binding->bind2 == UI_KEY_UNBOUND || firstKeyOnly != qfalse) {
            return ui_compat_consoleBindingText;
        }

        DC->keynumToStringBuf(binding->bind2,
                              ui_compat_consoleSecondBindingText,
                              UI_COMPAT_KEY_NAME_SIZE);
        coduo_client_crt_strcpy(
            ui_compat_consoleSecondBindingText,
            DC->getLocalizedString(ui_compat_consoleSecondBindingText));
        strcat(ui_compat_consoleBindingText,
               va(" %s ", DC->getLocalizedString("KEY_OR")));
        strcat(ui_compat_consoleBindingText,
               ui_compat_consoleSecondBindingText);
        return ui_compat_consoleBindingText;
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
