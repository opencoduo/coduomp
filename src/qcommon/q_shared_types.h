#ifndef QCOMMON_Q_SHARED_TYPES_H
#define QCOMMON_Q_SHARED_TYPES_H

#include <stddef.h>
#include <stdint.h>

#include "q_collision_types.h"

/* Canonical inherited Quake string capacities shared by the engine and all
 * native modules. These are distinct semantic domains despite currently
 * having equal token and general-string capacities. MAX_VA_STRING is proven
 * by the 0x7d00 scratch and return-ring extents in the Windows engine, cgame,
 * and UI and in both Linux server binaries. */
enum {
    MAX_QPATH = 64,
    MAX_STRING_CHARS = 1024,
    MAX_TOKEN_CHARS = 1024,
    MAX_VA_STRING = 32000
};

enum { MAX_PARSE_SESSIONS = 16 };

/*
 * Common Com tokenizer session.  The authoritative i386 bodies all index
 * their session arrays with a 0x45c-byte stride and use the same offsets:
 *
 *   CoDUOMP.exe                 0x0044e550
 *   uo_cgame_mp_x86.dll        0x3004d250
 *   uo_ui_mp_x86.dll           0x40005260
 *   uo_game_mp_x86.dll         0x20056a80
 *   coduo_lnxded               0x0808521c
 *   game.mp.uo.i386.so         RVA 0x00091a44
 *
 * Each begin-session body compares the depth with 15, multiplies the new
 * index by 0x45c, initializes the dwords at +0x400..+0x40c, and copies the
 * 64-byte source name at +0x41c.  Pointer fields widen naturally in native
 * host builds; the original ABI assertions therefore apply only to i386.
 */
typedef struct com_parse_session_s {
    char token[MAX_TOKEN_CHARS];
    int32_t line;
    int32_t ungetToken;
    int32_t spaceDelimited;
    int32_t csv;
    int32_t parseNegativeNumbers;
    int32_t savedLine;
    char *savedParse;
    char name[MAX_QPATH];
} com_parse_session_t;

typedef char q_com_parse_session_line_offset[
    offsetof(com_parse_session_t, line) == 0x400 ? 1 : -1];
typedef char q_com_parse_session_unget_offset[
    offsetof(com_parse_session_t, ungetToken) == 0x404 ? 1 : -1];
typedef char q_com_parse_session_space_delimited_offset[
    offsetof(com_parse_session_t, spaceDelimited) == 0x408 ? 1 : -1];
typedef char q_com_parse_session_csv_offset[
    offsetof(com_parse_session_t, csv) == 0x40c ? 1 : -1];
typedef char q_com_parse_session_parse_negative_offset[
    offsetof(com_parse_session_t, parseNegativeNumbers) == 0x410 ? 1 : -1];
typedef char q_com_parse_session_saved_line_offset[
    offsetof(com_parse_session_t, savedLine) == 0x414 ? 1 : -1];
typedef char q_com_parse_session_saved_parse_offset[
    offsetof(com_parse_session_t, savedParse) == 0x418 ? 1 : -1];

#if UINTPTR_MAX == UINT32_MAX
typedef char q_com_parse_session_name_offset[
    offsetof(com_parse_session_t, name) == 0x41c ? 1 : -1];
typedef char q_com_parse_session_size[
    sizeof(com_parse_session_t) == 0x45c ? 1 : -1];
#endif

/*
 * Compact public precompiler token exchanged by the engine and client
 * modules.  CoDUOMP.exe PC_ReadTokenHandle (0x00446f90) copies text to +0x10,
 * stores type/subtype/intValue at +0x00/+0x04/+0x08, and narrows the private
 * floating value into the binary32 slot at +0x0c.  The Windows cgame and UI
 * parsers reserve and consume the same 0x410-byte record at their
 * PC_READ_TOKEN syscall boundaries.  This is distinct from each engine's
 * larger private token_t, whose floating carrier and trailing fields differ
 * between Windows and Linux.
 */
