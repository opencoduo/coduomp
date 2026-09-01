#include "filesystem.h"

#include "compat/coduo_ctype_compat.h"
#include "compat/coduo_int32_bits.h"

#include <ctype.h>

/* Source: CoDUOMP.exe 0x0042c990..0x0042c9e5.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0042c990_0042c9e6.mcode.
 * Name and signature: exact same-module Mac symbol FS_HashFileName. The
 * original passes sign-extended name bytes to the locale-aware CRT tolower,
 * normalizes backslashes, and ignores the extension beginning at the first
 * period. The signed shifts reproduce the x86 SAR hash mixing. */
uint32_t FS_HashFileName(const char *name, int32_t hashSize)
{
    uint32_t hash = 0;

    for (int32_t index = 0; name[index] != '\0'; ++index) {
        int32_t character = tolower(
            coduo_ctype_signed_byte_arg(name[index]));
        if (character == '.')
            break;
        if (character == '\\')
            character = '/';

        hash += (uint32_t)(index + 119) * (uint32_t)character;
    }

    hash = coduo_int32_sar_bits(hash, 20U) ^
           coduo_int32_sar_bits(hash, 10U) ^ hash;
    return hash & ((uint32_t)hashSize - 1u);
}
