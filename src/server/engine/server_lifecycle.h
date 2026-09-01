#ifndef SHARED_SERVER_LIFECYCLE_H
#define SHARED_SERVER_LIFECYCLE_H

#ifdef __cplusplus
extern "C" {
#endif

void SV_Init(void);
void SV_FinalMessage(const char *message);
void SV_Shutdown(const char *finalMessage);

#ifdef __cplusplus
}
#endif

#endif