typedef struct pc_token_s {
    int32_t type;
    int32_t subtype;
    int32_t intValue;
    float floatValue;
    char string[MAX_TOKEN_CHARS];
} pc_token_t;

typedef char q_pc_token_type_offset[
    offsetof(pc_token_t, type) == 0x00 ? 1 : -1];
typedef char q_pc_token_subtype_offset[
    offsetof(pc_token_t, subtype) == 0x04 ? 1 : -1];
typedef char q_pc_token_int_value_offset[
    offsetof(pc_token_t, intValue) == 0x08 ? 1 : -1];
typedef char q_pc_token_float_value_offset[
    offsetof(pc_token_t, floatValue) == 0x0c ? 1 : -1];
typedef char q_pc_token_string_offset[
    offsetof(pc_token_t, string) == 0x10 ? 1 : -1];
typedef char q_pc_token_size[sizeof(pc_token_t) == 0x410 ? 1 : -1];

/* Shared scalar domain used by the engine message and module boundaries. */
typedef enum qboolean_e {
    qfalse = 0,
    qtrue = 1
} qboolean;

typedef char qboolean_abi_size[sizeof(qboolean) == 4 ? 1 : -1];

/* Canonical Quake eight-byte integer carrier used by the complete shared
 * BigLong64/LittleLong64 family.  Windows i386 returns this small aggregate in
 * EDX:EAX; Linux i386 and PowerPC receive a hidden result pointer.  The
 * different machine-level return paths are compiler ABIs for this same source
 * type, whose eight byte fields and complete reversal agree on every target. */
typedef struct qint64_s {
    uint8_t b0;
    uint8_t b1;
    uint8_t b2;
    uint8_t b3;
    uint8_t b4;
    uint8_t b5;
    uint8_t b6;
    uint8_t b7;
} qint64;

typedef char qint64_abi_size[sizeof(qint64) == 8 ? 1 : -1];

/* Canonical Quake error domain passed to Com_Error by the engine and native
 * modules. CoDUOMP.exe 0x0043a020 and coduo_lnxded 0x080704ac distinguish the
 * same complete 0..7 range. Their cleanup bodies (0x00439e10 and 0x0807032a)
 * agree on the recoverable values, while the original callers distinguish
 * server-disconnect, disconnect, script, and localization uses. */
typedef enum errorParm_e {
    ERR_FATAL = 0,
    ERR_DROP = 1,
    ERR_SERVER_DISCONNECT = 2,
    ERR_DISCONNECT = 3,
    ERR_NEED_CD = 4,
    ERR_END_GAME = 5,
    ERR_SCRIPT = 6,
    ERR_LOCALIZATION = 7
} errorParm_t;

typedef char error_parm_abi_size[sizeof(errorParm_t) == 4 ? 1 : -1];

/*
 * Engine resource handle (renderer shader/model/etc.).  Quake3 canonical
 * typedef.  The original Windows cgame and UI registration wrappers
 * (0x3003dda0 and 0x4001d5c0) return the syscall result unchanged in EAX; the
 * UI model-bounds wrapper at 0x4001d770 likewise forwards the handle as one
 * 32-bit argument word.
 */
typedef int32_t qhandle_t;

typedef char qhandle_abi_size[sizeof(qhandle_t) == 4 ? 1 : -1];

/* Shared language identifiers used by the engine and both client modules.
 * CoDUOMP.exe Q_GetDecimalDelimiter (0x00450830) and the matching cgame/UI/game
 * localized-float bodies select comma for values 1, 2, 3, 4, 6, and 7.  The
 * renderer's Asian-language paths use the same contiguous 8..11 tail. */
