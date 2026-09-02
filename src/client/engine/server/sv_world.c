#include "server.h"

/* Source: CoDUOMP.exe 0x005ca4c8..0x005ca4df (.data). The world-sector
 * relinker uses these fixed bounds when SVF_DOBJ_USE_DEFAULT_BOUNDS is
 * set. The complete vectors are retained even though sector insertion only
 * consumes their X/Y components. */
vec3_t sv_defaultEntityClipMins = {-64.0f, -64.0f, -32.0f};
vec3_t sv_defaultEntityClipMaxs = {64.0f, 64.0f, 72.0f};

vec3_t cm_worldMins; /* original 0x0494de84 */
vec3_t cm_worldMaxs; /* original 0x0494de90 */
worldSector_t cm_worldSectorRoot; /* original 0x0494dec8 */
worldSector_t *cm_freeWorldSectors; /* original 0x0494def0 */
worldSector_t cm_nullWorldSector; /* original 0x0494def4 */
worldSector_t
    cm_worldSectorPool[SERVER_WORLD_SECTOR_POOL_COUNT]; /* 0x0494df1c */
