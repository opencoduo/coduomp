#ifndef ENGINE_HUNK_CLEAR_SERVICES_H
#define ENGINE_HUNK_CLEAR_SERVICES_H

void SV_ShutdownGameProgs(void);
void VM_Clear(void);

#define HUNK_CLEAR_TO_START_PRE_SERVER() \
    do { \
    } while (0)

#define HUNK_CLEAR_TO_START_POST_SERVER() \
    do { \
    } while (0)

#endif
