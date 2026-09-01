/*
 * Source reconstruction for common math/random helpers.
 *
 * Binary SHA-256:
 * 4d2391c10e234559ace294fb69d96741cad522653c361f13f626f4dc1891acb7
 */

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "compat/libm/coduo_libm.h"
#include "compat/coduo_int32_bits.h"
#include "compat/coduo_native_x87.h"
#include "math/q_math.h"
#include "qcommon/q_shared_types.h"

#define DEGREES_PER_HALF_CIRCLE 180.0f
#define DOUBLE_PI 3.141592653589793 /* pi rounded to double64; original double64 0x400921fb54442d18 */
#define DEGREES_PER_RADIAN 180.0f
#define RADIANS_PER_DEGREE 0.017453292f /* pi/180 rounded to float32; original float32 0x3c8efa35 */
#define FLOAT_SIGN_BIT_MASK 0x80000000u
