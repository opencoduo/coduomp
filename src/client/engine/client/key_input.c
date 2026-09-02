#include "console.h"

#include "cgame.h"
#include "../platform/crt_boundary.h"
#include "../ui/ui_module_loader.h"

#include <stdlib.h>
#include <string.h>

enum {
    KEY_BINDING_COMPARE_LIMIT = 99999,
    KEY_EVENT_COMMAND_SIZE = 1024
};

typedef struct key_name_s {
    const char *name;
    /* Special key codes share this domain with printable ASCII and the zero
     * table terminator; it is not exclusively a key_code_t enumerator. */
    int32_t key;
} key_name_t;

/* Original 0x04e19b00..0x04e1a6ff. Native bindings widen with the host. */
key_state_t keyStates[MAX_KEYS];
static int32_t keyDownCount; /* original 0x04e19adc */
static char keyNameBuffer[5]; /* original 0x008ce0cc */
cvar_t *cl_nodelta;           /* original 0x0495818c */
cvar_t *cl_debugMove;         /* original 0x04e19980 */
clKeyButton_t in_left;         /* original 0x008cddf8 */
clKeyButton_t in_right;        /* original 0x008cde10 */
clKeyButton_t in_forward;      /* original 0x008cde28 */
clKeyButton_t in_back;         /* original 0x008cde40 */
clKeyButton_t in_lookup;       /* original 0x008cde58 */
clKeyButton_t in_lookdown;     /* original 0x008cde70 */
clKeyButton_t in_moveleft;     /* original 0x008cde88 */
clKeyButton_t in_moveright;    /* original 0x008cdea0 */
clKeyButton_t in_strafe;       /* original 0x008cdeb8 */
clKeyButton_t in_speed;        /* original 0x008cded0 */
clKeyButton_t in_up;           /* original 0x008cdee8 */
clKeyButton_t in_down;         /* original 0x008cdf00 */
clKeyButton_t in_stanceUp;     /* original 0x008cdf18 */

/* The transmitted command buttons are separate original objects rather than
 * one C array: a non-command 24-byte object occupies 0x008cdfd8..0x008cdfef
 * between bits 6 and 7. CL_CmdButtons proves every active/wasPressed field and
 * the command registration table proves the semantic names below. */
clKeyButton_t in_attack;         /* bit 0, original 0x008cdf30 */
clKeyButton_t in_commandButton1; /* bit 1, original 0x008cdf48 */
clKeyButton_t in_dropWeapon;     /* bit 2, original 0x008cdf60 */
clKeyButton_t in_sprint;         /* bit 3, original 0x008cdf78 */
clKeyButton_t in_commandButton4; /* bit 4, original 0x008cdf90 */
clKeyButton_t in_melee;          /* bit 5, original 0x008cdfa8 */
clKeyButton_t in_activate;       /* bit 6, original 0x008cdfc0 */
clKeyButton_t in_commandButton7; /* bit 7, original 0x008cdff0 */
clKeyButton_t in_commandButton8; /* bit 8, original 0x008ce008 */
clKeyButton_t in_commandButton9; /* bit 9, original 0x008ce020 */
clKeyButton_t in_reload;         /* bit 10, original 0x008ce038 */
clKeyButton_t in_leanLeft;       /* bit 11, original 0x008ce050 */
clKeyButton_t in_leanRight;      /* bit 12, original 0x008ce068 */
clKeyButton_t in_prone;          /* bit 13, original 0x008ce080 */
qboolean in_mlooking;          /* original 0x008ce0c0 */

/* The PE contains two independent 128-entry {name,key} tables at 0x005c3d78
 * and 0x005c4178. Their key lanes and terminators are identical; only the name
 * pointers differ. This X-macro is source factoring for those two original
 * tables, not a third runtime table. */