typedef enum language_e {
    LANGUAGE_ENGLISH = 0,
    LANGUAGE_FRENCH = 1,
    LANGUAGE_GERMAN = 2,
    LANGUAGE_ITALIAN = 3,
    LANGUAGE_SPANISH = 4,
    LANGUAGE_BRITISH = 5,
    LANGUAGE_RUSSIAN = 6,
    LANGUAGE_POLISH = 7,
    LANGUAGE_KOREAN = 8,
    LANGUAGE_TAIWANESE = 9,
    LANGUAGE_JAPANESE = 10,
    LANGUAGE_CHINESE = 11
} language_t;

/* Canonical Quake command-buffer execution timing.  The Windows engine,
 * Linux engine, cgame, and UI all pass these exact integer values across their
 * command and syscall boundaries. */
typedef enum cbufExec_e {
    EXEC_NOW = 0,
    EXEC_INSERT = 1,
    EXEC_APPEND = 2
} cbufExec_t;

/* Parser checkpoint shared by both engines and every module containing the
 * common tokenizer.  CoDUOMP.exe 0x0044e7a0, coduo_lnxded 0x08085478,
 * uo_cgame_mp_x86.dll 0x3004d4a0, and game.mp.uo.i386.so RVA 0x00091d58
 * save and restore these five fields in this order.  Pointer lanes widen
 * naturally in native host builds; only the original i386 extent is 0x14. */
typedef struct com_parse_mark_s {
    int32_t line;
    char *parse;
    qboolean ungetToken;
    int32_t savedLine;
    char *savedParse;
} com_parse_mark_t;

#if UINTPTR_MAX == UINT32_MAX
typedef char q_com_parse_mark_line_offset[
    offsetof(com_parse_mark_t, line) == 0x00 ? 1 : -1];
typedef char q_com_parse_mark_parse_offset[
    offsetof(com_parse_mark_t, parse) == 0x04 ? 1 : -1];
typedef char q_com_parse_mark_unget_offset[
    offsetof(com_parse_mark_t, ungetToken) == 0x08 ? 1 : -1];
typedef char q_com_parse_mark_saved_line_offset[
    offsetof(com_parse_mark_t, savedLine) == 0x0c ? 1 : -1];
typedef char q_com_parse_mark_saved_parse_offset[
    offsetof(com_parse_mark_t, savedParse) == 0x10 ? 1 : -1];
typedef char q_com_parse_mark_size[
    sizeof(com_parse_mark_t) == 0x14 ? 1 : -1];
#endif

/*
 * Canonical engine cvar flags shared by the executable and every native
 * module.  The Linux engine's original Cvar_Get (0x08073114) tests the
 * combined 0x1080 placeholder mask and the 0x0200 cheat bit; Cvar_Set2
 * (0x0807343d) independently tests 0x0020, 0x0040, 0x0010, and 0x0200 for
 * latch, ROM, init, and cheat behavior.  The original game-module table at
 * RVA 0x000ad140 stores the same bits in each registration row's +0x0c lane.
 * Values 0x0800..0x2000 are the Call of Duty script/configstring extensions
 * to the inherited Quake flag domain.
 */
typedef enum cvarFlags_e {
    CVAR_NONE = 0x0000,
    CVAR_ARCHIVE = 0x0001,
    CVAR_USERINFO = 0x0002,
    CVAR_SERVERINFO = 0x0004,
    CVAR_SYSTEMINFO = 0x0008,
    CVAR_INIT = 0x0010,
    CVAR_LATCH = 0x0020,
    CVAR_ROM = 0x0040,
    CVAR_USER_CREATED = 0x0080,
    CVAR_TEMP = 0x0100,
    CVAR_CHEAT = 0x0200,
    CVAR_NORESTART = 0x0400,
    CVAR_SCRIPT_MAKE_SERVERINFO = 0x0800,
    CVAR_SCRIPT_SETCVAR = 0x1000,
    CVAR_SCRIPT_SETCVAR_SERVERINFO = 0x2000,
    CVAR_PLACEHOLDER_CREATED_MASK =
        CVAR_USER_CREATED | CVAR_SCRIPT_SETCVAR,
    CVAR_RESTART_PRESERVE_MASK = CVAR_INIT | CVAR_ROM | CVAR_NORESTART,
    CVAR_SERVERINFO_SYNC_MASK =
        CVAR_SERVERINFO | CVAR_SCRIPT_SETCVAR_SERVERINFO,
    CVAR_SYSTEMINFO_SYNC_MASK = CVAR_SYSTEMINFO,
    CVAR_SYSTEMINFO_KEY_VALUE = CVAR_SCRIPT_MAKE_SERVERINFO,
    CVAR_SYSTEMINFO_KEY_VALUE_SYNC_MASK = CVAR_SCRIPT_MAKE_SERVERINFO
} cvarFlags_t;

