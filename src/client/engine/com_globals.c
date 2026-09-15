#include "q_shared.h"

/* Original common error-recursion guard at CoDUOMP.exe 0x0492909c. */
qboolean com_errorEntered;

/* Common frame timestamp at original Win32 0x04929090. The server snapshots
 * this into sv.serverId when spawning or restarting, while client input uses
 * it to derive the duration represented by each new user command. */
int32_t com_frameTime;

/* Current low-plus-high permanent hunk usage. The original allocator updates
 * this scalar at CoDUOMP.exe 0x049290a8 after each allocation/clear. */
int32_t hunk_used;

/* Original common frame-control slots at CoDUOMP.exe 0x04927eb8,
 * 0x04929064, and 0x0492906c. */
cvar_t *com_developer; /* original 0x04927ea4 */
cvar_t *com_developerScript; /* original 0x0492907c */
cvar_t *com_logfile;   /* original 0x04927ebc */
cvar_t *com_statmon;   /* original 0x04927ea8 */
cvar_t *com_viewlog; /* original 0x04927eb4 */
cvar_t *com_fixedtime;
cvar_t *com_speeds;
cvar_t *com_maxfps;       /* original 0x0492908c */
cvar_t *com_recommendedSet; /* original 0x04929094 */
cvar_t *com_introPlayed;  /* original 0x04929098 */
cvar_t *com_animCheck;    /* original 0x04929078 */
cvar_t *com_version;      /* original 0x04927f64, registered as "version" */
cvar_t *com_shortVersion; /* original 0x04929088, registered as "shortversion" */
qboolean com_configAutowriteEnabled; /* original 0x049290ac */
int32_t com_timeGame;
int32_t com_timeFrontend;
int32_t com_timeBackend;
