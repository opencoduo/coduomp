#ifndef CODUOMP_HUNK_CLEAR_SERVICES_H
#define CODUOMP_HUNK_CLEAR_SERVICES_H

void CL_ShutdownCGame(void);
void CL_ShutdownUI(void);
void CIN_CloseAllVideos(void);
void SV_ShutdownGameProgs(void);
void VM_Clear(void);

#define HUNK_CLEAR_TO_START_PRE_SERVER() \
    do { \
        CL_ShutdownCGame(); \
        CL_ShutdownUI(); \
    } while (0)

#define HUNK_CLEAR_TO_START_POST_SERVER() CIN_CloseAllVideos()

#endif
