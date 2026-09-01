#include "server_xmodel.h"

#include "qcommon/hunk.h"
#include "qcommon/q_shared_types.h"
#include "qcommon/q_string.h"
#include "animation/xmodel.h"

#include <stddef.h>
#include <stdint.h>

enum {
    SERVER_XMODEL_PREFIX_LENGTH = 6
};

void Com_Error(errorParm_t code, const char *format, ...);

/*
 * The game syscall's server-model lookup wrapper is shared by the Windows
 * client/listen server and Linux dedicated engine:
 *
 *   CoDUOMP.exe   0x0045ce90..0x0045ced3
 *   coduo_lnxded  0x0808e963..0x0808e9e2
 *
 * Both validate the six-character "xmodel" prefix case-insensitively,
 * require the same slash separator, skip seven bytes, request parts-only
 * loading, and supply Hunk_AllocXModelPrecache as the sole allocator. Windows
 * calls its linked _strnicmp and Linux calls strncasecmp; Q_stricmpn preserves
 * that common predicate for this fixed ASCII prefix without a target fork.
 * The supporting Mac client exports the canonical SV_XModelGet name.
 */
XModel *SV_XModelGet(const char *name)
{
    if (Q_stricmpn(name, "xmodel", SERVER_XMODEL_PREFIX_LENGTH) != 0 ||
        (name[SERVER_XMODEL_PREFIX_LENGTH] != '/' &&
         name[SERVER_XMODEL_PREFIX_LENGTH] != '\\')) {
        Com_Error(ERR_DROP, "\x15" "bad model name '%s'", name);
    }

    return XModelPrecache(name + SERVER_XMODEL_PREFIX_LENGTH + 1,
                          XMODEL_LOAD_PARTS_ONLY,
                          Hunk_AllocXModelPrecache, NULL);
}