#define KEY_NAME_STANDARD(entry_, key_, display_) entry_(key_, display_, "KEY_" display_)
#define KEY_NAME_DEFINITIONS(entry_) \
    KEY_NAME_STANDARD(entry_, K_TAB, "TAB") \
    KEY_NAME_STANDARD(entry_, K_ENTER, "ENTER") \
    KEY_NAME_STANDARD(entry_, K_ESCAPE, "ESCAPE") \
    KEY_NAME_STANDARD(entry_, K_SPACE, "SPACE") \
    KEY_NAME_STANDARD(entry_, K_BACKSPACE, "BACKSPACE") \
    KEY_NAME_STANDARD(entry_, K_UPARROW, "UPARROW") \
    KEY_NAME_STANDARD(entry_, K_DOWNARROW, "DOWNARROW") \
    KEY_NAME_STANDARD(entry_, K_LEFTARROW, "LEFTARROW") \
    KEY_NAME_STANDARD(entry_, K_RIGHTARROW, "RIGHTARROW") \
    KEY_NAME_STANDARD(entry_, K_ALT, "ALT") \
    KEY_NAME_STANDARD(entry_, K_CTRL, "CTRL") \
    KEY_NAME_STANDARD(entry_, K_SHIFT, "SHIFT") \
    KEY_NAME_STANDARD(entry_, K_CAPSLOCK, "CAPSLOCK") \
    KEY_NAME_STANDARD(entry_, K_F1, "F1") \
    KEY_NAME_STANDARD(entry_, K_F2, "F2") \
    KEY_NAME_STANDARD(entry_, K_F3, "F3") \
    KEY_NAME_STANDARD(entry_, K_F4, "F4") \
    KEY_NAME_STANDARD(entry_, K_F5, "F5") \
    KEY_NAME_STANDARD(entry_, K_F6, "F6") \
    KEY_NAME_STANDARD(entry_, K_F7, "F7") \
    KEY_NAME_STANDARD(entry_, K_F8, "F8") \
    KEY_NAME_STANDARD(entry_, K_F9, "F9") \
    KEY_NAME_STANDARD(entry_, K_F10, "F10") \
    KEY_NAME_STANDARD(entry_, K_F11, "F11") \
    KEY_NAME_STANDARD(entry_, K_F12, "F12") \
    KEY_NAME_STANDARD(entry_, K_INS, "INS") \
    KEY_NAME_STANDARD(entry_, K_DEL, "DEL") \
    KEY_NAME_STANDARD(entry_, K_PGDN, "PGDN") \
    KEY_NAME_STANDARD(entry_, K_PGUP, "PGUP") \
    KEY_NAME_STANDARD(entry_, K_HOME, "HOME") \
    KEY_NAME_STANDARD(entry_, K_END, "END") \
    KEY_NAME_STANDARD(entry_, K_MOUSE1, "MOUSE1") \
    KEY_NAME_STANDARD(entry_, K_MOUSE2, "MOUSE2") \
    KEY_NAME_STANDARD(entry_, K_MOUSE3, "MOUSE3") \
    KEY_NAME_STANDARD(entry_, K_MOUSE4, "MOUSE4") \
    KEY_NAME_STANDARD(entry_, K_MOUSE5, "MOUSE5") \
    KEY_NAME_STANDARD(entry_, K_MWHEELUP, "MWHEELUP") \
    KEY_NAME_STANDARD(entry_, K_MWHEELDOWN, "MWHEELDOWN") \
    KEY_NAME_STANDARD(entry_, K_JOY1, "JOY1") \
    KEY_NAME_STANDARD(entry_, K_JOY2, "JOY2") \
    KEY_NAME_STANDARD(entry_, K_JOY3, "JOY3") \
    KEY_NAME_STANDARD(entry_, K_JOY4, "JOY4") \
    KEY_NAME_STANDARD(entry_, K_JOY5, "JOY5") \
    KEY_NAME_STANDARD(entry_, K_JOY6, "JOY6") \
    KEY_NAME_STANDARD(entry_, K_JOY7, "JOY7") \
    KEY_NAME_STANDARD(entry_, K_JOY8, "JOY8") \
    KEY_NAME_STANDARD(entry_, K_JOY9, "JOY9") \
    KEY_NAME_STANDARD(entry_, K_JOY10, "JOY10") \
    KEY_NAME_STANDARD(entry_, K_JOY11, "JOY11") \
    KEY_NAME_STANDARD(entry_, K_JOY12, "JOY12") \
    KEY_NAME_STANDARD(entry_, K_JOY13, "JOY13") \
    KEY_NAME_STANDARD(entry_, K_JOY14, "JOY14") \
    KEY_NAME_STANDARD(entry_, K_JOY15, "JOY15") \
    KEY_NAME_STANDARD(entry_, K_JOY16, "JOY16") \
    KEY_NAME_STANDARD(entry_, K_JOY17, "JOY17") \
    KEY_NAME_STANDARD(entry_, K_JOY18, "JOY18") \
    KEY_NAME_STANDARD(entry_, K_JOY19, "JOY19") \
    KEY_NAME_STANDARD(entry_, K_JOY20, "JOY20") \
    KEY_NAME_STANDARD(entry_, K_JOY21, "JOY21") \
    KEY_NAME_STANDARD(entry_, K_JOY22, "JOY22") \
    KEY_NAME_STANDARD(entry_, K_JOY23, "JOY23") \
    KEY_NAME_STANDARD(entry_, K_JOY24, "JOY24") \
    KEY_NAME_STANDARD(entry_, K_JOY25, "JOY25") \
    KEY_NAME_STANDARD(entry_, K_JOY26, "JOY26") \
    KEY_NAME_STANDARD(entry_, K_JOY27, "JOY27") \
    KEY_NAME_STANDARD(entry_, K_JOY28, "JOY28") \
    KEY_NAME_STANDARD(entry_, K_JOY29, "JOY29") \
    KEY_NAME_STANDARD(entry_, K_JOY30, "JOY30") \
    KEY_NAME_STANDARD(entry_, K_JOY31, "JOY31") \
    KEY_NAME_STANDARD(entry_, K_JOY32, "JOY32") \
    KEY_NAME_STANDARD(entry_, K_AUX1, "AUX1") \
    KEY_NAME_STANDARD(entry_, K_AUX2, "AUX2") \
    KEY_NAME_STANDARD(entry_, K_AUX3, "AUX3") \
    KEY_NAME_STANDARD(entry_, K_AUX4, "AUX4") \
    KEY_NAME_STANDARD(entry_, K_AUX5, "AUX5") \
    KEY_NAME_STANDARD(entry_, K_AUX6, "AUX6") \
    KEY_NAME_STANDARD(entry_, K_AUX7, "AUX7") \
    KEY_NAME_STANDARD(entry_, K_AUX8, "AUX8") \
    KEY_NAME_STANDARD(entry_, K_AUX9, "AUX9") \
    KEY_NAME_STANDARD(entry_, K_AUX10, "AUX10") \
    KEY_NAME_STANDARD(entry_, K_AUX11, "AUX11") \
    KEY_NAME_STANDARD(entry_, K_AUX12, "AUX12") \
    KEY_NAME_STANDARD(entry_, K_AUX13, "AUX13") \
    KEY_NAME_STANDARD(entry_, K_AUX14, "AUX14") \
    KEY_NAME_STANDARD(entry_, K_AUX15, "AUX15") \
    KEY_NAME_STANDARD(entry_, K_AUX16, "AUX16") \
    KEY_NAME_STANDARD(entry_, K_KP_HOME, "KP_HOME") \
    KEY_NAME_STANDARD(entry_, K_KP_UPARROW, "KP_UPARROW") \
    KEY_NAME_STANDARD(entry_, K_KP_PGUP, "KP_PGUP") \
    KEY_NAME_STANDARD(entry_, K_KP_LEFTARROW, "KP_LEFTARROW") \
    KEY_NAME_STANDARD(entry_, K_KP_5, "KP_5") \
    KEY_NAME_STANDARD(entry_, K_KP_RIGHTARROW, "KP_RIGHTARROW") \
    KEY_NAME_STANDARD(entry_, K_KP_END, "KP_END") \
    KEY_NAME_STANDARD(entry_, K_KP_DOWNARROW, "KP_DOWNARROW") \
    KEY_NAME_STANDARD(entry_, K_KP_PGDN, "KP_PGDN") \
    KEY_NAME_STANDARD(entry_, K_KP_ENTER, "KP_ENTER") \
    KEY_NAME_STANDARD(entry_, K_KP_INS, "KP_INS") \
    KEY_NAME_STANDARD(entry_, K_KP_DEL, "KP_DEL") \
    KEY_NAME_STANDARD(entry_, K_KP_SLASH, "KP_SLASH") \
    KEY_NAME_STANDARD(entry_, K_KP_MINUS, "KP_MINUS") \
    KEY_NAME_STANDARD(entry_, K_KP_PLUS, "KP_PLUS") \
    KEY_NAME_STANDARD(entry_, K_KP_NUMLOCK, "KP_NUMLOCK") \
    KEY_NAME_STANDARD(entry_, K_KP_STAR, "KP_STAR") \
    KEY_NAME_STANDARD(entry_, K_KP_EQUALS, "KP_EQUALS") \
    KEY_NAME_STANDARD(entry_, K_PAUSE, "PAUSE") \
    KEY_NAME_STANDARD(entry_, ';', "SEMICOLON") \
    KEY_NAME_STANDARD(entry_, K_COMMAND, "COMMAND") \
    entry_(K_CHAR_MICRO, "181", "\xb5") entry_(K_CHAR_INVERTED_QUESTION, "191", "\xbf") entry_(K_CHAR_SHARP_S, "223", "\xdf") \
        entry_(K_CHAR_A_GRAVE, "224", "\xe0") entry_(K_CHAR_A_ACUTE, "225", "\xe1") entry_(K_CHAR_A_DIAERESIS, "228", "\xe4") \
            entry_(K_CHAR_A_RING, "229", "\xe5") entry_(K_CHAR_AE, "230", "\xe6") entry_(K_CHAR_C_CEDILLA, "231", "\xe7") \
                entry_(K_CHAR_E_GRAVE, "232", "\xe8") entry_(K_CHAR_E_ACUTE, "233", "\xe9") entry_(K_CHAR_I_GRAVE, "236", "\xec") \
                    entry_(K_CHAR_N_TILDE, "241", "\xf1") entry_(K_CHAR_O_GRAVE, "242", "\xf2") entry_(K_CHAR_O_ACUTE, "243", "\xf3") \
                        entry_(K_CHAR_O_DIAERESIS, "246", "\xf6") entry_(K_CHAR_O_STROKE, "248", "\xf8") \
                            entry_(K_CHAR_U_GRAVE, "249", "\xf9") entry_(K_CHAR_U_ACUTE, "250", "\xfa") \
                                entry_(K_CHAR_U_DIAERESIS, "252", "\xfc")

#define KEY_DISPLAY_ENTRY(key_, display_, localized_) {display_, key_},
/* Original display-name table at 0x005c3d78, including the NULL terminator.
 * PE_RELOCATION_VALUES_VERIFIED: all 128 pointer targets match in order. */
static const key_name_t keyNames[] = {KEY_NAME_DEFINITIONS(KEY_DISPLAY_ENTRY){NULL, 0}};
#undef KEY_DISPLAY_ENTRY

#define KEY_LOCALIZED_ENTRY(key_, display_, localized_) {localized_, key_},
/* Original localized-name table at 0x005c4178, including the NULL terminator.
 * PE_RELOCATION_VALUES_VERIFIED: all 128 pointer targets match in order. */
static const key_name_t localizedKeyNames[] = {KEY_NAME_DEFINITIONS(KEY_LOCALIZED_ENTRY){NULL, 0}};
#undef KEY_LOCALIZED_ENTRY
#undef KEY_NAME_DEFINITIONS
#undef KEY_NAME_STANDARD

/* Original pointer table at 0x005c4578. The pointed-to strings are exact
 * one-byte French AZERTY number-row labels, not UTF-8 text.
 * PE_RELOCATION_VALUES_VERIFIED. */
