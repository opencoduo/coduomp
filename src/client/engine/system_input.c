#include "system_input.h"

#include "client/console.h"
#include "platform/crt_boundary.h"
#include "system_event.h"

#include <string.h>

#if defined(_WIN32)
#include <mmsystem.h>
#include <windows.h>
#else
#include "platform/sdl_platform.h"
#endif

enum {
    IN_MOUSE_BUTTON_COUNT = 3,
    IN_JOYSTICK_DIRECTION_COUNT = 16,
    IN_JOYSTICK_DIGITAL_AXIS_COUNT = 4,
    IN_JOYSTICK_BALL_AXIS_COUNT = 6,
    IN_JOYSTICK_AXIS_CENTER = 32768,
    IN_JOYSTICK_POV_FORWARD_BIT = 1 << 12,
    IN_JOYSTICK_POV_BACKWARD_BIT = 1 << 13,
    IN_JOYSTICK_POV_RIGHT_BIT = 1 << 14,
    IN_JOYSTICK_POV_LEFT_BIT = 1 << 15,
    IN_MIDI_DEVICE_CAPACITY = 8,
    IN_MIDI_KEY_OFFSET = 179,
    IN_MIDI_FIRST_KEY = 239,
    IN_MIDI_LAST_KEY = 255,
    IN_MIDI_STATUS_TYPE_MASK = 0xf0,
    IN_MIDI_CHANNEL_MASK = 0x0f,
    IN_MIDI_NOTE_ON = 0x90,
    IN_MIDI_NOTE_OFF = 0x80,
    IN_MIDI_DATA_BYTE_MASK = 0xff
};

cvar_t *in_mouse; /* original 0x048a5484 */
cvar_t *in_midi; /* original 0x048a5480 */
cvar_t *in_midiport; /* original 0x048a54a4 */
cvar_t *in_midichannel; /* original 0x048a5488 */
cvar_t *in_mididevice; /* original 0x048a54a0 */
cvar_t *in_joystick; /* original 0x048a548c */
cvar_t *in_joyBallScale; /* original 0x048a5494 */
cvar_t *in_debugjoystick; /* original 0x048a5490 */
cvar_t *joy_threshold; /* original 0x048a549c */

/* Set by the platform window/event layer. The original Win32 activation
 * handler at 0x0046fa90 writes this process-local state. */
qboolean sysInputAppActive; /* original 0x048a54a8 */

static int32_t windowCenterX;       /* original 0x009cdbb8 */
static int32_t windowCenterY;       /* original 0x009cda48 */
static int32_t previousMouseButtons;/* original 0x009cdbbc */
static qboolean mouseActive;        /* original 0x009cdbc0 */
static qboolean mouseInitialized;   /* original 0x009cdbc4 */
static int32_t midiDeviceCount;     /* original 0x009cda50 */

#if defined(_WIN32)
/* The Win32 window procedure installs this handle during WM_CREATE. It is a
 * process-local platform handle, not a serialized or cross-module ABI field. */
HWND win32MainWindow; /* original 0x0489bb88 */

static MIDIINCAPSA midiDeviceCaps[IN_MIDI_DEVICE_CAPACITY];
                                      /* original 0x009cda54..0x009cdbb3 */
static HMIDIIN midiInputHandle;       /* original 0x009cdbb4 */
static qboolean joystickAvailable;    /* original 0x009cdbc8 */
static int32_t joystickId;            /* original 0x009cdbcc */
static JOYCAPSA joystickCaps;         /* original 0x009cdbd0 */
static DWORD previousJoystickButtons; /* original 0x009cdd64 */
static uint32_t previousJoystickDirections; /* original 0x009cdd68 */
static JOYINFOEX joystickInfo;        /* original 0x009cdd6c */

_Static_assert(sizeof(MIDIINCAPSA) == 44, "original Win32 MIDI capabilities record size");
_Static_assert(sizeof(JOYCAPSA) == 404, "original Win32 joystick capabilities record size");
_Static_assert(sizeof(JOYINFOEX) == 52, "original Win32 joystick state record size");
#endif

static void IN_StartupMouse(void);
static void IN_StartupJoystick(void);
static void IN_JoyMove(void);
static void IN_StartupMIDI(void);
static void IN_ShutdownMIDI(void);
static void MidiInfo_f(void);

