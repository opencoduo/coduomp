#include "ui_runtime.h"

#include "qcommon/q_string.h"

#include <string.h>

extern displayContextDef_t *DC;

enum {
    UI_BINDING_SCAN_BUFFER_SIZE = 256,
    UI_BINDING_TEXT_SIZE = 128,
    UI_BINDING_KEY_TEXT_SIZE = 32,
    UI_LOCALIZED_BINDING_TEXT_SIZE = 256,
    UI_KEYS_PER_BINDING = 2,
    UI_UNBOUNDED_STRING_COMPARE = 99999
};

#define UI_KEY_UNBOUND ((int32_t)-1)

/*
 * Complete stock controls-binding core shared by the two client DLLs.
 * Every retained Windows pair below agrees instruction for instruction apart
 * from relocations to the module's DC pointer, table, strings, and callees:
 *
 *                               cgame       UI
 * Controls_GetKeyAssignment     0x300566f0  0x40018250
 * Controls_GetConfig            0x30056790  0x400182f0
 * Controls_SetConfig            0x300567d0  0x40018330
 * Controls_SetDefaults          0x30056830  0x40018390
 * BindingIDFromName             0x30056850  0x400183b0
 * BindingFromName               0x300568a0  0x40018400
 * GetCommandHasBinding          0x30056a10  0x40018570
 * Key_GetKeysForCommand         0x30056a60  0x400185c0
 * UI_KeysStringForBinding       0x30056b40  0x400186a0
 *
 * The paired PE tables also contain the same 55 records in the same order,
 * with the same 0x18-byte i386 layout and initializer values.  Supporting Mac
 * symbols retain the canonical Controls_GetConfig, BindingFromName, and other
 * source names; these replace the cgame reconstruction-only
 * UI_UpdateKeyBindingCache, UI_FindBindingIndex, and CG_KeyBindingText names.
 */
bind_t g_bindings[CONTROL_BINDING_COUNT] = {
    { "+scores", { -1, -1, -1 }, -1, 0 },
    { "+speed", { 201, -1, -1 }, -1, 0 },
    { "+forward", { 119, -1, -1 }, -1, 0 },
    { "+back", { 115, -1, -1 }, -1, 0 },
    { "+moveleft", { 44, -1, -1 }, -1, 0 },
    { "+moveright", { 46, -1, -1 }, -1, 0 },
    { "+moveup", { 32, -1, -1 }, -1, 0 },
    { "+movedown", { 99, -1, -1 }, -1, 0 },
    { "+left", { 156, -1, -1 }, -1, 0 },
    { "+right", { 157, -1, -1 }, -1, 0 },
    { "+strafe", { 158, -1, -1 }, -1, 0 },
    { "+lookup", { 163, -1, -1 }, -1, 0 },
    { "+lookdown", { 162, -1, -1 }, -1, 0 },
    { "+mlook", { 47, -1, -1 }, -1, 0 },
    { "centerview", { 166, -1, -1 }, -1, 0 },
    { "+attack", { 159, -1, -1 }, -1, 0 },
    { "weapprev", { 205, -1, -1 }, -1, 0 },
    { "weapnext", { 206, -1, -1 }, -1, 0 },
    { "weapalt", { -1, -1, -1 }, -1, 0 },
    { "scoresUp", { -1, -1, -1 }, -1, 0 },
    { "scoresDown", { -1, -1, -1 }, -1, 0 },
    { "messagemode", { -1, -1, -1 }, -1, 0 },
    { "messagemode2", { -1, -1, -1 }, -1, 0 },
    { "messagemode3", { -1, -1, -1 }, -1, 0 },
    { "messagemode4", { -1, -1, -1 }, -1, 0 },
    { "+activate", { -1, -1, -1 }, -1, 0 },
    { "+reload", { -1, -1, -1 }, -1, 0 },
    { "help", { 167, -1, -1 }, -1, 0 },
    { "+leanleft", { -1, -1, -1 }, -1, 0 },
    { "+leanright", { -1, -1, -1 }, -1, 0 },
    { "vote yes", { -1, -1, -1 }, -1, 0 },
    { "vote no", { -1, -1, -1 }, -1, 0 },
    { "mp_QuickMessage", { -1, -1, -1 }, -1, 0 },
    { "mp_quickmap", { -1, -1, -1 }, -1, 0 },
    { "mp_purchase", { -1, -1, -1 }, -1, 0 },
    { "weaponslot primary", { 49, -1, -1 }, -1, 0 },
    { "weaponslot primaryb", { 50, -1, -1 }, -1, 0 },
    { "weaponslot pistol", { 51, -1, -1 }, -1, 0 },
    { "weaponslot grenade", { 52, -1, -1 }, -1, 0 },
    { "weaponslot smokegrenade", { 53, -1, -1 }, -1, 0 },
    { "weaponslot satchel", { 54, -1, -1 }, -1, 0 },
    { "weaponslot binocular", { 55, -1, -1 }, -1, 0 },
    { "+melee", { -1, -1, -1 }, -1, 0 },
    { "+prone", { -1, -1, -1 }, -1, 0 },
    { "lowerstance", { -1, -1, -1 }, -1, 0 },
    { "raisestance", { -1, -1, -1 }, -1, 0 },
    { "togglecrouch", { -1, -1, -1 }, -1, 0 },
    { "toggleprone", { -1, -1, -1 }, -1, 0 },
    { "goprone", { -1, -1, -1 }, -1, 0 },
    { "gocrouch", { -1, -1, -1 }, -1, 0 },
    { "+gostand", { -1, -1, -1 }, -1, 0 },
    { "toggle cl_run", { -1, -1, -1 }, -1, 0 },
    { "+sprint", { -1, -1, -1 }, -1, 0 },
    { "screenshot", { -1, -1, -1 }, -1, 0 },
    { "screenshotJPEG", { -1, -1, -1 }, -1, 0 }
};

