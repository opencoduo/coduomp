#ifndef CODUOMP_CLIENT_DOWNLOAD_H
#define CODUOMP_CLIENT_DOWNLOAD_H

#include "../q_shared.h"

typedef enum dlDownloadResult_e {
    DL_DOWNLOAD_CONTINUE = 0,
    DL_DOWNLOAD_SUCCESS = 1,
    DL_DOWNLOAD_FAILURE = 2
} dlDownloadResult_t;

void DL_InitDownload(void);
void DL_Shutdown(void);
qboolean DL_BeginDownload(char *localFileName, const char *remoteUrl);
dlDownloadResult_t DL_DownloadLoop(void);

#endif
