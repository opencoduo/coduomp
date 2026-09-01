#include "dobj.h"

/* Source: CoDUOMP.exe 0x0049aca0..0x0049aca3, recovered from an exporter gap.
 * Exact original accessor name is not yet bound; the field role is proven by
 * DObj skeleton evaluation consumers. */
dobj_eval_storage_t *DObjGetEvaluationStorage(DObj *obj)
{
    return obj->evaluationStorage;
}