/* Source: CoDUOMP.exe 0x004699b0..0x00469a72.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004699b0_00469a73.mcode.
 * Role name: this is the Win32 cursor-capture body reached by
 * IN_ActivateMouse. */
static void IN_ActivateWin32Mouse(void)
{
#if defined(_WIN32)
    RECT windowRect;
    const int32_t screenWidth = GetSystemMetrics(SM_CXSCREEN);
    const int32_t screenHeight = GetSystemMetrics(SM_CYSCREEN);

    GetWindowRect(win32MainWindow, &windowRect);
    if (windowRect.left < 0)
        windowRect.left = 0;
    if (windowRect.top < 0)
        windowRect.top = 0;
    if (windowRect.right >= screenWidth)
        windowRect.right = screenWidth - 1;
    if (windowRect.bottom >= screenHeight)
        windowRect.bottom = screenHeight - 1;

    windowCenterX = (windowRect.right + windowRect.left) / 2;
    windowCenterY = (windowRect.bottom + windowRect.top) / 2;
    SetCursorPos(windowCenterX, windowCenterY);
    SetCapture(win32MainWindow);
    if (com_developer->integer == 0)
        ClipCursor(&windowRect);
    while (ShowCursor(FALSE) >= 0) {
    }
#else
    CoduoSDL_SetRelativeMouse(qtrue);
    windowCenterX = 0;
    windowCenterY = 0;
#endif
}

/* Source: CoDUOMP.exe 0x00469a80..0x00469aaa.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00469a80_00469aab.mcode.
 * Role name: inverse Win32 cursor-capture body used by every mouse
 * deactivation path. */
static void IN_DeactivateWin32Mouse(void)
{
#if defined(_WIN32)
    if (com_developer->integer == 0)
        ClipCursor(NULL);
    ReleaseCapture();
    while (ShowCursor(TRUE) < 0) {
    }
#else
    CoduoSDL_SetRelativeMouse(qfalse);
#endif
}

/* Source: CoDUOMP.exe 0x00469ab0..0x00469af5.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00469ab0_00469af6.mcode.
 * Role name: obtains the Win32 cursor displacement and recenters it. */
static void IN_Win32Mouse(int32_t *deltaX, int32_t *deltaY)
{
#if defined(_WIN32)
    POINT cursor;

    GetCursorPos(&cursor);
    SetCursorPos(windowCenterX, windowCenterY);
    *deltaX = cursor.x - windowCenterX;
    *deltaY = cursor.y - windowCenterY;
#else
    /* NOT_FROM_ORIGINAL_SOURCE: non-Windows backends enqueue their native
     * relative-motion events directly. */
    *deltaX = 0;
    *deltaY = 0;
#endif
}

/* Source: CoDUOMP.exe 0x00469b00..0x00469b37.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00469b00_00469b38.mcode.
 * Name: established id-engine platform input interface. */
void IN_ActivateMouse(void)
{
    if (mouseInitialized == qfalse)
        return;
    if (in_mouse->integer == 0) {
        mouseActive = qfalse;
        return;
    }
    if (mouseActive == qfalse) {
        mouseActive = qtrue;
        IN_ActivateWin32Mouse();
    }
}

/* Source: CoDUOMP.exe 0x00469b40..0x00469b61, recovered from an executable
 * gap after repairing the missing Ghidra function entry.
 * Name: established id-engine platform input interface. */
void IN_DeactivateMouse(void)
{
    if (mouseInitialized != qfalse && mouseActive != qfalse) {
        mouseActive = qfalse;
        IN_DeactivateWin32Mouse();
    }
}

/* Source: CoDUOMP.exe 0x00469b70..0x00469b9e, recovered from an executable
 * gap after repairing the missing Ghidra function entry.
 * Name: established id-engine platform input interface. */
static void IN_StartupMouse(void)
{
    mouseInitialized = qfalse;
    if (in_mouse->integer == 0) {
        Com_Printf("Mouse control not active.\n");
        return;
    }
    mouseInitialized = qtrue;
}

/* Source: CoDUOMP.exe 0x00469ba0..0x00469c1e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00469ba0_00469c1f.mcode.
 * Name and button-mask contract: established Win32 id-engine input
 * interface; the original handles the first three mouse buttons. */
