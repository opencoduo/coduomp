#include "compat/crt/random_compat.h"

#include <stdlib.h>

enum {
    CODUO_CRT_RAND_MASK = 32767,
    CODUO_CRT_RAND_CARDINALITY = 32768
};

#if RAND_MAX % 32768 != 32767
#error "Host rand domain must contain a whole number of 15-bit domains"
#endif

#define CODUO_GLIBC_RAND_MASK UINT32_C(2147483647)
#define CODUO_GLIBC_RAND_CARDINALITY 2147483648.0

/* NOT_FROM_ORIGINAL_SOURCE: map a non-negative compatibility-domain sample
 * without overflowing when the requested signed interval spans INT32_MIN to
 * INT32_MAX. The public range functions require maximum > minimum. */
static int32_t coduo_random_map_range(int32_t sample, int32_t minimum,
                                      int32_t maximum)
{
    const uint32_t span =
        (uint32_t)((int64_t)maximum - (int64_t)minimum);
    const int64_t result =
        (int64_t)minimum + (int64_t)((uint32_t)sample % span);

    return (int32_t)result;
}

/* NOT_FROM_ORIGINAL_SOURCE: the five Windows-CRT-domain provider functions
 * below preserve the original 15-bit result domain. */
int32_t coduo_crt_rand(void)
{
    return rand() % CODUO_CRT_RAND_CARDINALITY;
}

float coduo_crt_randf(void)
{
    return (float)coduo_crt_rand() / (float)CODUO_CRT_RAND_CARDINALITY;
}

double coduo_crt_rand_unit(void)
{
    return (double)coduo_crt_rand() / (double)CODUO_CRT_RAND_CARDINALITY;
}

double coduo_crt_rand_signed_unit(void)
{
    return coduo_crt_rand_unit() * 2.0 - 1.0;
}

int32_t coduo_crt_randrange(int32_t minimum, int32_t maximum)
{
    return coduo_random_map_range(coduo_crt_rand(), minimum, maximum);
}

/* NOT_FROM_ORIGINAL_SOURCE: the four glibc-domain provider functions below
 * supply 31 bits, combining native samples when the host rand is narrower. */
int32_t coduo_glibc_rand(void)
{
#if RAND_MAX >= 2147483647
    return (int32_t)((uint32_t)rand() & CODUO_GLIBC_RAND_MASK);
#else
    const uint32_t high = (uint32_t)rand() & CODUO_CRT_RAND_MASK;
    const uint32_t middle = (uint32_t)rand() & CODUO_CRT_RAND_MASK;
    const uint32_t low = (uint32_t)rand() & 1u;

    return (int32_t)((high << 16) | (middle << 1) | low);
#endif
}

double coduo_glibc_rand_unit(void)
{
    return (double)coduo_glibc_rand() / CODUO_GLIBC_RAND_CARDINALITY;
}

double coduo_glibc_rand_signed_unit(void)
{
    return coduo_glibc_rand_unit() * 2.0 - 1.0;
}

int32_t coduo_glibc_randrange(int32_t minimum, int32_t maximum)
{
    return coduo_random_map_range(coduo_glibc_rand(), minimum, maximum);
}
