#include "collision_area.h"

#include "qcommon/collision_map_types.h"

#include <stdint.h>

extern collisionArea_t *cm_areas;
extern int32_t *cm_areaPortals;
extern int32_t cm_numAreas;
extern int32_t cm_floodValid;

void Com_Error(errorParm_t code, const char *format, ...);

/*
 * Complete area-portal flood subsystem.  The authoritative bodies agree on
 * all state changes, branches, error levels, and recursion:
 *
 *   CoDUOMP.exe   0x00425b90..0x00425d4c
 *   coduo_lnxded  0x08057890..0x08057b14
 *
 * The canonical CM_FloodArea_* spellings are exported by the Windows/Mac
 * engine family.  The former Linux recovered FloodArea_* names were local
 * reconstruction artifacts.
 */

void CM_FloodArea_r(int32_t areaNum, int32_t floodNum)
{
    collisionArea_t *const area = &cm_areas[areaNum];

    if (area->floodValid == cm_floodValid) {
        if (area->floodNum == floodNum) {
            return;
        }
        Com_Error(ERR_DROP, "\x15" "FloodArea_r: reflooded");
    }

    area->floodNum = floodNum;
    area->floodValid = cm_floodValid;

    int32_t *const portals = &cm_areaPortals[areaNum * cm_numAreas];
    for (int32_t otherArea = 0; otherArea < cm_numAreas; ++otherArea) {
        if (portals[otherArea] > 0) {
            CM_FloodArea_r(otherArea, floodNum);
        }
    }
}

void CM_FloodAreaConnections(void)
{
    int32_t floodNum = 0;

    cm_floodValid++;
    for (int32_t areaNum = 0; areaNum < cm_numAreas; ++areaNum) {
        if (cm_areas[areaNum].floodValid != cm_floodValid) {
            floodNum++;
            CM_FloodArea_r(areaNum, floodNum);
        }
    }
}

void CM_AdjustAreaPortalState(int32_t area1, int32_t area2, qboolean open)
{
    if (area1 < 0 || area2 < 0) {
        return;
    }

    if (area1 >= cm_numAreas || area2 >= cm_numAreas) {
        Com_Error(ERR_DROP,
                  "\x15" "CM_ChangeAreaPortalState: bad area number");
    }

    if (open != qfalse) {
        cm_areaPortals[area1 * cm_numAreas + area2]++;
        cm_areaPortals[area2 * cm_numAreas + area1]++;
    } else if (cm_areaPortals[area2 * cm_numAreas + area1] != 0) {
        cm_areaPortals[area1 * cm_numAreas + area2]--;
        cm_areaPortals[area2 * cm_numAreas + area1]--;

        if (cm_areaPortals[area2 * cm_numAreas + area1] < 0) {
            Com_Error(ERR_DROP,
                      "\x15"
                      "CM_AdjustAreaPortalState: negative reference count");
        }
    }

    CM_FloodAreaConnections();
}

qboolean CM_AreasConnected(int32_t area1, int32_t area2)
{
    if (area1 < 0 || area2 < 0) {
        return qfalse;
    }
    return cm_areas[area1].floodNum == cm_areas[area2].floodNum;
}