static const char *const keyFrenchDigitNames[10] = {"\x26", "\xe9", "\x22", "\x27", "\x28", "\x2d", "\xe8", "\x5f", "\xe7", "\xe0"};

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(_Alignof(key_state_t) == 4, "original i386 key-state alignment");
_Static_assert(offsetof(key_state_t, down) == 0x00, "original i386 key-state down offset");
_Static_assert(offsetof(key_state_t, repeatCount) == 0x04, "original i386 key-state repeat-count offset");
_Static_assert(offsetof(key_state_t, binding) == 0x08, "original i386 key-state binding offset");
_Static_assert(sizeof(key_state_t) == 0x0c, "original i386 key-state extent");
#endif

/* Source: CoDUOMP.exe 0x0040b570..0x0040b5ed.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040b570_0040b5ee.mcode.
 * Name and two-key rollover behavior: exact same-module Mac symbol
 * IN_KeyDown. The Windows ABI passes button in ESI; maintained source uses
 * the source-level pointer argument. */
static void IN_KeyDown(clKeyButton_t *button)
{
    int32_t key = -1;
    if (cmd_argc > 1 && cmd_argv[1][0] != '\0')
        key = coduo_crt_atoi(cmd_argv[1]);

    if (key == button->downKeys[0] || key == button->downKeys[1]) {
        return;
    }

    if (button->downKeys[0] == 0) {
        button->downKeys[0] = key;
    } else if (button->downKeys[1] == 0) {
        button->downKeys[1] = key;
    } else {
        Com_Printf("Three keys down for a button!\n");
        return;
    }

    if (button->active != qfalse)
        return;

    button->downtime = cmd_argc > 2 ? coduo_crt_atoi(cmd_argv[2]) : 0;
    button->active = qtrue;
    button->wasPressed = qtrue;
}

/* Source: CoDUOMP.exe 0x0040b5f0..0x0040b677.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040b5f0_0040b678.mcode.
 * Name and release-time accounting: exact same-module Mac symbol IN_KeyUp. */
static void IN_KeyUp(clKeyButton_t *button)
{
    if (cmd_argc <= 1 || cmd_argv[1][0] == '\0') {
        button->downKeys[0] = 0;
        button->downKeys[1] = 0;
        button->active = qfalse;
        return;
    }

    const int32_t key = coduo_crt_atoi(cmd_argv[1]);
    if (button->downKeys[0] == key) {
        button->downKeys[0] = 0;
    } else if (button->downKeys[1] == key) {
        button->downKeys[1] = 0;
        if (button->downKeys[0] != 0)
            return;
    } else {
        return;
    }

    if (button->downKeys[1] != 0)
        return;

    button->active = qfalse;
    const int32_t eventTime = cmd_argc > 2 ? coduo_crt_atoi(cmd_argv[2]) : 0;
    if (eventTime != 0) {
        button->accumulatedMsec = (int32_t)((uint32_t)button->accumulatedMsec + ((uint32_t)eventTime - (uint32_t)button->downtime));
    } else {
        button->accumulatedMsec = (int32_t)((uint32_t)button->accumulatedMsec + (cl_commandFrameMsec >> 1));
    }
}

enum {
    CL_STANCE_STAND = 0,
    CL_STANCE_CROUCH = 1,
    CL_STANCE_PRONE = 2
};

/* Source: CoDUOMP.exe 0x0040b700..0x0040b73b.
 * Name and role: exact same-module Mac symbol CL_SetTempStanceStatus. */
void CL_SetTempStanceStatus(void)
{
    const char *const value = in_prone.active != qfalse || in_down.active != qfalse ? "1" : "0";
    (void)Cvar_Set2("cl_stanceTemp", value, qtrue);
}

/* Source: CoDUOMP.exe 0x0040b740..0x0040b7c6.
 * Name and command binding: exact same-module Mac symbols IN_UpDown and
 * IN_UpUp. The stance-up accumulator is distinct from the actual vertical/
 * jump accumulator; +moveup drives both only while already standing. */
void IN_UpDown(void)
{
    IN_KeyDown(&in_stanceUp);

    if (in_prone.active != qfalse || in_down.active != qfalse) {
        return;
    }

    if (cl_stance->integer > CL_STANCE_CROUCH) {
        (void)Cvar_Set2("cl_stance", "1", qtrue);
        return;
    }
    if (cl_stance->integer > CL_STANCE_STAND) {
        (void)Cvar_Set2("cl_stance", "0", qtrue);
        return;
    }

    IN_KeyDown(&in_up);
}

void IN_UpUp(void)
{
    IN_KeyUp(&in_stanceUp);
    IN_KeyUp(&in_up);
}

/* Source: CoDUOMP.exe 0x0040b7d0..0x0040b867.
 * Name and command binding: exact same-module Mac symbols IN_DownDown and
 * IN_DownUp. */
void IN_DownDown(void)
{
    IN_KeyDown(&in_down);
    CL_SetTempStanceStatus();
}

void IN_DownUp(void)
{
    IN_KeyUp(&in_down);
    CL_SetTempStanceStatus();
}

/* Source: CoDUOMP.exe 0x0040b870..0x0040bc2c.
 * Evidence: direct wrapper bodies in executable_gaps.mcode and the exact
 * command/handler table at 0x0040d130. Each ordinary pair forwards one
 * original key-button object to IN_KeyDown/IN_KeyUp. */
void IN_LeftDown(void)
{
    IN_KeyDown(&in_left);
}

void IN_LeftUp(void)
{
    IN_KeyUp(&in_left);
}

void IN_RightDown(void)
{
    IN_KeyDown(&in_right);
}

void IN_RightUp(void)
{
    IN_KeyUp(&in_right);
}

void IN_ForwardDown(void)
{
    IN_KeyDown(&in_forward);
}

void IN_ForwardUp(void)
{
    IN_KeyUp(&in_forward);
}

void IN_BackDown(void)
{
    IN_KeyDown(&in_back);
}

void IN_BackUp(void)
{
    IN_KeyUp(&in_back);
}

void IN_LookupDown(void)
{
    IN_KeyDown(&in_lookup);
}

void IN_LookupUp(void)
{
    IN_KeyUp(&in_lookup);
}

void IN_LookdownDown(void)
{
    IN_KeyDown(&in_lookdown);
}

void IN_LookdownUp(void)
{
    IN_KeyUp(&in_lookdown);
}

void IN_MoveleftDown(void)
{
    IN_KeyDown(&in_moveleft);
}

void IN_MoveleftUp(void)
{
    IN_KeyUp(&in_moveleft);
}

void IN_MoverightDown(void)
{
    IN_KeyDown(&in_moveright);
}

void IN_MoverightUp(void)
{
    IN_KeyUp(&in_moveright);
}

void IN_SpeedDown(void)
{
    IN_KeyDown(&in_speed);
}

void IN_SpeedUp(void)
{
    IN_KeyUp(&in_speed);
}

void IN_StrafeDown(void)
{
    IN_KeyDown(&in_strafe);
}

void IN_StrafeUp(void)
{
    IN_KeyUp(&in_strafe);
}

void IN_Button0Down(void)
{
    IN_KeyDown(&in_attack);
}

void IN_Button0Up(void)
{
    IN_KeyUp(&in_attack);
}

/* These unused generic button wrappers are present in the Windows image but
 * are not installed by CL_InitKeyCommands. Their names follow the proven
 * transmitted bit and neighboring exact IN_Button0/IN_Button5 names. */
void IN_Button1Down(void)
{
    IN_KeyDown(&in_commandButton1);
}

void IN_Button1Up(void)
{
    IN_KeyUp(&in_commandButton1);
}

void IN_Button2Down(void)
{
    IN_KeyDown(&in_dropWeapon);
}

void IN_Button2Up(void)
{
    IN_KeyUp(&in_dropWeapon);
}

void IN_Button3Down(void)
{
    IN_KeyDown(&in_sprint);
}

