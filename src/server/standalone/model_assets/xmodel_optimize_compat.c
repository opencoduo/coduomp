#include "animation/xmodel.h"

/* NOT_FROM_ORIGINAL_SOURCE: target-local edge used by the common XModel
 * loader.  The dedicated server has no renderer preprocessing subsystem. */
void xmodel_compat_optimize_loaded_surfs(XModelSurfsData *surfs, xmodel_asset_alloc_fn alloc)
{
    (void)surfs;
    (void)alloc;
}
