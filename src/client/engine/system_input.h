#ifndef CODUOMP_SYSTEM_INPUT_H
#define CODUOMP_SYSTEM_INPUT_H

#include "q_shared.h"

#if defined(_WIN32)
#include <windows.h>
extern HWND win32MainWindow; /* original 0x0489bb88 */
#endif

extern cvar_t *in_mouse; /* original 0x048a5484 */
extern cvar_t *in_midi; /* original 0x048a5480 */
extern cvar_t *in_midiport; /* original 0x048a54a4 */
extern cvar_t *in_midichannel; /* original 0x048a5488 */
extern cvar_t *in_mididevice; /* original 0x048a54a0 */
extern cvar_t *in_joystick; /* original 0x048a548c */
extern cvar_t *in_joyBallScale; /* original 0x048a5494 */
extern cvar_t *in_debugjoystick; /* original 0x048a5490 */
extern cvar_t *joy_threshold; /* original 0x048a549c */
extern qboolean sysInputAppActive; /* original 0x048a54a8 */

void IN_ActivateMouse(void);
void IN_DeactivateMouse(void);
void IN_MouseEvent(int32_t buttonMask);
void IN_MouseMove(void);
void IN_Activate(qboolean active);
void IN_ClearStates(void);
void IN_Init(void);
void IN_Shutdown(void);
void IN_Restart(void);
void IN_Frame(void);

#endif