void IN_MouseEvent(int32_t buttonMask)
{
    if (mouseInitialized == qfalse)
        return;

    for (int32_t button = 0; button < IN_MOUSE_BUTTON_COUNT; ++button) {
        const int32_t buttonBit = 1 << button;
        if ((buttonMask & buttonBit) != 0 && (previousMouseButtons & buttonBit) == 0) {
            Sys_QueEvent(sysMsgTime, SE_KEY, K_MOUSE1 + button, qtrue, 0, NULL);
        }
        if ((buttonMask & buttonBit) == 0 && (previousMouseButtons & buttonBit) != 0) {
            Sys_QueEvent(sysMsgTime, SE_KEY, K_MOUSE1 + button, qfalse, 0, NULL);
        }
    }
    previousMouseButtons = buttonMask;
}

/* Source: CoDUOMP.exe 0x00469c20..0x00469c75.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00469c20_00469c76.mcode.
 * Name: established id-engine platform input interface. The compiler inlined
 * IN_Win32Mouse into this body in the shipped executable. */
void IN_MouseMove(void)
{
    int32_t deltaX;
    int32_t deltaY;

    IN_Win32Mouse(&deltaX, &deltaY);
    if (deltaX != 0 || deltaY != 0)
        Sys_QueEvent(0, SE_MOUSE, deltaX, deltaY, 0, NULL);
}

/* Source: CoDUOMP.exe 0x00469c80..0x00469cd3.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00469c80_00469cd4.mcode.
 * Name: established id-engine platform input interface. The compiler inlined
 * IN_StartupMouse into the shipped body. */
static void IN_Startup(void)
{
    IN_StartupMouse();
    IN_StartupJoystick();
    IN_StartupMIDI();
    in_mouse->modified = qfalse;
    in_joystick->modified = qfalse;
}

/* Source: CoDUOMP.exe 0x00469ce0..0x00469d2e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00469ce0_00469d2f.mcode.
 * Name: established id-engine platform input interface. */
void IN_Shutdown(void)
{
    IN_DeactivateMouse();
    IN_ShutdownMIDI();
    Cmd_RemoveCommand("midiinfo");
}

/* Source: CoDUOMP.exe 0x0046bdf0..0x0046bdf9, recovered from the executable
 * gap immediately before Sys_Init.
 * Name: established id-engine input command. The original is the two-call
 * shutdown/reinitialize sequence registered as "in_restart" by Sys_Init. */
void IN_Restart(void)
{
    IN_Shutdown();
    IN_Init();
}

/* Source: CoDUOMP.exe 0x00469d30..0x00469e12.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00469d30_00469e13.mcode.
 * Name: established id-engine platform input interface. */
void IN_Init(void)
{
    in_midi = Cvar_Get("in_midi", "0", CVAR_ARCHIVE);
    in_midiport = Cvar_Get("in_midiport", "-1", CVAR_ARCHIVE);
    in_midichannel = Cvar_Get("in_midichannel", "-1", CVAR_ARCHIVE);
    in_mididevice = Cvar_Get("in_mididevice", "0", CVAR_ARCHIVE);
    Cmd_AddCommand("midiinfo", MidiInfo_f);

    in_mouse = Cvar_Get("in_mouse", "1", CVAR_ARCHIVE | CVAR_LATCH);
    in_joystick = Cvar_Get("in_joystick", "0", CVAR_ARCHIVE | CVAR_LATCH);
    in_joyBallScale = Cvar_Get("in_joyBallScale", "0.02", CVAR_ARCHIVE);
    in_debugjoystick = Cvar_Get("in_debugjoystick", "0", CVAR_TEMP);
    joy_threshold = Cvar_Get("joy_threshold", "0.15", CVAR_ARCHIVE);

    IN_Startup();
}

/* Source: CoDUOMP.exe 0x00469e20..0x00469e4a, recovered from an executable
 * gap after repairing the missing Ghidra function entry.
 * Name: established id-engine platform input interface. The original receives
 * active in EAX and inlines IN_DeactivateMouse. */
void IN_Activate(qboolean active)
{
    sysInputAppActive = active;
    if (active == qfalse)
        IN_DeactivateMouse();
}

