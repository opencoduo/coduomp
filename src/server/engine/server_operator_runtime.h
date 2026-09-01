#ifndef SHARED_SERVER_OPERATOR_RUNTIME_H
#define SHARED_SERVER_OPERATOR_RUNTIME_H

#ifdef __cplusplus
extern "C" {
#endif

void SV_Status_f(void);
void SV_ConSay_f(void);
void SV_ConTell_f(void);
void SV_Heartbeat_f(void);
void SV_Serverinfo_f(void);
void SV_Systeminfo_f(void);
void SV_DumpUser_f(void);
void SV_KillServer_f(void);
void SV_GameCompleteStatus_f(void);
void SV_ScriptUsage_f(void);
void SV_StringUsage_f(void);
void SV_SetDrawFriend_f(void);
void SV_SetFriendlyFire_f(void);
void SV_SetKillcam_f(void);
void SV_AddOperatorCommands(void);
void SV_RemoveOperatorCommands(void);
void SV_AddDedicatedCommands(void);
void SV_RemoveDedicatedCommands(void);

#ifdef __cplusplus
}
#endif

#endif
