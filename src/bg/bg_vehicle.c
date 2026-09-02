#include "bg_vehicle.h"

#include "math/q_math.h"

/* Identical storage and values in the authoritative Windows client/game and
 * Linux game module.  The Windows cgame/game modules store the three vectors
 * at image-relative 0x8277c, 0x82788, and 0x82794 respectively:
 *   00000000 0000c0c1 00000000 -> {0, -24, 0}
 *   00002041 0000a040 00001cc2 -> {10, 5, -39}
 *   00000000 00000000 0000a0c1 -> {0, 0, -20}
 */
const vec3_t bgVehicleArtilleryPositionOffset = {0.0f, -24.0f, 0.0f};
const vec3_t bgVehicleTankPosition2Offset = {10.0f, 5.0f, -39.0f};
const vec3_t bgVehicleTankPosition1Offset = {0.0f, 0.0f, -20.0f};

/* The Windows cgame body at 0x300081e0 and Windows game body at 0x20007f90
 * have the same four-way pointer selection. Linux game RVA 0x230e5 preserves
 * the same comparisons and results. The default is the canonical zero vector.
 */
const float *BG_GetVehiclePosOffset(vehicle_type_t vehicleType, int32_t vehiclePosition)
{
    if (vehicleType == VEHICLE_TYPE_ARTILLERY) {
        return bgVehicleArtilleryPositionOffset;
    }
    if (vehicleType == VEHICLE_TYPE_TANK && vehiclePosition == 2) {
        return bgVehicleTankPosition2Offset;
    }
    if (vehicleType == VEHICLE_TYPE_TANK && vehiclePosition == 1) {
        return bgVehicleTankPosition1Offset;
    }
    return vec3_origin;
}

/* Windows cgame 0x30008210, Windows game 0x20007fc0, and Linux game RVA
 * 0x23148 all accept only the four-wheel gunner position. */
int32_t BG_AllowPlayerWeaponAtVehiclePos(vehicle_type_t vehicleType, int32_t vehiclePosition)
{
    return vehicleType == VEHICLE_TYPE_4_WHEEL && vehiclePosition == 3;
}