/* Source: CoDUOMP.exe 0x00469e50..0x00469ebe.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00469e50_00469ebf.mcode.
 * Name: established id-engine platform input interface. */
void IN_Frame(void)
{
    IN_JoyMove();
    if (mouseInitialized == qfalse)
        return;

    if ((cls.keyCatchers & KEYCATCH_CONSOLE) != 0) {
        cvar_t *const fullscreen = Cvar_FindVar("r_fullscreen");
        if (fullscreen == NULL || fullscreen->value == 0.0f) {
            IN_DeactivateMouse();
            return;
        }
    }

    if (sysInputAppActive != qfalse) {
        IN_ActivateMouse();
        IN_MouseMove();
    } else {
        IN_DeactivateMouse();
    }
}

/* Source: CoDUOMP.exe 0x00469ec0..0x00469eca, recovered from an executable
 * gap after repairing the missing Ghidra function entry.
 * Name: established id-engine platform input interface. */
void IN_ClearStates(void)
{
    previousMouseButtons = 0;
}

/* Source: CoDUOMP.exe 0x00469ed0..0x0046a067.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00469ed0_0046a068.mcode.
 * Name: established id-engine WinMM input interface. */
static void IN_StartupJoystick(void)
{
#if defined(_WIN32)
    joystickAvailable = qfalse;
    if (in_joystick->integer == 0)
        return;

    const int32_t joystickCount = (int32_t)joyGetNumDevs();
    if (joystickCount == 0) {
        Com_DPrintf("joystick not found -- driver not present\n");
        return;
    }

    MMRESULT result = JOYERR_NOERROR;
    for (joystickId = 0; joystickId < joystickCount; ++joystickId) {
        memset(&joystickInfo, 0, sizeof(joystickInfo));
        joystickInfo.dwSize = sizeof(joystickInfo);
        joystickInfo.dwFlags = JOY_RETURNCENTERED;
        result = joyGetPosEx((UINT)joystickId, &joystickInfo);
        if (result == JOYERR_NOERROR)
            break;
    }
    if (joystickId == joystickCount) {
        Com_Printf("joystick not found -- no valid joysticks (%x)\n", result);
        return;
    }

    memset(&joystickCaps, 0, sizeof(joystickCaps));
    result = joyGetDevCapsA((UINT_PTR)joystickId, &joystickCaps, sizeof(joystickCaps));
    if (result != JOYERR_NOERROR) {
        Com_Printf("joystick not found -- invalid joystick capabilities (%x)\n", result);
        return;
    }

    Com_DPrintf("Joystick found.\n");
    Com_DPrintf("Pname: %s\n", joystickCaps.szPname);
    Com_DPrintf("OemVxD: %s\n", joystickCaps.szOEMVxD);
    Com_DPrintf("RegKey: %s\n", joystickCaps.szRegKey);
    Com_DPrintf("Numbuttons: %i / %i\n", (int32_t)joystickCaps.wNumButtons, (int32_t)joystickCaps.wMaxButtons);
    Com_DPrintf("Axis: %i / %i\n", (int32_t)joystickCaps.wNumAxes, (int32_t)joystickCaps.wMaxAxes);
    Com_DPrintf("Caps: 0x%x\n", joystickCaps.wCaps);
    Com_DPrintf((joystickCaps.wCaps & JOYCAPS_HASPOV) != 0 ? "HASPOV\n" : "no POV\n");

    previousJoystickButtons = 0;
    previousJoystickDirections = 0;
    joystickAvailable = qtrue;
#else
    /* NOT_FROM_ORIGINAL_SOURCE: WinMM joystick discovery has no direct
     * non-Windows equivalent; a native input backend will enqueue events. */
#endif
}

#if defined(_WIN32)
/* Source: CoDUOMP.exe 0x0046a070..0x0046a0af.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046a070_0046a0b0.mcode.
 * Role name: converts a centered unsigned WinMM axis to [-1, 1]. */
static float JoyToF(DWORD axisValue)
{
    /* Exact 0x38000000 float from 0x005b9b68: 1 / 32768. */
    const float centeredAxisScale = 0.000030517578125f;
    float value = (int32_t)(axisValue - IN_JOYSTICK_AXIS_CENTER) * centeredAxisScale;
    if (value < -1.0f)
        value = -1.0f;
    if (value > 1.0f)
        value = 1.0f;
    return value;
}

