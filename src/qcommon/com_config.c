#include "com_config.h"
#include "config_profile.h"
#include "config_script.h"

#include "filesystem/filesystem.h"
#include "q_command.h"
#include "q_cvar.h"
#include "q_path.h"
#include "q_string.h"

#include <stdint.h>
#include <string.h>

void Com_Printf(const char *format, ...);

/*
 * Common configuration-writing entry points. Their original Windows client
 * and Linux dedicated engine bodies share the same decisions and call order:
 *
 *   CoDUOMP.exe   0x0043c260..0x0043c4b0
 *   coduo_lnxded  0x08071b45..0x08071d2f
 *
 * The same-module Mac client exports the canonical function names.  The one
 * target behavior difference is the automatically written filename: Windows
 * uses uoconfig_mp.cfg, while the Linux dedicated server uses
 * uoconfig_mp_server.cfg.  The explicit writeconfig and writedefaults commands
 * use their supplied filename identically on both targets.
 * NOT_FROM_ORIGINAL_SOURCE: the improved product keeps these entry points
 * while routing persistence through checked snapshots and transactions.
 */

/* Source: CoDUOMP.exe 0x0043c260..0x0043c2a2; coduo_lnxded
 * 0x08071b45..0x08071ba9. Exact same-module Mac symbol:
 * Com_WriteConfigToFile. */
void Com_WriteConfigToFile(const char *filename)
{
    /* NOT_FROM_ORIGINAL_SOURCE: the improved product uses checked snapshots
     * and adjacent-file transactions for every configuration write. */
    (void)coduomp_config_save(NULL, NULL, filename, qfalse);
}

/* Source: CoDUOMP.exe 0x0043c2b0..0x0043c2ec; coduo_lnxded
 * 0x08071baa..0x08071c03. Exact same-module Mac symbol:
 * Com_WriteDefaultsToFile. Defaults contain cvars only; the regular config
 * writer above additionally writes bindings. */
void Com_WriteDefaultsToFile(const char *filename)
{
    /* NOT_FROM_ORIGINAL_SOURCE: the improved product uses checked snapshots
     * and adjacent-file transactions for every configuration write. */
    (void)coduomp_config_save(NULL, NULL, filename, qtrue);
}

/* Source: CoDUOMP.exe 0x0043c2f0..0x0043c313; coduo_lnxded
 * 0x08071c04..0x08071c37. Exact same-module Mac symbol:
 * Com_WriteConfiguration. The Windows optimizer also inlines this helper at
 * the start of Com_Frame. */
void Com_WriteConfiguration(void)
{
    /* NOT_FROM_ORIGINAL_SOURCE: collect checked saves and coalesce new ones;
     * the dirty revision is acknowledged only after successful publication. */
    coduomp_config_service();
}

/* Source: CoDUOMP.exe 0x0043c320..0x0043c3e6; coduo_lnxded
 * 0x08071c38..0x08071cb3. Exact same-module Mac symbol: Com_WriteConfig_f.
 * The Windows compiler inlined Com_WriteConfigToFile into this command
 * callback. */
void Com_WriteConfig_f(void)
{
    char filename[MAX_QPATH];

    if (Cmd_Argc() != 2) {
        Com_Printf("Usage: writeconfig <filename>\n");
        return;
    }

    if (!coduomp_config_filename(Cmd_Argv(1), filename, sizeof(filename))) {
        Com_Printf("Config filename is too long.\n");
        return;
    }
    Com_Printf("Writing %s.\n", filename);
    Com_WriteConfigToFile(filename);
}

/* Source: CoDUOMP.exe 0x0043c3f0..0x0043c4b0; coduo_lnxded
 * 0x08071cb4..0x08071d2f. Exact same-module Mac symbol:
 * Com_WriteDefaults_f. */
void Com_WriteDefaults_f(void)
{
    char filename[MAX_QPATH];

    if (Cmd_Argc() != 2) {
        Com_Printf("Usage: writedefaults <filename>\n");
        return;
    }

    if (!coduomp_config_filename(Cmd_Argv(1), filename, sizeof(filename))) {
        Com_Printf("Config filename is too long.\n");
        return;
    }
    Com_Printf("Writing %s.\n", filename);
    Com_WriteDefaultsToFile(filename);
}