typedef char q_cvar_flags_abi_size[sizeof(cvarFlags_t) == 4 ? 1 : -1];

/*
 * Canonical Quake module-side cvar mirror shared by every engine/module ABI.
 * CoDUOMP.exe Cvar_Update (0x0043e090), coduo_lnxded Cvar_Update
 * (0x0807392b), and the supporting Mac engine body at file offset 0x000e3230
 * all read the handle at +0x00 and modification count at +0x04, copy at most
 * 256 bytes to +0x10, and store the numeric mirrors at +0x08 and +0x0c.
 * The record contains no pointers, so the same 0x110-byte layout applies to
 * the original 32-bit binaries and native 64-bit recovery builds.
 */
enum { MAX_CVAR_VALUE_STRING = 256 };

typedef struct vmCvar_s {
    int32_t handle;                              /* +0x00 */
    int32_t modificationCount;                   /* +0x04 */
    float value;                                 /* +0x08 */
    int32_t integer;                             /* +0x0c */
    char string[MAX_CVAR_VALUE_STRING];          /* +0x10 */
} vmCvar_t;

typedef char q_vm_cvar_handle_offset[
    offsetof(vmCvar_t, handle) == 0x00 ? 1 : -1];
typedef char q_vm_cvar_modification_count_offset[
    offsetof(vmCvar_t, modificationCount) == 0x04 ? 1 : -1];
typedef char q_vm_cvar_value_offset[
    offsetof(vmCvar_t, value) == 0x08 ? 1 : -1];
typedef char q_vm_cvar_integer_offset[
    offsetof(vmCvar_t, integer) == 0x0c ? 1 : -1];
typedef char q_vm_cvar_string_offset[
    offsetof(vmCvar_t, string) == 0x10 ? 1 : -1];
typedef char q_vm_cvar_string_extent[
    sizeof(((vmCvar_t *)0)->string) == 0x100 ? 1 : -1];
typedef char q_vm_cvar_size[sizeof(vmCvar_t) == 0x110 ? 1 : -1];

/*
 * Canonical Quake client-module cvar registration row.  The Windows cgame
 * CG_RegisterCvars body (0x3002b1a0) and UI UI_RegisterCvars body
 * (0x400113b0) both walk four-field rows with the same +0x00/+0x04/+0x08/+0x0c
 * accesses and a 0x10-byte i386 stride.  The server game module extends its
 * registration rows with modification-count and track-change fields and
 * therefore retains its distinct six-field type.
 */
typedef struct cvarTable_s {
    vmCvar_t *vmCvar;
    const char *cvarName;
    const char *defaultString;
    int32_t cvarFlags;
} cvarTable_t;

#if UINTPTR_MAX == UINT32_MAX
typedef char q_cvar_table_vm_cvar_offset[
    offsetof(cvarTable_t, vmCvar) == 0x00 ? 1 : -1];
typedef char q_cvar_table_name_offset[
    offsetof(cvarTable_t, cvarName) == 0x04 ? 1 : -1];
typedef char q_cvar_table_default_offset[
    offsetof(cvarTable_t, defaultString) == 0x08 ? 1 : -1];
typedef char q_cvar_table_flags_offset[
    offsetof(cvarTable_t, cvarFlags) == 0x0c ? 1 : -1];
