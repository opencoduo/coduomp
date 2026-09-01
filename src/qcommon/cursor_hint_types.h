#ifndef QCOMMON_CURSOR_HINT_TYPES_H
#define QCOMMON_CURSOR_HINT_TYPES_H

/*
 * Cursor-hint values exchanged through entityState_t and playerState_t.
 * Windows and Linux game-module hintStrings tables publish the names for
 * values 1..10, and the Windows cgame consumes the same selector values.
 * Zero disables the runtime hint; -1 is the trigger-use inheritance sentinel.
 * Values beginning at 10 also select the two per-weapon HUD-icon ranges.
 */
typedef enum cursorHint_e {
    CURSOR_HINT_INHERIT = -1,
    CURSOR_HINT_OFF = 0,
    CURSOR_HINT_NONE = 1,
    CURSOR_HINT_ACTIVATE = 2,
    CURSOR_HINT_NOACTIVATE = 3,
    CURSOR_HINT_DOOR = 4,
    CURSOR_HINT_DOOR_LOCKED = 5,
    CURSOR_HINT_MG42 = 6,
    CURSOR_HINT_LMG = 7,
    CURSOR_HINT_HEALTH = 8,
    CURSOR_HINT_LADDER = 9,
    CURSOR_HINT_FRIENDLY = 10,
    CURSOR_HINT_BUILTIN_ICON_COUNT = CURSOR_HINT_FRIENDLY,
    CURSOR_HINT_WEAPON_BASE = CURSOR_HINT_FRIENDLY
} cursorHint_t;

typedef char cursor_hint_abi_size[(sizeof(cursorHint_t) == 4) ? 1 : -1];

#endif
