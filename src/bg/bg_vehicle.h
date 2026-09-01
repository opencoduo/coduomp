#ifndef BG_VEHICLE_H
#define BG_VEHICLE_H

#include "qcommon/q_vector_types.h"
#include "qcommon/vehicle_types.h"

#include <stdint.h>

extern const vec3_t bgVehicleArtilleryPositionOffset;
extern const vec3_t bgVehicleTankPosition2Offset;
extern const vec3_t bgVehicleTankPosition1Offset;

const float *BG_GetVehiclePosOffset(vehicle_type_t vehicleType,
                                    int32_t vehiclePosition);
int32_t BG_AllowPlayerWeaponAtVehiclePos(vehicle_type_t vehicleType,
                                         int32_t vehiclePosition);

#endif