static char bindingText[UI_BINDING_TEXT_SIZE];
static char secondBindingText[UI_BINDING_TEXT_SIZE];
static char firstKeyNameBuffer[UI_KEY_NAME_BUFFER_SIZE];
static char secondKeyNameBuffer[UI_KEY_NAME_BUFFER_SIZE];
static char localizedBindingText[UI_LOCALIZED_BINDING_TEXT_SIZE];

void Controls_GetKeyAssignment(const char *command, int32_t keys[2])
{
    char binding[UI_BINDING_SCAN_BUFFER_SIZE];
    int32_t found = 0;
    int32_t key;

    keys[1] = UI_KEY_UNBOUND;
    keys[0] = UI_KEY_UNBOUND;

    for (key = 0; key < MAX_KEYS; ++key) {
        DC->getBindingBuf(key, binding, UI_BINDING_SCAN_BUFFER_SIZE);
        if (binding[0] != '\0' && command != NULL &&
            Q_stricmpn(command, binding,
                       UI_UNBOUNDED_STRING_COMPARE) == 0) {
            keys[found++] = key;
            if (found == UI_KEYS_PER_BINDING) {
                return;
            }
        }
    }
}

void Controls_GetConfig(void)
{
    int32_t index;

    for (index = 0; index < CONTROL_BINDING_COUNT; ++index) {
        int32_t keys[UI_KEYS_PER_BINDING];

        Controls_GetKeyAssignment(g_bindings[index].command, keys);
        g_bindings[index].bind1 = keys[0];
        g_bindings[index].bind2 = keys[1];
    }
}

void Controls_SetConfig(void)
{
    int32_t index;

    for (index = 0; index < CONTROL_BINDING_COUNT; ++index) {
        bind_t *binding = &g_bindings[index];

        if (binding->bind1 == UI_KEY_UNBOUND) {
            continue;
        }
        DC->setBinding(binding->bind1, binding->command);
        if (binding->bind2 != UI_KEY_UNBOUND) {
            DC->setBinding(binding->bind2, binding->command);
        }
    }

    DC->executeText(EXEC_APPEND, "in_restart\n");
}

void Controls_SetDefaults(void)
{
    int32_t index;

    for (index = 0; index < CONTROL_BINDING_COUNT; ++index) {
        /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
        g_bindings[index].bind1 = g_bindings[index].defaultKeys[0];
        g_bindings[index].bind2 = UI_KEY_UNBOUND;
    }
}

int32_t BindingIDFromName(const char *command)
{
    int32_t index;

    for (index = 0; index < CONTROL_BINDING_COUNT; ++index) {
        if (command != NULL && g_bindings[index].command != NULL &&
            Q_stricmpn(g_bindings[index].command, command,
                       UI_UNBOUNDED_STRING_COMPARE) == 0) {
            return index;
        }
    }
    return -1;
}

