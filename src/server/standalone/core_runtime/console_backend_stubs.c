#include "core_runtime_private.h"

/*
 * Mac MP symbols include CL_Shutdown; the Linux dedicated build keeps the
 * client shutdown hook as a no-op called from fatal error and quit paths.
 */
void CL_Shutdown(void)
{
}

void CL_MouseEvent(int32_t value,
                   int32_t value2,
                   int32_t time)
{
    (void)value;
    (void)value2;
    (void)time;
}

void Key_WriteBindings(int32_t handle)
{
    (void)handle;
}

void CL_Frame(int32_t rawMsec, int32_t scaledMsec)
{
    (void)rawMsec;
    (void)scaledMsec;
}

void CL_PacketEvent(netadr_t from, msg_t *msg,
                    int32_t time)
{
    (void)from;
    (void)msg;
    (void)time;
}

void CL_CharEvent(int32_t value)
{
    (void)value;
}

/*
 * Mac MP symbols include CL_Disconnect; the Linux dedicated binary calls this
 * no-op with qtrue from Com_Shutdown and qfalse from SV_Shutdown.
 */
void CL_Disconnect(qboolean showMainMenu)
{
    (void)showMainMenu;
}

/*
 * Quake III's dedicated null-client source places CL_MapLoading in this exact
 * function sequence between CL_Disconnect and CL_GameCommand.  The sole Linux
 * call is likewise the map-load transition in SV_SpawnServer, and the Mac MP
 * binary exports CL_MapLoading.  The dedicated variant takes no arguments and
 * intentionally does nothing.
 */
void CL_MapLoading(void)
{
}

qboolean CL_GameCommand(void)
{
    return qfalse;
}

void CL_KeyEvent(int32_t value,
                 int32_t value2,
                 int32_t time)
{
    (void)value;
    (void)value2;
    (void)time;
}

qboolean UI_GameCommand(void)
{
    return qfalse;
}

void CL_ForwardCommandToServer(const char *text)
{
    (void)text;
}

void CL_ConsolePrint(int32_t channel, const char *message,
                     int32_t arg2, int32_t arg3)
{
    (void)channel;
    (void)message;
    (void)arg2;
    (void)arg3;
}

void FUN_080851cd(void)
{
}

void CL_JoystickEvent(int32_t value,
                      int32_t value2,
                      int32_t time)
{
    (void)value;
    (void)value2;
    (void)time;
}

/*
 * Mac MP symbols include CL_InitKeyCommands; the dedicated build keeps the
 * early Com_Init client key-command registration hook as a no-op.
 */
void CL_InitKeyCommands(void)
{
}

/*
 * Mac MP symbols include CL_CDDialog; the dedicated build keeps the client
 * server-CD-error dialog hook as a no-op.
 */
void CL_CDDialog(void)
{
}

/*
 * Mac MP symbols include CL_StartHunkUsers; the dedicated build keeps the
 * common-init and post-hunk-clear client hunk-user hook as a no-op.
 */
void CL_StartHunkUsers(void)
{
}

/*
 * Mac MP symbols include CL_ShutdownAll; the dedicated build keeps the
 * full client shutdown hook as a no-op.
 */
void CL_ShutdownAll(void)
{
}

qboolean CL_CDKeyValidate(const char *cdkey, const char *hash)
{
    (void)cdkey;
    (void)hash;

    return qtrue;
}

void FUN_080851f5(void)
{
}

/*
 * Mac MP symbols include Sys_Input; the dedicated build keeps the per-event
 * input polling hook as a no-op.
 */
void Sys_Input(void)
{
}

/*
 * Mac MP symbols include Sys_SendKeyEvents; Sys_GetEvent calls this before
 * console input polling, and the dedicated build keeps it as a no-op.
 */
void Sys_SendKeyEvents(void)
{
}

void FUN_08085210(void)
{
}

void FUN_08085215(void)
{
}
