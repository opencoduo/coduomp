#ifndef CLIENT_CRT_BOUNDARY_H
#define CLIENT_CRT_BOUNDARY_H

#include "client/common/client_legacy_crt.h"
#include "compat/coduo_fp_conversion.h"
#include "compat/crt/format_compat.h"
#include "compat/crt/random_compat.h"

/*
 * NOT_FROM_ORIGINAL_SOURCE
 *
 * The Windows uo_cgame_mp_x86.dll contains statically linked MSVC CRT/runtime
 * code. Those machine-code functions are part of the binary inventory, but
 * they are not CoD client game source and must not be reconstructed under
 * src/client/cgame/functions/.
 *
 * Recovered game code uses ordinary host facilities where the CRT contract is
 * platform-specific and the compatibility interfaces above only where the
 * original Windows behavior is part of the recovered client contract.
 */

#endif /* CLIENT_CRT_BOUNDARY_H */