/* Source: CoDUOMP.exe 0x0046a0b0..0x0046a0b5, recovered from an executable
 * gap after repairing the missing Ghidra function entry.
 * Role name: returns the raw signed displacement of a WinMM axis. */
static int32_t JoyToI(DWORD axisValue)
{
    return (int32_t)(axisValue - IN_JOYSTICK_AXIS_CENTER);
}

static DWORD coduomp_joystick_axis_value(int32_t axisIndex)
{
    /* NOT_FROM_ORIGINAL_SOURCE: typed source-level selection for the four
     * contiguous JOYINFOEX axes read by the original indexed load. */
    switch (axisIndex) {
    case 0:
        return joystickInfo.dwXpos;
    case 1:
        return joystickInfo.dwYpos;
    case 2:
        return joystickInfo.dwZpos;
    default:
        return joystickInfo.dwRpos;
    }
}

static const int32_t joystickDirectionKeys[IN_JOYSTICK_DIRECTION_COUNT] = {
    K_LEFTARROW, K_RIGHTARROW, K_UPARROW, K_DOWNARROW, K_JOY16, K_JOY17, K_JOY18, K_JOY19,
    K_JOY20,     K_JOY21,      K_JOY22,   K_JOY23,     K_JOY24, K_JOY25, K_JOY26, K_JOY27};
#endif

/* Source: CoDUOMP.exe 0x0046a0c0..0x0046a3c5.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046a0c0_0046a3c6.mcode.
 * Name: established id-engine WinMM input interface. */
static void IN_JoyMove(void)
{
#if defined(_WIN32)
    if (joystickAvailable == qfalse)
        return;

    memset(&joystickInfo, 0, sizeof(joystickInfo));
    joystickInfo.dwSize = sizeof(joystickInfo);
    joystickInfo.dwFlags = JOY_RETURNALL;
    if (joyGetPosEx(joystickId, &joystickInfo) != JOYERR_NOERROR)
        return;

    if (in_debugjoystick->integer != 0) {
        Com_Printf("%8x %5i %5.2f %5.2f %5.2f %5.2f %6i %6i\n", joystickInfo.dwButtons, (int32_t)joystickInfo.dwPOV,
                   JoyToF(joystickInfo.dwXpos), JoyToF(joystickInfo.dwYpos), JoyToF(joystickInfo.dwZpos), JoyToF(joystickInfo.dwRpos),
                   JoyToI(joystickInfo.dwUpos), JoyToI(joystickInfo.dwVpos));
    }

    for (UINT button = 0; button < joystickCaps.wNumButtons; ++button) {
        const DWORD buttonBit = 1U << button;
        if ((joystickInfo.dwButtons & buttonBit) != 0 && (previousJoystickButtons & buttonBit) == 0) {
            Sys_QueEvent(sysMsgTime, SE_KEY, K_JOY1 + (int32_t)button, qtrue, 0, NULL);
        }
        if ((joystickInfo.dwButtons & buttonBit) == 0 && (previousJoystickButtons & buttonBit) != 0) {
            Sys_QueEvent(sysMsgTime, SE_KEY, K_JOY1 + (int32_t)button, qfalse, 0, NULL);
        }
    }
    previousJoystickButtons = joystickInfo.dwButtons;

    uint32_t directions = 0;
    const int32_t digitalAxisCount =
        joystickCaps.wNumAxes < IN_JOYSTICK_DIGITAL_AXIS_COUNT ? (int32_t)joystickCaps.wNumAxes : IN_JOYSTICK_DIGITAL_AXIS_COUNT;
    for (int32_t axis = 0; axis < digitalAxisCount; ++axis) {
        const float value = JoyToF(coduomp_joystick_axis_value(axis));
        if (value < -joy_threshold->value)
            directions |= 1U << (axis * 2);
        else if (value > joy_threshold->value)
            directions |= 1U << (axis * 2 + 1);
    }

    if ((joystickCaps.wCaps & JOYCAPS_HASPOV) != 0 && joystickInfo.dwPOV != JOY_POVCENTERED) {
        if (joystickInfo.dwPOV == JOY_POVFORWARD)
            directions |= IN_JOYSTICK_POV_FORWARD_BIT;
        else if (joystickInfo.dwPOV == JOY_POVBACKWARD)
            directions |= IN_JOYSTICK_POV_BACKWARD_BIT;
        else if (joystickInfo.dwPOV == JOY_POVRIGHT)
            directions |= IN_JOYSTICK_POV_RIGHT_BIT;
        else if (joystickInfo.dwPOV == JOY_POVLEFT)
            directions |= IN_JOYSTICK_POV_LEFT_BIT;
    }

    for (int32_t direction = 0; direction < IN_JOYSTICK_DIRECTION_COUNT; ++direction) {
        const uint32_t directionBit = 1U << direction;
        if ((directions & directionBit) != 0 && (previousJoystickDirections & directionBit) == 0) {
            Sys_QueEvent(sysMsgTime, SE_KEY, joystickDirectionKeys[direction], qtrue, 0, NULL);
        }
        if ((directions & directionBit) == 0 && (previousJoystickDirections & directionBit) != 0) {
            Sys_QueEvent(sysMsgTime, SE_KEY, joystickDirectionKeys[direction], qfalse, 0, NULL);
        }
    }
    previousJoystickDirections = directions;

    if (joystickCaps.wNumAxes >= IN_JOYSTICK_BALL_AXIS_COUNT) {
        const int32_t deltaX = (int32_t)(JoyToI(joystickInfo.dwUpos) * in_joyBallScale->value);
        const int32_t deltaY = (int32_t)(JoyToI(joystickInfo.dwVpos) * in_joyBallScale->value);
        if (deltaX != 0 || deltaY != 0)
            Sys_QueEvent(sysMsgTime, SE_MOUSE, deltaX, deltaY, 0, NULL);
    }
#else
    /* NOT_FROM_ORIGINAL_SOURCE: native backends provide joystick events
     * directly rather than polling WinMM here. */
#endif
}

