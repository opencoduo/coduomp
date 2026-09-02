#ifndef CODUOMP_SERVER_NETCHAN_PROFILE_SERVICES_H
#define CODUOMP_SERVER_NETCHAN_PROFILE_SERVICES_H

#include "qcommon/qcommon_runtime_types.h"

#include <stdint.h>

void SV_ProfDraw(const char *text, int32_t y);

#define SV_NETCHAN_PROFILE_DRAW(text, y) \
    SV_ProfDraw((text), (y))

#endif
