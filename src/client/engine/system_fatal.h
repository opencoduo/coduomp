#ifndef CODUOMP_SYSTEM_FATAL_H
#define CODUOMP_SYSTEM_FATAL_H

#ifdef __cplusplus
extern "C" {
#endif

void Sys_OutOfMemory(void);
int coduomp_config_recovery_dialog(const char *message, const char *backup);

#ifdef __cplusplus
}
#endif

#endif