const char *BindingFromName(const char *command, qboolean firstKeyOnly)
{
    const int32_t bindingIndex = BindingIDFromName(command);
    const char *localized;
    bind_t *binding;

    if (bindingIndex < 0 || g_bindings[bindingIndex].bind1 == UI_KEY_UNBOUND) {
        /* NOT_FROM_ORIGINAL_SOURCE: mounted localization text is display data;
         * keep it within the fixed shared binding buffer. */
        Q_strncpyz(bindingText, DC->getLocalizedString("KEY_UNBOUND"), UI_BINDING_TEXT_SIZE);
        return bindingText;
    }

    binding = &g_bindings[bindingIndex];
    DC->keynumToStringBuf(binding->bind1, bindingText,
                          UI_BINDING_KEY_TEXT_SIZE);
    localized = DC->getLocalizedString(bindingText);
    if (localized != bindingText) {
        Q_strncpyz(bindingText, localized, UI_BINDING_TEXT_SIZE);
    }
    if (binding->bind2 == UI_KEY_UNBOUND || firstKeyOnly != qfalse) {
        return bindingText;
    }

    DC->keynumToStringBuf(binding->bind2, secondBindingText,
                          UI_BINDING_KEY_TEXT_SIZE);
    localized = DC->getLocalizedString(secondBindingText);
    if (localized != secondBindingText) {
        Q_strncpyz(secondBindingText, localized, UI_BINDING_TEXT_SIZE);
    }
    /* NOT_FROM_ORIGINAL_SOURCE: retain the "<key> <or> <key>" order while
     * bounding every append to the shared result buffer. */
    Q_strcat(bindingText, UI_BINDING_TEXT_SIZE, " ");
    Q_strcat(bindingText, UI_BINDING_TEXT_SIZE, DC->getLocalizedString("KEY_OR"));
    Q_strcat(bindingText, UI_BINDING_TEXT_SIZE, " ");
    Q_strcat(bindingText, UI_BINDING_TEXT_SIZE, secondBindingText);
    return bindingText;
}

qboolean GetCommandHasBinding(const char *command)
{
    const int32_t bindingIndex = BindingIDFromName(command);

    if (bindingIndex < 0) {
        return qfalse;
    }
    return g_bindings[bindingIndex].bind1 != UI_KEY_UNBOUND
               ? qtrue : qfalse;
}

int32_t Key_GetKeysForCommand(char **firstKeyName, char **secondKeyName,
                              const char *command)
{
    const int32_t bindingIndex = BindingIDFromName(command);
    bind_t *binding;

    firstKeyNameBuffer[0] = '\0';
    *firstKeyName = firstKeyNameBuffer;
    secondKeyNameBuffer[0] = '\0';
    *secondKeyName = secondKeyNameBuffer;

    if (bindingIndex < 0 ||
        g_bindings[bindingIndex].bind1 == UI_KEY_UNBOUND) {
        memcpy(firstKeyNameBuffer, "KEY_UNBOUND", sizeof("KEY_UNBOUND"));
        return 0;
    }

    binding = &g_bindings[bindingIndex];
    DC->keynumToStringBuf(binding->bind1, firstKeyNameBuffer,
                          UI_KEY_NAME_BUFFER_SIZE);
    if (binding->bind2 == UI_KEY_UNBOUND) {
        return 1;
    }
    DC->keynumToStringBuf(binding->bind2, secondKeyNameBuffer,
                          UI_KEY_NAME_BUFFER_SIZE);
    return 2;
}

int32_t UI_KeysStringForBinding(const char *command, char **bindingTextOut)
{
    char *firstKeyName;
    char *secondKeyName;
    const char *translated;
    int32_t keyCount;

    *bindingTextOut = localizedBindingText;
    keyCount = Key_GetKeysForCommand(&firstKeyName, &secondKeyName, command);
    if (keyCount == 0) {
        /* NOT_FROM_ORIGINAL_SOURCE: bound mounted display text to the shared
         * buffer; representable localized labels are unchanged. */
        Q_strncpyz(localizedBindingText, DC->getLocalizedString("KEY_UNBOUND"), UI_LOCALIZED_BINDING_TEXT_SIZE);
        return keyCount;
    }

    translated = DC->translateString(firstKeyName);
    Q_strncpyz(localizedBindingText, translated != NULL ? translated : firstKeyName, UI_LOCALIZED_BINDING_TEXT_SIZE);
    if (keyCount <= 1) {
        return keyCount;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: bound the separator and second translated key
     * to the display buffer. */
    Q_strcat(localizedBindingText, UI_LOCALIZED_BINDING_TEXT_SIZE, " ");
    Q_strcat(localizedBindingText, UI_LOCALIZED_BINDING_TEXT_SIZE, DC->getLocalizedString("KEY_OR"));
    Q_strcat(localizedBindingText, UI_LOCALIZED_BINDING_TEXT_SIZE, " ");
    translated = DC->translateString(secondKeyName);
    Q_strcat(localizedBindingText, UI_LOCALIZED_BINDING_TEXT_SIZE, translated != NULL ? translated : secondKeyName);
    return keyCount;
}