typedef char q_cvar_table_size[sizeof(cvarTable_t) == 0x10 ? 1 : -1];
#endif

/*
 * Canonical engine/module user-command boundary.  CoDUOMP.exe
 * MSG_SetDefaultUserCmd (0x0044a1e0) clears the complete 0x18-byte record; it
 * and the adjacent delta codecs use the fields below at these offsets.  The
 * Windows cgame and game PM_CmdScale bodies (0x30008690 and 0x20008440) use
 * the same signed movement bytes, as do coduo_lnxded MSG_SetDefaultUserCmd
 * (0x08080e16) and the Linux game PM_CmdScale body (RVA 0x00023905).  The
 * supporting Mac cgame, game, and engine bodies corroborate the same offsets
 * at file offsets 0x0000ac20, 0x0000b570, and 0x0012e6f0.  The bytes at +0x07
 * and +0x17 are compiler padding, but complete command copies preserve them.
 */
enum usercmdButtonFlags_e {
    PM_BUTTON_FIRE = 0x01u,
    PM_BUTTON_UI_CAPTURE = 0x02u,
    PM_BUTTON_DROP_WEAPON = 0x04u,
    PM_BUTTON_SPRINT = 0x08u,
    PM_BUTTON_ADS = 0x10u,
    PM_BUTTON_MELEE = 0x20u,
    PM_BUTTON_ACTIVATE = 0x40u
};

enum usercmdWeaponButtonFlags_e {
    PM_WBUTTON_0 = 0x01u,
    PM_WBUTTON_STANCE_LATCH = 0x02u,
    PM_WBUTTON_WALK = 0x04u,
    PM_WBUTTON_RELOAD = 0x08u,
    PM_WBUTTON_LEAN_LEFT = 0x10u,
    PM_WBUTTON_LEAN_RIGHT = 0x20u,
    PM_WBUTTON_PRONE = 0x40u,
    PM_WBUTTON_CROUCH = 0x80u,
    PM_WBUTTON_STANCE_MASK = PM_WBUTTON_PRONE | PM_WBUTTON_CROUCH
};

typedef struct usercmd_s {
    int32_t commandTime;     /* +0x00 */
    uint8_t buttons;         /* +0x04, primary input flags */
    uint8_t wbuttons;        /* +0x05, secondary/weapon input flags */
    uint8_t weapon;          /* +0x06, requested weapon index */
    int32_t angles[3];       /* +0x08, pitch/yaw/roll angle words */
    int8_t forwardmove;      /* +0x14 */
    int8_t rightmove;        /* +0x15 */
    int8_t upmove;           /* +0x16 */
} usercmd_t;

typedef char q_usercmd_command_time_offset[
    offsetof(usercmd_t, commandTime) == 0x00 ? 1 : -1];
typedef char q_usercmd_buttons_offset[
    offsetof(usercmd_t, buttons) == 0x04 ? 1 : -1];
typedef char q_usercmd_wbuttons_offset[
    offsetof(usercmd_t, wbuttons) == 0x05 ? 1 : -1];
typedef char q_usercmd_weapon_offset[
    offsetof(usercmd_t, weapon) == 0x06 ? 1 : -1];
typedef char q_usercmd_angles_offset[
    offsetof(usercmd_t, angles) == 0x08 ? 1 : -1];
typedef char q_usercmd_angles_extent[
    sizeof(((usercmd_t *)0)->angles) == 0x0c ? 1 : -1];
typedef char q_usercmd_forwardmove_offset[
    offsetof(usercmd_t, forwardmove) == 0x14 ? 1 : -1];
typedef char q_usercmd_rightmove_offset[
    offsetof(usercmd_t, rightmove) == 0x15 ? 1 : -1];
typedef char q_usercmd_upmove_offset[
    offsetof(usercmd_t, upmove) == 0x16 ? 1 : -1];
typedef char q_usercmd_size[sizeof(usercmd_t) == 0x18 ? 1 : -1];

#endif
