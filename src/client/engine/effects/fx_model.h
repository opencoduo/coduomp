#ifndef CODUOMP_FX_MODEL_H
#define CODUOMP_FX_MODEL_H

#include "fx_archive.h"

#ifdef __cplusplus
extern "C" {
#endif

extern fx_model_registration_t *fxModelRegistrations;

void *CFxModel_Alloc(size_t size);
DObj *CFxModel_Register(const char *name);
DObj *CFxModel_DObjForHandle(DObj *handle);
const char *CFxModel_NameForDObj(const DObj *obj);
void CFxModel_Clean(void);

#ifdef __cplusplus
}
#endif

#endif