#if defined(_WIN32)
/* Source: CoDUOMP.exe 0x0046a3d0..0x0046a3f9.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046a3d0_0046a3fa.mcode.
 * Role name: translates a MIDI note-off message to an engine key release. */
static void MIDI_NoteOff(int32_t note)
{
    const int32_t key = note + IN_MIDI_KEY_OFFSET;
    if (key >= IN_MIDI_FIRST_KEY && key <= IN_MIDI_LAST_KEY)
        Sys_QueEvent(sysMsgTime, SE_KEY, key, qfalse, 0, NULL);
}

/* Source: CoDUOMP.exe 0x0046a400..0x0046a458.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046a400_0046a459.mcode.
 * Role name: translates a MIDI note-on message to engine key events. */
static void MIDI_NoteOn(int32_t note, int32_t velocity)
{
    const int32_t key = note + IN_MIDI_KEY_OFFSET;
    if (velocity == 0)
        MIDI_NoteOff(note);

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (key >= IN_MIDI_FIRST_KEY && key <= IN_MIDI_LAST_KEY)
        Sys_QueEvent(sysMsgTime, SE_KEY, key, qtrue, 0, NULL);
}

/* Source: CoDUOMP.exe 0x0046a460..0x0046a4cf, recovered from an executable
 * gap after repairing the missing Ghidra function entry.
 * Role name: WinMM input callback installed by IN_StartupMIDI. */
static void CALLBACK MIDI_InputCallback(HMIDIIN inputHandle, UINT message, DWORD_PTR instance, DWORD_PTR messageData, DWORD_PTR timestamp)
{
    (void)inputHandle;
    (void)instance;
    (void)timestamp;

    if (message != MIM_DATA)
        return;

    const uint32_t packedMessage = (uint32_t)messageData;
    const uint8_t status = (uint8_t)packedMessage;
    const uint8_t messageType = status & IN_MIDI_STATUS_TYPE_MASK;
    if (messageType == IN_MIDI_NOTE_ON) {
        const int32_t channel = (status & IN_MIDI_CHANNEL_MASK) + 1;
        if (channel != in_midichannel->integer)
            return;
        const int32_t note = (packedMessage >> 8) & IN_MIDI_DATA_BYTE_MASK;
        const int32_t velocity = (packedMessage >> 16) & IN_MIDI_DATA_BYTE_MASK;
        MIDI_NoteOn(note, velocity);
    } else if (messageType == IN_MIDI_NOTE_OFF) {
        const int32_t channel = (status & IN_MIDI_CHANNEL_MASK) + 1;
        if (channel != in_midichannel->integer)
            return;
        const int32_t note = (packedMessage >> 8) & IN_MIDI_DATA_BYTE_MASK;
        MIDI_NoteOff(note);
    }
}
#endif