void IN_Button3Up(void)
{
    IN_KeyUp(&in_sprint);
}

void IN_Button4Down(void)
{
    IN_KeyDown(&in_commandButton4);
}

void IN_Button4Up(void)
{
    IN_KeyUp(&in_commandButton4);
}

void IN_Button5Down(void)
{
    IN_KeyDown(&in_melee);
}

void IN_Button5Up(void)
{
    IN_KeyUp(&in_melee);
}

void IN_ActivateDown(void)
{
    IN_KeyDown(&in_activate);
}

void IN_ActivateUp(void)
{
    IN_KeyUp(&in_activate);
}

void IN_Button7Down(void)
{
    IN_KeyDown(&in_commandButton7);
}

void IN_Button7Up(void)
{
    IN_KeyUp(&in_commandButton7);
}

void IN_Button9Down(void)
{
    IN_KeyDown(&in_commandButton9);
}

void IN_Button9Up(void)
{
    IN_KeyUp(&in_commandButton9);
}

void IN_ReloadDown(void)
{
    IN_KeyDown(&in_reload);
}

void IN_ReloadUp(void)
{
    IN_KeyUp(&in_reload);
}

void IN_LeanLeftDown(void)
{
    IN_KeyDown(&in_leanLeft);
}

void IN_LeanLeftUp(void)
{
    IN_KeyUp(&in_leanLeft);
}

void IN_LeanRightDown(void)
{
    IN_KeyDown(&in_leanRight);
}

void IN_LeanRightUp(void)
{
    IN_KeyUp(&in_leanRight);
}

void IN_MP_DropWeaponDown(void)
{
    IN_KeyDown(&in_dropWeapon);
}

void IN_MP_DropWeaponUp(void)
{
    IN_KeyUp(&in_dropWeapon);
}

void IN_SprintDown(void)
{
    IN_KeyDown(&in_sprint);
}

void IN_SprintUp(void)
{
    IN_KeyUp(&in_sprint);
}

/* Source: CoDUOMP.exe 0x0040bb30..0x0040bbc7.
 * Name and command binding: exact same-module Mac symbols IN_Wbutton6Down
 * and IN_Wbutton6Up. Wbutton6 is the original source spelling for the prone
 * command; it drives transmitted command bit 13. */
void IN_Wbutton6Down(void)
{
    IN_KeyDown(&in_prone);
    CL_SetTempStanceStatus();
}

void IN_Wbutton6Up(void)
{
    IN_KeyUp(&in_prone);
    CL_SetTempStanceStatus();
}

/* Source: CoDUOMP.exe 0x0040bc10..0x0040bc2c. This second unregistered pair
 * targets command bit 1 as well; its exact source spelling is unavailable
 * because the Mac linker dead-stripped the unused duplicate. */
void IN_UnboundCommandButton1Down(void)
{
    IN_KeyDown(&in_commandButton1);
}

void IN_UnboundCommandButton1Up(void)
{
    IN_KeyUp(&in_commandButton1);
}

/* Source: CoDUOMP.exe 0x0040bc30..0x0040bc44.
 * Name and pitch operation: exact same-module Mac symbol IN_CenterView. */
void IN_CenterView(void)
{
    cl.inputState.viewAngles[0] = -(float)cl.snap.ps.deltaAngles[0] * 0.0054931640625f; /* exact 0x3bb40000, 360 / 65536 */
}

/* Source: CoDUOMP.exe 0x0040bc50..0x0040bcec.
 * Names and cvar transitions: exact same-module Mac symbols IN_LowerStance
 * and IN_RaiseStance. */
void IN_LowerStance(void)
{
    if (in_prone.active != qfalse || in_down.active != qfalse) {
        return;
    }

    if (cl_stance->integer < CL_STANCE_CROUCH) {
        (void)Cvar_Set2("cl_stance", "1", qtrue);
    } else if (cl_stance->integer < CL_STANCE_PRONE) {
        (void)Cvar_Set2("cl_stance", "2", qtrue);
    }
}

void IN_RaiseStance(void)
{
    if (in_prone.active != qfalse || in_down.active != qfalse) {
        return;
    }

    if (cl_stance->integer > CL_STANCE_CROUCH) {
        (void)Cvar_Set2("cl_stance", "1", qtrue);
    } else if (cl_stance->integer > CL_STANCE_STAND) {
        (void)Cvar_Set2("cl_stance", "0", qtrue);
    }
}

/* Source: CoDUOMP.exe 0x0040bcf0..0x0040bde6.
 * Names and exact toggle/direct-set transitions: same-module Mac symbols
 * IN_ToggleCrouch, IN_ToggleProne, IN_GoProne, and IN_GoCrouch. */
void IN_ToggleCrouch(void)
{
    if (in_prone.active != qfalse || in_down.active != qfalse) {
        return;
    }

    (void)Cvar_Set2("cl_stance", cl_stance->integer == CL_STANCE_CROUCH ? "0" : "1", qtrue);
}

void IN_ToggleProne(void)
{
    if (in_prone.active != qfalse || in_down.active != qfalse) {
        return;
    }

    (void)Cvar_Set2("cl_stance", cl_stance->integer == CL_STANCE_PRONE ? "0" : "2", qtrue);
}

void IN_GoProne(void)
{
    if (in_prone.active == qfalse && in_down.active == qfalse) {
        (void)Cvar_Set2("cl_stance", "2", qtrue);
    }
}

void IN_GoCrouch(void)
{
    if (in_prone.active == qfalse && in_down.active == qfalse) {
        (void)Cvar_Set2("cl_stance", "1", qtrue);
    }
}

/* Original 0x0389fce0, private to IN_GoStandDown. */
static int32_t cl_lastGoStandTime;

/* Source: CoDUOMP.exe 0x0040bdf0..0x0040be86.
 * Names and jump-window behavior: exact same-module Mac symbols
 * IN_GoStandDown and IN_GoStandUp. */
void IN_GoStandDown(void)
{
    IN_KeyDown(&in_stanceUp);

    const int32_t jumpWindow = cl_goStandJumpTime->integer;
    if ((jumpWindow <= 0 && cl_stance->integer == CL_STANCE_STAND) ||
        (jumpWindow > 0 && (int32_t)((uint32_t)com_frameTime - (uint32_t)cl_lastGoStandTime) < jumpWindow)) {
        IN_KeyDown(&in_up);
        cl_lastGoStandTime = 0;
        return;
    }

    cl_lastGoStandTime = com_frameTime;
    if (in_prone.active == qfalse && in_down.active == qfalse) {
        (void)Cvar_Set2("cl_stance", "0", qtrue);
    }
}

void IN_GoStandUp(void)
{
    IN_KeyUp(&in_stanceUp);
    IN_KeyUp(&in_up);
}

/* Source: CoDUOMP.exe 0x0040b530..0x0040b56a.
 * Names and behavior: exact same-module Mac symbols IN_MLookDown and
 * IN_MLookUp. */
void IN_MLookDown(void)
{
    in_mlooking = qtrue;
}

void IN_MLookUp(void)
{
    in_mlooking = qfalse;
    if (cl_freelook->integer == 0)
        IN_CenterView();
}

/* Source: CoDUOMP.exe 0x0040c1e0..0x0040c200, repaired from an executable
 * gap. Name: exact same-module Mac symbol CL_JoystickEvent. The Windows
 * compiler also inlined the complete body into Com_EventLoop at
 * 0x0043ac7c..0x0043ac9b; both copies prove the bounds, error text, and
 * six-element destination array. */
void CL_JoystickEvent(int32_t axis, int32_t value)
{
    if (axis < 0 || axis >= (int32_t)(sizeof(cl.inputState.joystickAxis) / sizeof(cl.inputState.joystickAxis[0]))) {
        Com_Error(ERR_DROP,
                  "\x15"
                  "CL_JoystickEvent: bad axis %i",
                  axis);
    }

    cl.inputState.joystickAxis[axis] = value;
}

