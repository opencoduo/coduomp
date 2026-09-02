#include "punkbuster_boundary.h"

#include "server_packet_services.h"

#include "qcommon/q_cvar.h"

/* Source: CoDUOMP.exe 0x004604d0..0x004604e1, recovered from an executable
 * gap after repairing the missing function boundary.
 * Name: exact same-module Mac symbol set_sv_punkbuster. */
void set_sv_punkbuster(const char *value)
{
    Cvar_Set("sv_punkbuster", value);
}

/* NOT_FROM_ORIGINAL_SOURCE: the shared connectionless-packet dispatcher uses
 * this target adapter at the retired Windows PunkBuster packet boundary. The
 * unavailable backend still consumes its packet class, as before sharing. */
void server_compat_handle_pb_packet(netadr_t from, msg_t *message)
{
    (void)from;
    (void)message;
}

/* NOT_FROM_ORIGINAL_SOURCE: disabled modern replacement for the optional
 * PunkBuster server event callback at CoDUOMP.exe 0x004bd500. The original
 * target belongs to the classified optional_punkbuster_server static-linkage
 * range; modern builds intentionally do not load its retired backend. */
void PB_InvokeEventCallback(const char *address, const uint8_t *packetData)
{
    (void)address;
    (void)packetData;
}

/* NOT_FROM_ORIGINAL_SOURCE: disabled modern replacement for the post-command
 * PunkBuster server drain reached through the classified static-linkage call
 * at CoDUOMP.exe 0x004bd130. Normal rcon command execution remains intact. */
void PB_CallServerSaCommandDrain(void)
{
}

/* NOT_FROM_ORIGINAL_SOURCE: retired-backend boundary replacing
 * PbClientInitialize at CoDUOMP.exe 0x004bbaa0. The retail WinMain calls it
 * only for a client process and passes the application HINSTANCE in EAX.
 * PunkBuster is intentionally absent from modern builds. */
void PB_InitializeClient(void *applicationInstance)
{
    (void)applicationInstance;
}

/* NOT_FROM_ORIGINAL_SOURCE: retired-backend boundary replacing
 * PbServerInitialize at CoDUOMP.exe 0x004bd3d0. The retail executable
 * initializes this optional backend even when running as a client. */
void PB_InitializeServer(void)
{
}

/* NOT_FROM_ORIGINAL_SOURCE: retired-backend boundary replacing the
 * PbClientProcessEvents state machine at CoDUOMP.exe 0x004bb1b0 and its
 * inlined WinMain caller at 0x0046c7a4..0x0046c835. */
void PB_ProcessClientEvents(void)
{
}

/* NOT_FROM_ORIGINAL_SOURCE: retired-backend boundary replacing the
 * PbServerProcessEvents state machine at CoDUOMP.exe 0x004bced0 and its
 * inlined WinMain caller at 0x0046c836..0x0046c8d4. */
void PB_ProcessServerEvents(void)
{
}

/* NOT_FROM_ORIGINAL_SOURCE: retired-backend boundary replacing the
 * EnablePbCl/DisablePbCl selection made by CLUI_SetPbClStatus at
 * CoDUOMP.exe 0x0041b430. */
void PB_SetClientEnabled(qboolean enabled)
{
    (void)enabled;
}

/* NOT_FROM_ORIGINAL_SOURCE: disabled replacement for the optional embedded
 * PbClientConnecting consumer called by CL_CheckForResend. The original
 * PunkBuster callback can inspect or alter the connection packet and length;
 * modern builds deliberately send the engine-built packet unchanged. */
void PbClientConnecting(pbClientConnectingEvent_t event, char *packet, int32_t *packetLength)
{
    (void)event;
    (void)packet;
    (void)packetLength;
}

/* NOT_FROM_ORIGINAL_SOURCE: retired-backend boundary replacing the optional
 * PbClientTrapConsole callback read from CoDUOMP.exe 0x005cef24 by
 * Cmd_ExecuteString at 0x0042c590. Returning false preserves normal command
 * dispatch when the retired PunkBuster client is unavailable. */
qboolean PB_ClientTrapConsole(const char *text)
{
    (void)text;
    return qfalse;
}

/* NOT_FROM_ORIGINAL_SOURCE: retired-backend boundary replacing the client
 * PunkBuster opcode-14 console-command dispatch at
 * CoDUOMP.exe 0x0042c6d0..0x0042c734. The command remains consumed by
 * Cmd_ExecuteString, as in the original when the optional backend is absent. */
void PB_DispatchClientConsoleCommand(const char *text)
{
    (void)text;
}

/* NOT_FROM_ORIGINAL_SOURCE: retired-backend boundary replacing the server
 * PunkBuster opcode-14 console-command dispatch at
 * CoDUOMP.exe 0x0042c641..0x0042c6cb. The command remains consumed by
 * Cmd_ExecuteString, as in the original when the optional backend is absent. */
void PB_DispatchServerConsoleCommand(const char *text)
{
    (void)text;
}

/* NOT_FROM_ORIGINAL_SOURCE: the retired PunkBuster backend contributed its
 * own console completion candidates. With that backend absent, preserve the
 * user's existing text and report no additional completion. */
void PbClientCompleteCommand(char *command, int32_t commandCapacity)
{
    (void)command;
    (void)commandCapacity;
}

/* NOT_FROM_ORIGINAL_SOURCE: server-side counterpart of the disabled
 * PunkBuster completion boundary above. */
void PbServerCompleteCommand(char *command, int32_t commandCapacity)
{
    (void)command;
    (void)commandCapacity;
}
