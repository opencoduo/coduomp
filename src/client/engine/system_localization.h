#ifndef CODUOMP_SYSTEM_LOCALIZATION_H
#define CODUOMP_SYSTEM_LOCALIZATION_H

#include "q_shared.h"

int32_t Sys_InitLocalization(void);
void Sys_ShutdownLocalization(void);
const char *Sys_LocalizeString(const char *reference);

#endif