/* Source: CoDUOMP.exe 0x0040c180..0x0040c1da.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040c180_0040c1db.mcode.
 * Name and delta routing: exact same-module Mac symbol CL_MouseEvent. */
void CL_MouseEvent(int32_t deltaX, int32_t deltaY)
{
    if ((cls.keyCatchers & KEYCATCH_UI) != 0 && cl_bypassMouseInput->integer != 1) {
        (void)VM_Call(coduo_uiVm, UIVM_MOUSE_EVENT, deltaX, deltaY, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        return;
    }

    if ((cls.keyCatchers & KEYCATCH_CGAME) != 0) {
        (void)VM_Call(coduo_cgameVm, CGVM_MOUSE_EVENT, deltaX, deltaY, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        return;
    }

    cl.inputState.mouseDx[cl.inputState.mouseIndex] += deltaX;
    cl.inputState.mouseDy[cl.inputState.mouseIndex] += deltaY;
}

/* Source: CoDUOMP.exe 0x0040e880..0x0040e953.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040e880_0040e954.mcode and the original
 * name/value table bytes at 0x005c3d78..0x005c4177.
 * Name and signature: exact same-module Mac symbol Key_StringToKeynum. */
int32_t Key_StringToKeynum(const char *string)
{
    if (string == NULL || string[0] == '\0')
        return -1;
    if (string[1] == '\0')
        return (int8_t)string[0];

    if (string[0] == '0' && string[1] == 'x' && strlen(string) == 4) {
        int32_t value = 0;

        for (int32_t index = 2; index < 4; ++index) {
            int32_t nibble;
            const int32_t character = (int8_t)string[index];

            if (Q_isnumeric(character) != qfalse)
                nibble = character - '0';
            else if (character >= 'a' && character <= 'f')
                nibble = character - 'a' + 10;
            else
                nibble = 0;
            value = value * 16 + nibble;
        }
        return value;
    }

    for (const key_name_t *entry = keyNames; entry->name != NULL; ++entry) {
        if (Q_stricmpn(entry->name, string, KEY_BINDING_COMPARE_LIMIT) == 0)
            return entry->key;
    }
    return -1;
}

/* Source: CoDUOMP.exe 0x0040e960..0x0040ea52.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040e960_0040ea53.mcode, the original
 * tables at 0x005c3d78..0x005c4577, and the French digit-byte pointer table
 * addressed from 0x005c44b8.
 * Name and signature: exact same-module Mac symbol Key_KeynumToString. */
const char *Key_KeynumToString(int32_t key, qboolean localized)
{
    if (key == -1)
        return "<KEY NOT FOUND>";
    if (key < 0 || key >= MAX_KEYS)
        return "<OUT OF RANGE>";

    if (localized != qfalse && cl_language->integer == LANGUAGE_FRENCH && key >= '0' && key <= '9') {
        return keyFrenchDigitNames[key - '0'];
    }

    if (key > K_SPACE && key < K_BACKSPACE && key != '"') {
        keyNameBuffer[0] = (char)coduo_crt_toupper(key);
        keyNameBuffer[1] = '\0';
        if (key != ';' || localized != qfalse)
            return keyNameBuffer;
    }

    const key_name_t *names = localized != qfalse ? localizedKeyNames : keyNames;
    for (const key_name_t *entry = names; entry->name != NULL; ++entry) {
        if ((int32_t)entry->key == key)
            return entry->name;
    }

    keyNameBuffer[0] = '0';
    keyNameBuffer[1] = 'x';
    int32_t highNibble = key >> 4;
    int32_t lowNibble = key & 15;
    keyNameBuffer[2] = (char)(highNibble > 9 ? highNibble + ('a' - 10) : highNibble + '0');
    keyNameBuffer[3] = (char)(lowNibble > 9 ? lowNibble + ('a' - 10) : lowNibble + '0');
    keyNameBuffer[4] = '\0';
    return keyNameBuffer;
}

/* Source: CoDUOMP.exe 0x0040e860..0x0040e872.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040e860_0040e873.mcode.
 * Name and return type: exact same-module Mac symbol Key_IsDown. The Windows
 * VM-dispatch caller passes the key number in EAX and forwards EAX unchanged. */
qboolean Key_IsDown(int32_t key)
{
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    if ((uint32_t)key >= (uint32_t)MAX_KEYS)
        return qfalse;
    return keyStates[key].down;
}

/* Source: CoDUOMP.exe 0x0040e840..0x0040e845.
 * Name and signature: exact same-module Mac symbol
 * Key_GetOverstrikeMode. */
qboolean Key_GetOverstrikeMode(void)
{
    return key_overstrikeMode;
}

/* Source: CoDUOMP.exe 0x0040e850..0x0040e855.
 * Name and signature: exact same-module Mac symbol
 * Key_SetOverstrikeMode. */
void Key_SetOverstrikeMode(qboolean enabled)
{
    key_overstrikeMode = enabled;
}

/* Source: CoDUOMP.exe 0x0041b4f0..0x0041b4f5.
 * Name and signature: exact same-module Mac symbol Key_GetCatcher. */
int32_t Key_GetCatcher(void)
{
    return cls.keyCatchers;
}

/* Source: CoDUOMP.exe 0x0041b500..0x0041b511.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041b500_0041b512.mcode.
 * Name: exact same-module Mac symbol Key_SetCatcher. The console bit cannot
 * be cleared by this boundary while the console already owns input. */
void Key_SetCatcher(int32_t catcher)
{
    if ((cls.keyCatchers & KEYCATCH_CONSOLE) != 0)
        catcher |= KEYCATCH_CONSOLE;
    cls.keyCatchers = catcher;
}

/* Source: CoDUOMP.exe 0x0041b4b0..0x0041b4e1.
 * Name and argument roles: exact same-module Mac symbol
 * Key_GetBindingBuf. */
void Key_GetBindingBuf(int32_t key, char *buffer, int32_t bufferSize)
{
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    if (bufferSize <= 0) {
        Com_Printf("WARNING: Key_GetBindingBuf: invalid buffer size %i\n", bufferSize);
        return;
    }

    const char *binding = (uint32_t)key < (uint32_t)MAX_KEYS ? keyStates[key].binding : "";

    if (binding == NULL) {
        buffer[0] = '\0';
        return;
    }

    strncpy(buffer, binding, (size_t)((uint32_t)bufferSize - 1u));
    buffer[bufferSize - 1] = '\0';
}

/* Source: CoDUOMP.exe 0x0041b490..0x0041b4af.
 * Name and argument roles: exact same-module Mac symbol
 * Key_KeynumToStringBuf. */
void Key_KeynumToStringBuf(int32_t key, char *buffer, int32_t bufferSize)
{
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    if (bufferSize <= 0) {
        Com_Printf("WARNING: Key_KeynumToStringBuf: invalid buffer size %i\n", bufferSize);
        return;
    }

    const char *name = Key_KeynumToString(key, qtrue);
    strncpy(buffer, name, (size_t)((uint32_t)bufferSize - 1u));
    buffer[bufferSize - 1] = '\0';
}

/* Source: CoDUOMP.exe 0x0040ea60..0x0040ea91.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040ea60_0040ea92.mcode.
 * Name and argument roles: exact same-module Mac symbol Key_SetBinding and
 * both the cgame VM dispatcher and UI dispatcher call sites. */
void Key_SetBinding(int32_t key, const char *binding)
{
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    if ((uint32_t)key >= (uint32_t)MAX_KEYS) {
        Com_Printf("WARNING: Key_SetBinding: invalid key number %i\n", key);
        return;
    }


    if (keyStates[key].binding != NULL)
        free(keyStates[key].binding);
    keyStates[key].binding = CopyStringInternal(binding);
    cvar_modifiedFlags |= CVAR_ARCHIVE;
}

/* Source: CoDUOMP.exe 0x0040eb10..0x0040eb82, recovered from the executable
 * gap between Key_GetKey and Key_Unbindall_f.
 * Name and command behavior: exact same-module Mac symbol Key_Unbind_f. */
void Key_Unbind_f(void)
{
    if (Cmd_Argc() != 2) {
        Com_Printf("unbind <key> : remove commands from a key\n");
        return;
    }

    const char *const keyName = Cmd_Argv(1);
    const int32_t key = Key_StringToKeynum(keyName);
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    if ((uint32_t)key >= (uint32_t)MAX_KEYS) {
        Com_Printf("\"%s\" isn't a valid key\n", keyName);
        return;
    }
    Key_SetBinding(key, "");
}

/* Source: CoDUOMP.exe 0x0040eb90..0x0040ebd0, recovered from the executable
 * gap. Name and full 256-entry traversal: exact same-module Mac symbol
 * Key_Unbindall_f. */
void Key_Unbindall_f(void)
{
    for (int32_t key = 0; key < MAX_KEYS; ++key) {
        if (keyStates[key].binding != NULL)
            Key_SetBinding(key, "");
    }
}

/* Source: CoDUOMP.exe 0x0040ebe0..0x0040edbc, recovered from the executable
 * gap. Name and command behavior: exact same-module Mac symbol Key_Bind_f.
 * The original lowercases the parsed key number before looking up or
 * replacing its binding and joins arguments 2..n with single spaces in a
 * 1024-byte local buffer without a bounds check. */
void Key_Bind_f(void)
{
    const int32_t argumentCount = Cmd_Argc();
    if (argumentCount < 2) {
        Com_Printf("bind <key> [command] : attach a command to a key\n");
        return;
    }

    const char *const keyName = Cmd_Argv(1);
    const int32_t key = coduo_crt_tolower(Key_StringToKeynum(keyName));
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    if ((uint32_t)key >= (uint32_t)MAX_KEYS) {
        Com_Printf("\"%s\" isn't a valid key\n", keyName);
        return;
    }
    if (argumentCount == 2) {
        const char *const binding = keyStates[key].binding;
        if (binding != NULL)
            Com_Printf("\"%s\" = \"%s\"\n", keyName, binding);
        else
            Com_Printf("\"%s\" is not bound\n", keyName);
        return;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    Key_SetBinding(key, Cmd_Args(2));
}

/* Source: CoDUOMP.exe 0x0040eef0..0x0040f006, recovered from the executable
 * gap. Name and output format: exact same-module Mac symbol Key_Bindlist_f.
 * The Windows optimizer inlines Key_KeynumToString in the traversal. */
void Key_Bindlist_f(void)
{
    for (int32_t key = 0; key < MAX_KEYS; ++key) {
        const char *const binding = keyStates[key].binding;
        if (binding != NULL && binding[0] != '\0') {
            Com_Printf("%s \"%s\"\n", Key_KeynumToString(key, qfalse), binding);
        }
    }
}

/* Source: CoDUOMP.exe 0x0040f010..0x0040f04f, recovered from the executable
 * gap after Key_Bindlist_f. The Windows compiler inlines this complete
 * source-level registration wrapper into Com_Init. */
void Key_Init(void)
{
    Cmd_AddCommand("bind", Key_Bind_f);
    Cmd_AddCommand("unbind", Key_Unbind_f);
    Cmd_AddCommand("unbindall", Key_Unbindall_f);
    Cmd_AddCommand("bindlist", Key_Bindlist_f);
}

/* Source: CoDUOMP.exe 0x0040eaa0..0x0040eab5.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040eaa0_0040eab6.mcode.
 * Name: exact same-module Mac symbol Key_GetBinding. Address 0x0058e74d is an
 * empty byte in .rdata, used only for the invalid-key sentinel. */
const char *Key_GetBinding(int32_t key)
{
    /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
    if ((uint32_t)key >= (uint32_t)MAX_KEYS)
        return "";
    return keyStates[key].binding;
}

/* Source: CoDUOMP.exe 0x0040eac0..0x0040eac5, recovered from an exporter
 * gap. Name and fixed result: exact same-module Mac symbol PbMaxKeys. */
int32_t PbMaxKeys(void)
{
    return MAX_KEYS;
}

/* Source: CoDUOMP.exe 0x0040ead0..0x0040eb0c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040ead0_0040eb0d.mcode.
 * Name and signature: exact same-module Mac symbol Key_GetKey. The machine
 * code scans all 256 binding fields at a 12-byte record stride and compares
 * through Q_stricmpn with the original effectively-unbounded count. */
int32_t Key_GetKey(const char *binding)
{
    if (binding == NULL)
        return -1;

    for (int32_t key = 0; key < MAX_KEYS; ++key) {
        if (keyStates[key].binding != NULL && Q_stricmpn(keyStates[key].binding, binding, KEY_BINDING_COMPARE_LIMIT) == 0) {
            return key;
        }
    }
    return -1;
}

/* Source: CoDUOMP.exe 0x0040d6b0..0x0040d6c0.
 * Name: exact same-module Mac symbol CL_ClearKeys. The Windows CL_KeyEvent
 * optimizer also inlines it at 0x0040f1a5 as a 720-byte REP STOSD over the
 * original contiguous input-state bank. Native recovered globals are
 * independent objects, so enumerate every represented key-button owner
 * instead of relying on linker adjacency. */
void CL_ClearKeys(void)
{
    clKeyButton_t *const buttons[] = {&in_left,
                                      &in_right,
                                      &in_forward,
                                      &in_back,
                                      &in_lookup,
                                      &in_lookdown,
                                      &in_moveleft,
                                      &in_moveright,
                                      &in_strafe,
                                      &in_speed,
                                      &in_up,
                                      &in_down,
                                      &in_stanceUp,
                                      &in_attack,
                                      &in_commandButton1,
                                      &in_dropWeapon,
                                      &in_sprint,
                                      &in_commandButton4,
                                      &in_melee,
                                      &in_activate,
                                      &in_commandButton7,
                                      &in_commandButton8,
                                      &in_commandButton9,
                                      &in_reload,
                                      &in_leanLeft,
                                      &in_leanRight,
                                      &in_prone};

    for (size_t index = 0; index < sizeof(buttons) / sizeof(buttons[0]); ++index) {
        memset(buttons[index], 0, sizeof(*buttons[index]));
    }
    in_mlooking = qfalse;
}

/* Source: CoDUOMP.exe 0x0040f050..0x0040f7f3.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040f050_0040f7f4.mcode.
 * Name and argument roles: exact same-module Mac symbol CL_KeyEvent. All
 * catcher routing, repeat suppression, VM commands, binding text, and state
 * transitions below are direct operands and branches in the Windows body.
 * The cgame-catcher release call to reserved command 6 is intentionally kept:
 * that is what the original executable dispatches. */
void CL_KeyEvent(int32_t key, qboolean down, uint32_t time)
{
    key_state_t *const state = &keyStates[key];
    const qboolean coduompSpecialConsoleKey = key == '`' || key == '~';
    state->down = down;

    if (down != qfalse) {
        ++state->repeatCount;
        if (state->repeatCount == 1) {
            ++keyDownCount;
        } else if ((cls.keyCatchers & (KEYCATCH_CONSOLE | KEYCATCH_MESSAGE)) == 0) {
            if ((cls.keyCatchers & KEYCATCH_UI) == 0)
                return;
            if (key != K_UPARROW && key != K_DOWNARROW && key != K_PGDN && key != K_PGUP) {
                return;
            }
        } else if (coduompSpecialConsoleKey != qfalse || key == K_ESCAPE) {
            return;
        }

        if (cl_waitForFire != NULL && cl_waitForFire->integer != 0) {
            if ((cls.keyCatchers & KEYCATCH_CONSOLE) != 0)
                Con_ToggleConsole_f();

            const char *const binding = state->binding;
            CL_ClearKeys();
            if (binding != NULL && Q_stricmpn(binding, "+attack", KEY_BINDING_COMPARE_LIMIT) == 0) {
                (void)Cvar_Set2("cl_waitForFire", "0", qtrue);
            }
            return;
        }
    } else {
        state->repeatCount = 0;
        if (--keyDownCount < 0)
            keyDownCount = 0;
    }

    if (coduompSpecialConsoleKey != qfalse) {
        const qboolean consoleAllowed =
            (cls.keyCatchers & KEYCATCH_CONSOLE) != 0 || sv_running->integer != 0 || sv_disableClientConsole->integer == 0;
        if (down != qfalse && consoleAllowed != qfalse)
            Con_ToggleConsole_f();
        return;
    }

    if (down != qfalse && (key < K_CHAR_MICRO || key == K_MOUSE1) &&
        (clc.demoPlayback != qfalse || cls.state == CA_CINEMATIC || cls.state == CA_LOGO) && cls.keyCatchers == 0) {
        (void)Cvar_Set2("nextdemo", "", qtrue);
        key = K_ESCAPE;
    }

    if (key == K_ESCAPE && down != qfalse) {
        if ((cls.keyCatchers & KEYCATCH_MESSAGE) != 0) {
            Message_Key(K_ESCAPE);
            return;
        }
        if ((cls.keyCatchers & KEYCATCH_CGAME) != 0) {
            cls.keyCatchers &= ~KEYCATCH_CGAME;
            (void)VM_Call(coduo_cgameVm, CGVM_EVENT_HANDLING, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
            return;
        }
        if ((cls.keyCatchers & KEYCATCH_UI) != 0) {
            (void)VM_Call(coduo_uiVm, UIVM_KEY_EVENT, K_ESCAPE, down, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
            return;
        }

        switch (cls.state) {
        case CA_CONNECTING:
        case CA_CHALLENGING:
        case CA_CONNECTED:
            CL_Disconnect(qtrue);
            if (sv_running->integer != 0)
                (void)Cvar_Set2("sv_killserver", "1", qtrue);
            return;

        case CA_ACTIVE:
            (void)VM_Call(coduo_uiVm, UIVM_SET_ACTIVE_MENU,
                          clc.demoPlayback != qfalse || cl_serverLoadWaiting->integer != 0 ? UI_MENU_MAIN : UI_MENU_INGAME, 0, 0, 0, 0, 0,
                          0, 0, 0, 0, 0, 0);
            return;

        case CA_CINEMATIC:
        case CA_LOGO:
            CL_Disconnect_f();
            MSS_StopSounds(MSS_STOP_ALL_SOUNDS);
            (void)VM_Call(coduo_uiVm, UIVM_SET_ACTIVE_MENU, UI_MENU_MAIN, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
            return;

        case CA_DISCONNECTED:
        case CA_LOADING:
        case CA_PRIMED:
            if (coduo_uiVm != NULL) {
                (void)VM_Call(coduo_uiVm, UIVM_SET_ACTIVE_MENU, UI_MENU_MAIN, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
            }
            return;
        }
    }

    if (coduo_cgameVm != NULL && VM_Call(coduo_cgameVm, CGVM_KEY_EVENT, key, down, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) != 0) {
        return;
    }

    if (down == qfalse) {
        const char *const binding = keyStates[key].binding;
        if (binding != NULL && binding[0] == '+') {
            char command[KEY_EVENT_COMMAND_SIZE];
            Com_sprintf(command, sizeof(command), "-%s %i %i\n", &binding[1], key, (int32_t)time);
            Cbuf_AddText(command);
        }

        if ((cls.keyCatchers & KEYCATCH_UI) != 0 && coduo_uiVm != NULL) {
            (void)VM_Call(coduo_uiVm, UIVM_KEY_EVENT, key, qfalse, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        } else if ((cls.keyCatchers & KEYCATCH_CGAME) != 0 && coduo_cgameVm != NULL) {
            (void)VM_Call(coduo_cgameVm, CGVM_LAST_ATTACKER, key, qfalse, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        }
        return;
    }

    qboolean bypassUi = qfalse;
    if (cl_bypassMouseInput != NULL && cl_bypassMouseInput->integer != 0) {
        if (key == K_MOUSE1 || key == K_MOUSE2 || key == K_MOUSE3) {
            bypassUi = cl_bypassMouseInput->integer == 1 ? qtrue : qfalse;
        } else if (UI_checkKeyExec(key) == 0) {
            bypassUi = qtrue;
        }
    }

    if ((cls.keyCatchers & KEYCATCH_CONSOLE) != 0) {
        Console_Key(key);
        return;
    }

    if ((cls.keyCatchers & KEYCATCH_UI) != 0 && bypassUi == qfalse) {
        const char *const binding = keyStates[key].binding;
        if (binding != NULL && Q_stricmp(binding, "help") == 0 &&
            VM_Call(coduo_uiVm, UIVM_GET_ACTIVE_MENU, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) == UI_MENU_HELP) {
            key = K_ESCAPE;
        }

        (void)VM_Call(coduo_uiVm, UIVM_KEY_EVENT, key, down, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        return;
    }

    if ((cls.keyCatchers & KEYCATCH_CGAME) != 0 && coduo_cgameVm != NULL) {
        (void)VM_Call(coduo_cgameVm, CGVM_LAST_ATTACKER, key, down, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        return;
    }

    if ((cls.keyCatchers & KEYCATCH_MESSAGE) != 0) {
        Message_Key(key);
        return;
    }

    if (cls.state == CA_DISCONNECTED) {
        Console_Key(key);
        return;
    }

    const char *const binding = keyStates[key].binding;
    if (binding == NULL) {
        if (key >= K_MOUSE1) {
            Com_Printf("%s is unbound, use controls menu to set.\n", Key_KeynumToString(key, qfalse));
        }
        return;
    }

    if (binding[0] == '+') {
        char command[KEY_EVENT_COMMAND_SIZE];
        Com_sprintf(command, sizeof(command), "%s %i %i\n", binding, key, (int32_t)time);
        Cbuf_AddText(command);
    } else {
        Cbuf_AddText(binding);
        Cbuf_AddText("\n");
    }
}

/* Source: CoDUOMP.exe 0x0040edc0..0x0040eee8.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040edc0_0040eee9.mcode.
 * Name and signature: exact same-module Mac symbol Key_WriteBindings. The
 * compiler inlined Key_KeynumToString into the loop; using the recovered
 * source routine preserves that same conversion behavior. */
void Key_WriteBindings(int32_t fileHandle)
{
    FS_Printf(fileHandle, "unbindall\n");

    for (int32_t key = 0; key < MAX_KEYS; ++key) {
        const char *const binding = keyStates[key].binding;
        if (binding != NULL && binding[0] != '\0') {
            FS_Printf(fileHandle, "bind %s \"%s\"\n", Key_KeynumToString(key, qfalse), binding);
        }
    }
}

/* Source: CoDUOMP.exe 0x0040f830..0x0040f888.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040f830_0040f889.mcode.
 * Name and signature: exact same-module Mac symbol CL_CharEvent. Catcher bits,
 * the UI character flag, UI VM command 3, and the disconnected-state fallback
 * are all direct machine-code operands. */
void CL_CharEvent(int32_t character)
{
    if (character == '`' || character == '~')
        return;

    if ((cls.keyCatchers & KEYCATCH_CONSOLE) != 0) {
        Field_CharEvent(&con_inputField, character);
        return;
    }

    if ((cls.keyCatchers & KEYCATCH_UI) != 0) {
        (void)VM_Call(coduo_uiVm, UIVM_KEY_EVENT, (intptr_t)(character | K_CHAR_FLAG), (intptr_t)qtrue, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        return;
    }

    if ((cls.keyCatchers & KEYCATCH_MESSAGE) != 0) {
        Field_CharEvent(&chatField, character);
        return;
    }

    if (cls.state == CA_DISCONNECTED)
        Field_CharEvent(&con_inputField, character);
}

/* Source: CoDUOMP.exe 0x0040f890..0x0040f8d0.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040f890_0040f8d1.mcode.
 * Name: exact same-module Mac symbol Key_ClearStates. Every held key first
 * receives the original CL_KeyEvent release before its local down/repeat
 * state is cleared. Bindings remain owned until Key_Shutdown. */
void Key_ClearStates(void)
{
    keyDownCount = 0;
    for (int32_t key = 0; key < MAX_KEYS; ++key) {
        if (keyStates[key].down != qfalse)
            CL_KeyEvent(key, qfalse, 0);
        keyStates[key].down = qfalse;
        keyStates[key].repeatCount = 0;
    }
}

/* Source: CoDUOMP.exe 0x0040d130..0x0040d48c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040d130_0040d48d.mcode.
 * Name and no-argument signature: exact same-module Mac symbol
 * CL_InitKeyCommands. Every command/handler pair and the two cvar stores
 * below are present in the original registration order. */
void CL_InitKeyCommands(void)
{
    Cmd_AddCommand("centerview", IN_CenterView);
    Cmd_AddCommand("+moveup", IN_UpDown);
    Cmd_AddCommand("-moveup", IN_UpUp);
    Cmd_AddCommand("+movedown", IN_DownDown);
    Cmd_AddCommand("-movedown", IN_DownUp);
    Cmd_AddCommand("+left", IN_LeftDown);
    Cmd_AddCommand("-left", IN_LeftUp);
    Cmd_AddCommand("+right", IN_RightDown);
    Cmd_AddCommand("-right", IN_RightUp);
    Cmd_AddCommand("+forward", IN_ForwardDown);
    Cmd_AddCommand("-forward", IN_ForwardUp);
    Cmd_AddCommand("+back", IN_BackDown);
    Cmd_AddCommand("-back", IN_BackUp);
    Cmd_AddCommand("+lookup", IN_LookupDown);
    Cmd_AddCommand("-lookup", IN_LookupUp);
    Cmd_AddCommand("+lookdown", IN_LookdownDown);
    Cmd_AddCommand("-lookdown", IN_LookdownUp);
    Cmd_AddCommand("+strafe", IN_StrafeDown);
    Cmd_AddCommand("-strafe", IN_StrafeUp);
    Cmd_AddCommand("+moveleft", IN_MoveleftDown);
    Cmd_AddCommand("-moveleft", IN_MoveleftUp);
    Cmd_AddCommand("+moveright", IN_MoverightDown);
    Cmd_AddCommand("-moveright", IN_MoverightUp);
    Cmd_AddCommand("+speed", IN_SpeedDown);
    Cmd_AddCommand("-speed", IN_SpeedUp);
    Cmd_AddCommand("+attack", IN_Button0Down);
    Cmd_AddCommand("-attack", IN_Button0Up);
    Cmd_AddCommand("+melee", IN_Button5Down);
    Cmd_AddCommand("-melee", IN_Button5Up);
    Cmd_AddCommand("+activate", IN_ActivateDown);
    Cmd_AddCommand("-activate", IN_ActivateUp);
    Cmd_AddCommand("+reload", IN_ReloadDown);
    Cmd_AddCommand("-reload", IN_ReloadUp);
    Cmd_AddCommand("+leanleft", IN_LeanLeftDown);
    Cmd_AddCommand("-leanleft", IN_LeanLeftUp);
    Cmd_AddCommand("+leanright", IN_LeanRightDown);
    Cmd_AddCommand("-leanright", IN_LeanRightUp);
    Cmd_AddCommand("+dropweapon", IN_MP_DropWeaponDown);
    Cmd_AddCommand("-dropweapon", IN_MP_DropWeaponUp);
    Cmd_AddCommand("+prone", IN_Wbutton6Down);
    Cmd_AddCommand("-prone", IN_Wbutton6Up);
    Cmd_AddCommand("+sprint", IN_SprintDown);
    Cmd_AddCommand("-sprint", IN_SprintUp);
    Cmd_AddCommand("+mlook", IN_MLookDown);
    Cmd_AddCommand("-mlook", IN_MLookUp);
    Cmd_AddCommand("lowerstance", IN_LowerStance);
    Cmd_AddCommand("raisestance", IN_RaiseStance);
    Cmd_AddCommand("togglecrouch", IN_ToggleCrouch);
    Cmd_AddCommand("toggleprone", IN_ToggleProne);
    Cmd_AddCommand("goprone", IN_GoProne);
    Cmd_AddCommand("gocrouch", IN_GoCrouch);
    Cmd_AddCommand("+gostand", IN_GoStandDown);
    Cmd_AddCommand("-gostand", IN_GoStandUp);

    cl_nodelta = Cvar_Get("cl_nodelta", "0", 0);
    cl_debugMove = Cvar_Get("cl_debugMove", "0", 0);
}

/* Source: CoDUOMP.exe 0x0040d490..0x0040d6ae.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040d490_0040d6af.mcode.
 * Provisional symmetric name: the function removes, in exact machine-code
 * order, every gameplay key command installed by CL_InitKeyCommands at
 * 0x0040d130. */
void CL_ShutdownKeyCommands(void)
{
    Cmd_RemoveCommand("centerview");
    Cmd_RemoveCommand("+moveup");
    Cmd_RemoveCommand("-moveup");
    Cmd_RemoveCommand("+movedown");
    Cmd_RemoveCommand("-movedown");
    Cmd_RemoveCommand("+left");
    Cmd_RemoveCommand("-left");
    Cmd_RemoveCommand("+right");
    Cmd_RemoveCommand("-right");
    Cmd_RemoveCommand("+forward");
    Cmd_RemoveCommand("-forward");
    Cmd_RemoveCommand("+back");
    Cmd_RemoveCommand("-back");
    Cmd_RemoveCommand("+lookup");
    Cmd_RemoveCommand("-lookup");
    Cmd_RemoveCommand("+lookdown");
    Cmd_RemoveCommand("-lookdown");
    Cmd_RemoveCommand("+strafe");
    Cmd_RemoveCommand("-strafe");
    Cmd_RemoveCommand("+moveleft");
    Cmd_RemoveCommand("-moveleft");
    Cmd_RemoveCommand("+moveright");
    Cmd_RemoveCommand("-moveright");
    Cmd_RemoveCommand("+speed");
    Cmd_RemoveCommand("-speed");
    Cmd_RemoveCommand("+attack");
    Cmd_RemoveCommand("-attack");
    Cmd_RemoveCommand("+melee");
    Cmd_RemoveCommand("-melee");
    Cmd_RemoveCommand("+activate");
    Cmd_RemoveCommand("-activate");
    Cmd_RemoveCommand("+reload");
    Cmd_RemoveCommand("-reload");
    Cmd_RemoveCommand("+leanleft");
    Cmd_RemoveCommand("-leanleft");
    Cmd_RemoveCommand("+leanright");
    Cmd_RemoveCommand("-leanright");
    Cmd_RemoveCommand("+dropweapon");
    Cmd_RemoveCommand("-dropweapon");
    Cmd_RemoveCommand("+prone");
    Cmd_RemoveCommand("-prone");
    Cmd_RemoveCommand("+sprint");
    Cmd_RemoveCommand("-sprint");
    Cmd_RemoveCommand("+mlook");
    Cmd_RemoveCommand("-mlook");
    Cmd_RemoveCommand("lowerstance");
    Cmd_RemoveCommand("raisestance");
    Cmd_RemoveCommand("togglecrouch");
    Cmd_RemoveCommand("toggleprone");
    Cmd_RemoveCommand("goprone");
    Cmd_RemoveCommand("gocrouch");
    Cmd_RemoveCommand("+gostand");
    Cmd_RemoveCommand("-gostand");
}

/* Source: CoDUOMP.exe 0x0040f8e0..0x0040f90c.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0040f8e0_0040f90d.mcode.
 * Provisional role name: shutdown ownership is proved by the 256-record,
 * 12-byte-stride scan that frees and clears each binding pointer. */
void Key_Shutdown(void)
{
    for (int32_t key = 0; key < MAX_KEYS; ++key) {
        free(keyStates[key].binding);
        keyStates[key].binding = NULL;
    }
}