/* Source: CoDUOMP.exe 0x0046a4d0..0x0046a602.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046a4d0_0046a603.mcode.
 * Name: command string and behavior establish the console command role. */
static void MidiInfo_f(void)
{
    Com_Printf("\nMIDI control:       %s\n", in_midi->integer != 0 ? "enabled" : "disabled");
    Com_Printf("port:               %d\n", in_midiport->integer);
    Com_Printf("channel:            %d\n", in_midichannel->integer);
    Com_Printf("current device:     %d\n", in_mididevice->integer);
    Com_Printf("number of devices:  %d\n", midiDeviceCount);

#if defined(_WIN32)
    for (int32_t device = 0; device < midiDeviceCount; ++device) {
        cvar_t *const selectedDevice = Cvar_FindVar("in_mididevice");
        const float selectedValue = selectedDevice != NULL ? selectedDevice->value : 0.0f;
        Com_Printf(device == selectedValue ? "***" : "");
        Com_Printf("device %2d:       %s\n", device, midiDeviceCaps[device].szPname);
        Com_Printf("...manufacturer ID: 0x%hx\n", midiDeviceCaps[device].wMid);
        Com_Printf("...product ID:      0x%hx\n", midiDeviceCaps[device].wPid);
        Com_Printf("\n");
    }
#endif
}

/* Source: CoDUOMP.exe 0x0046a610..0x0046a6e0.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0046a610_0046a6e1.mcode.
 * Name: established id-engine WinMM input interface. */
static void IN_StartupMIDI(void)
{
#if defined(_WIN32)
    cvar_t *const midiEnabled = Cvar_FindVar("in_midi");
    if (midiEnabled == NULL || midiEnabled->value == 0.0f)
        return;

    midiDeviceCount = (int32_t)midiInGetNumDevs();
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (midiDeviceCount > IN_MIDI_DEVICE_CAPACITY) {
        Com_Printf("WARNING: ignoring MIDI devices beyond the %i-device "
                   "capacity\n",
                   IN_MIDI_DEVICE_CAPACITY);
        midiDeviceCount = IN_MIDI_DEVICE_CAPACITY;
    }
    for (int32_t device = 0; device < midiDeviceCount; ++device) {
        midiInGetDevCapsA((UINT_PTR)device, &midiDeviceCaps[device], sizeof(midiDeviceCaps[device]));
    }

    const int32_t device = in_mididevice->integer;
    const MMRESULT result = midiInOpen(&midiInputHandle, (UINT)device, (DWORD_PTR)MIDI_InputCallback, 0, CALLBACK_FUNCTION);
    if (result == MMSYSERR_NOERROR) {
        midiInStart(midiInputHandle);
        return;
    }

    const int32_t capsIndex = coduo_fp_to_i32_extended((long double)in_mididevice->value);
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    const char *const deviceName = capsIndex >= 0 && capsIndex < midiDeviceCount ? midiDeviceCaps[capsIndex].szPname : "unknown";
    Com_Printf("WARNING: could not open MIDI device %d: '%s'\n", device, deviceName);
#else
    /* NOT_FROM_ORIGINAL_SOURCE: native MIDI integration is deferred to the
     * platform input backend; the original implementation is WinMM-only. */
    midiDeviceCount = 0;
#endif
}

/* Source: CoDUOMP.exe 0x0046a6f0..0x0046a710, recovered from an executable
 * gap after repairing the missing Ghidra function entry.
 * Name: established id-engine WinMM input interface. */
static void IN_ShutdownMIDI(void)
{
#if defined(_WIN32)
    if (midiInputHandle != NULL)
        midiInClose(midiInputHandle);
    memset(midiDeviceCaps, 0, sizeof(midiDeviceCaps));
    midiInputHandle = NULL;
#else
    /* NOT_FROM_ORIGINAL_SOURCE: no WinMM handle exists on native platforms. */
#endif
    midiDeviceCount = 0;
}
