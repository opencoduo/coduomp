#include "../renderer/backend.h"

enum {
    KSC5601_HANGUL_LEAD_FIRST = 0xb0,
    KSC5601_HANGUL_LEAD_LAST = 0xc8,
    KSC5601_HANGUL_TRAIL_FIRST = 0xa1,
    KSC5601_HANGUL_TRAIL_LAST = 0xfe,
    KSC5601_HANGUL_COLLAPSE_BASE = 0xb0a0,
    KSC5601_HANGUL_ROW_WIDTH = 96,

    BIG5_LEAD_FIRST_RANGE_FIRST = 0xa1,
    BIG5_LEAD_FIRST_RANGE_LAST = 0xc6,
    BIG5_LEAD_SECOND_RANGE_FIRST = 0xc9,
    BIG5_LEAD_SECOND_RANGE_LAST = 0xf9,
    BIG5_TRAIL_FIRST_RANGE_FIRST = 0x40,
    BIG5_TRAIL_FIRST_RANGE_LAST = 0x7e,
    BIG5_TRAIL_SECOND_RANGE_FIRST = 0xa1,
    BIG5_TRAIL_SECOND_RANGE_LAST = 0xfe,
    BIG5_COLLAPSE_BASE = 0xa140,
    BIG5_SECOND_RANGE_COLLAPSED_FIRST = 0x60,
    BIG5_SECOND_RANGE_GAP = 0x20,
    BIG5_COLLAPSED_ROW_WIDTH = 160,
    BIG5_TRAILING_PUNCTUATION_FIRST = 0xa140,
    BIG5_TRAILING_PUNCTUATION_END = 0xa154,

    SHIFT_JIS_LEAD_FIRST_RANGE_FIRST = 0x81,
    SHIFT_JIS_LEAD_FIRST_RANGE_LAST = 0x9f,
    SHIFT_JIS_LEAD_SECOND_RANGE_FIRST = 0xe0,
    SHIFT_JIS_LEAD_SECOND_RANGE_LAST = 0xef,
    SHIFT_JIS_TRAIL_FIRST_RANGE_FIRST = 0x40,
    SHIFT_JIS_TRAIL_FIRST_RANGE_LAST = 0x7e,
    SHIFT_JIS_TRAIL_SECOND_RANGE_FIRST = 0x80,
    SHIFT_JIS_TRAIL_SECOND_RANGE_LAST = 0xfc,
    SHIFT_JIS_COLLAPSE_BASE = 0x8140,
    SHIFT_JIS_LOW_GAP_FIRST = 0x40,
    SHIFT_JIS_HIGH_RANGE_FIRST = 0x5f00,
    SHIFT_JIS_HIGH_RANGE_GAP = 0x4000,
    SHIFT_JIS_COLLAPSED_ROW_WIDTH = 188,
    SHIFT_JIS_TRAILING_PUNCTUATION_FIRST = 0x8140,
    SHIFT_JIS_TRAILING_PUNCTUATION_END = 0x8152,

    GB2312_LEAD_FIRST = 0xa1,
    GB2312_LEAD_LAST = 0xf7,
    GB2312_TRAIL_FIRST = 0xa1,
    GB2312_TRAIL_LAST = 0xfe,
    GB2312_COLLAPSE_BASE = 0xa1a0,
    GB2312_COLLAPSED_ROW_WIDTH = 95,
    GB2312_TRAILING_PUNCTUATION_FIRST = 0xa1a1,
    GB2312_TRAILING_PUNCTUATION_END = 0xa1ae
};

/* Source: CoDUOMP.exe 0x00470aa0..0x00470aba.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00470aa0_00470abb.mcode.
 * The Mac build inlines this predicate into its named KSC5601 collapse and
 * character-reader routines; the Windows function boundary and byte ranges
 * prove its role. */
static qboolean Korean_ValidKSC5601HangulCode(
    uint8_t lead, uint8_t trail)
{
    return lead >= KSC5601_HANGUL_LEAD_FIRST &&
           lead <= KSC5601_HANGUL_LEAD_LAST &&
           trail >= KSC5601_HANGUL_TRAIL_FIRST &&
           trail <= KSC5601_HANGUL_TRAIL_LAST;
}

/* Source: CoDUOMP.exe 0x00470ac0..0x00470adf.
 * Evidence: repaired function record
 * coduomp/mcode/CoDUOMP/FUN_00470ac0_00470ae0.mcode.
 * The standalone packed-code overload was omitted by Ghidra; its KSC5601
 * ranges prove the role. */
qboolean Korean_ValidKSC5601HangulCodePacked(int32_t character)
{
    return Korean_ValidKSC5601HangulCode(
        (uint8_t)((uint32_t)character >> 8),
        (uint8_t)character);
}

/* Source: CoDUOMP.exe 0x00470ae0..0x00470b13.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00470ae0_00470b14.mcode.
 * Name and signature: exact same-module Mac symbol
 * Korean_CollapseKSC5601HangulCode. */
int32_t Korean_CollapseKSC5601HangulCode(int32_t character)
{
    if (Korean_ValidKSC5601HangulCodePacked(character) == qfalse)
        return 0;

    character -= KSC5601_HANGUL_COLLAPSE_BASE;
    return ((int32_t)((uint32_t)character >> 8) *
            KSC5601_HANGUL_ROW_WIDTH) +
           (character & 0xff);
}

/* Source: CoDUOMP.exe 0x00470b20..0x00470b4e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00470b20_00470b4f.mcode.
 * Name and signature: exact same-module Mac symbol Taiwanese_ValidBig5Code. */
qboolean Taiwanese_ValidBig5Code(int32_t character)
{
    const uint8_t lead = (uint8_t)((uint32_t)character >> 8);
    const uint8_t trail = (uint8_t)character;
    const qboolean validLead =
        (lead >= BIG5_LEAD_FIRST_RANGE_FIRST &&
         lead <= BIG5_LEAD_FIRST_RANGE_LAST) ||
        (lead >= BIG5_LEAD_SECOND_RANGE_FIRST &&
         lead <= BIG5_LEAD_SECOND_RANGE_LAST);
    const qboolean validTrail =
        (trail >= BIG5_TRAIL_FIRST_RANGE_FIRST &&
         trail <= BIG5_TRAIL_FIRST_RANGE_LAST) ||
        (trail >= BIG5_TRAIL_SECOND_RANGE_FIRST &&
         trail <= BIG5_TRAIL_SECOND_RANGE_LAST);
    return validLead != qfalse && validTrail != qfalse;
}

/* Source: CoDUOMP.exe 0x00470b50..0x00470b63.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00470b50_00470b64.mcode.
 * Name and signature: exact same-module Mac symbol
 * Taiwanese_IsTrailingPunctuation. */
qboolean Taiwanese_IsTrailingPunctuation(int32_t character)
{
    return character >= BIG5_TRAILING_PUNCTUATION_FIRST &&
           character < BIG5_TRAILING_PUNCTUATION_END;
}

/* Source: CoDUOMP.exe 0x00470b70..0x00470bc4.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00470b70_00470bc5.mcode.
 * Name and signature: exact same-module Mac symbol
 * Taiwanese_CollapseBig5Code. */
int32_t Taiwanese_CollapseBig5Code(int32_t character)
{
    if (Taiwanese_ValidBig5Code(character) == qfalse)
        return 0;

    character -= BIG5_COLLAPSE_BASE;
    if ((character & 0xff) >= BIG5_SECOND_RANGE_COLLAPSED_FIRST)
        character -= BIG5_SECOND_RANGE_GAP;
    return ((int32_t)((uint32_t)character >> 8) *
            BIG5_COLLAPSED_ROW_WIDTH) +
           (character & 0xff);
}

/* Source: CoDUOMP.exe 0x00470bd0..0x00470bf9.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00470bd0_00470bfa.mcode.
 * The Windows optimizer emits this byte-pair specialization for
 * SEH_ReadCharFromString. */
static qboolean Japanese_ValidShiftJISBytes(
    uint8_t lead, uint8_t trail)
{
    const qboolean validLead =
        (lead >= SHIFT_JIS_LEAD_FIRST_RANGE_FIRST &&
         lead <= SHIFT_JIS_LEAD_FIRST_RANGE_LAST) ||
        (lead >= SHIFT_JIS_LEAD_SECOND_RANGE_FIRST &&
         lead <= SHIFT_JIS_LEAD_SECOND_RANGE_LAST);
    const qboolean validTrail =
        (trail >= SHIFT_JIS_TRAIL_FIRST_RANGE_FIRST &&
         trail <= SHIFT_JIS_TRAIL_FIRST_RANGE_LAST) ||
        (trail >= SHIFT_JIS_TRAIL_SECOND_RANGE_FIRST &&
         trail <= SHIFT_JIS_TRAIL_SECOND_RANGE_LAST);
    return validLead != qfalse && validTrail != qfalse;
}

/* Source: CoDUOMP.exe 0x00470c00..0x00470c2e.
 * Evidence: repaired function record
 * coduomp/mcode/CoDUOMP/FUN_00470c00_00470c2f.mcode.
 * Name and signature: exact same-module Mac symbol
 * Japanese_ValidShiftJISCode. Ghidra omitted the packed-code entry and
 * retained only the byte-pair specialization above. */
qboolean Japanese_ValidShiftJISCode(int32_t character)
{
    return Japanese_ValidShiftJISBytes(
        (uint8_t)((uint32_t)character >> 8),
        (uint8_t)character);
}

/* Source: CoDUOMP.exe 0x00470c30..0x00470c43.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00470c30_00470c44.mcode.
 * Name and signature: exact same-module Mac symbol
 * Japanese_IsTrailingPunctuation. */
qboolean Japanese_IsTrailingPunctuation(int32_t character)
{
    return character >= SHIFT_JIS_TRAILING_PUNCTUATION_FIRST &&
           character < SHIFT_JIS_TRAILING_PUNCTUATION_END;
}

/* Source: CoDUOMP.exe 0x00470c50..0x00470cb8.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00470c50_00470cb9.mcode.
 * Name and signature: exact same-module Mac symbol
 * Japanese_CollapseShiftJISCode. */
int32_t Japanese_CollapseShiftJISCode(int32_t character)
{
    if (Japanese_ValidShiftJISCode(character) == qfalse)
        return 0;

    character -= SHIFT_JIS_COLLAPSE_BASE;
    if ((character & 0xff) >= SHIFT_JIS_LOW_GAP_FIRST)
        --character;
    if ((character & 0xff00) >= SHIFT_JIS_HIGH_RANGE_FIRST)
        character -= SHIFT_JIS_HIGH_RANGE_GAP;
    return ((int32_t)((uint32_t)character >> 8) *
            SHIFT_JIS_COLLAPSED_ROW_WIDTH) +
           (character & 0xff);
}

/* Source: CoDUOMP.exe 0x00470cc0..0x00470cda.
 * Evidence: repaired function record
 * coduomp/mcode/CoDUOMP/FUN_00470cc0_00470cdb.mcode.
 * The Windows optimizer emits this byte-pair specialization alongside the
 * packed-code function. */
static qboolean Chinese_ValidGBBytes(uint8_t lead, uint8_t trail)
{
    return lead >= GB2312_LEAD_FIRST &&
           lead <= GB2312_LEAD_LAST &&
           trail >= GB2312_TRAIL_FIRST &&
           trail <= GB2312_TRAIL_LAST;
}

/* Source: CoDUOMP.exe 0x00470ce0..0x00470cff.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00470ce0_00470d00.mcode.
 * Name and signature: exact same-module Mac symbol Chinese_ValidGBCode. */
qboolean Chinese_ValidGBCode(int32_t character)
{
    return Chinese_ValidGBBytes(
        (uint8_t)((uint32_t)character >> 8),
        (uint8_t)character);
}

/* Source: CoDUOMP.exe 0x00470d00..0x00470d13.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00470d00_00470d14.mcode.
 * Name and signature: exact same-module Mac symbol
 * Chinese_IsTrailingPunctuation. */
qboolean Chinese_IsTrailingPunctuation(int32_t character)
{
    return character >= GB2312_TRAILING_PUNCTUATION_FIRST &&
           character < GB2312_TRAILING_PUNCTUATION_END;
}

/* Source: CoDUOMP.exe 0x00470d20..0x00470d50.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00470d20_00470d51.mcode.
 * Name and signature: exact same-module Mac symbol Chinese_CollapseGBCode. */
int32_t Chinese_CollapseGBCode(int32_t character)
{
    if (Chinese_ValidGBCode(character) == qfalse)
        return 0;

    character -= GB2312_COLLAPSE_BASE;
    return ((int32_t)((uint32_t)character >> 8) *
            GB2312_COLLAPSED_ROW_WIDTH) +
           (character & 0xff);
}

/* Source: CoDUOMP.exe 0x00470d60..0x00470e9f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00470d60_00470ea0.mcode.
 * Name and signature: exact same-module Mac symbol SEH_ReadCharFromString.
 * Invalid multibyte pairs consume only their first byte. */
int32_t SEH_ReadCharFromString(
    const char **text, qboolean *isTrailingPunctuation)
{
    const uint8_t *cursor = (const uint8_t *)*text;
    int32_t character;

    if (rendererMultibyteTextEnabled != qfalse &&
        cl_language->integer >= LANGUAGE_KOREAN &&
        cl_language->integer <= LANGUAGE_CHINESE) {
        character = ((int32_t)cursor[0] << 8) | cursor[1];

        switch ((language_t)cl_language->integer) {
        case LANGUAGE_KOREAN:
            if (Korean_ValidKSC5601HangulCode(
                    cursor[0], cursor[1]) != qfalse) {
                cursor += 2;
                *text = (const char *)cursor;
                if (isTrailingPunctuation != NULL)
                    *isTrailingPunctuation = qfalse;
                return character;
            }
            break;
        case LANGUAGE_TAIWANESE:
            if (Taiwanese_ValidBig5Code(character) != qfalse) {
                cursor += 2;
                *text = (const char *)cursor;
                if (isTrailingPunctuation != NULL) {
                    *isTrailingPunctuation =
                        Taiwanese_IsTrailingPunctuation(character);
                }
                return character;
            }
            break;
        case LANGUAGE_JAPANESE:
            if (Japanese_ValidShiftJISBytes(
                    cursor[0], cursor[1]) != qfalse) {
                cursor += 2;
                *text = (const char *)cursor;
                if (isTrailingPunctuation != NULL) {
                    *isTrailingPunctuation =
                        Japanese_IsTrailingPunctuation(character);
                }
                return character;
            }
            break;
        case LANGUAGE_CHINESE:
            if (Chinese_ValidGBCode(character) != qfalse) {
                cursor += 2;
                *text = (const char *)cursor;
                if (isTrailingPunctuation != NULL) {
                    *isTrailingPunctuation =
                        Chinese_IsTrailingPunctuation(character);
                }
                return character;
            }
            break;
        default:
            break;
        }
    }

    character = *cursor++;
    *text = (const char *)cursor;
    if (isTrailingPunctuation != NULL) {
        switch (character) {
        case '!':
        case '?':
        case ',':
        case '.':
        case ';':
        case ':':
            *isTrailingPunctuation = qtrue;
            break;
        default:
            *isTrailingPunctuation = qfalse;
            break;
        }
    }
    return character;
}

/* Source: CoDUOMP.exe 0x00470eb0..0x00470eb5.
 * Evidence: repaired function record
 * coduomp/mcode/CoDUOMP/FUN_00470eb0_00470eb6.mcode.
 * Name and signature: exact same-module Mac symbol Language_IsAsian. */
qboolean Language_IsAsian(void)
{
    return rendererMultibyteTextEnabled;
}

/* Source: CoDUOMP.exe 0x00470ec0..0x00470eda.
 * Evidence: repaired function record
 * coduomp/mcode/CoDUOMP/FUN_00470ec0_00470edb.mcode.
 * Exact Mac name was not retained. The predicate is true for every language
 * except Taiwanese, Japanese, and Chinese, the three source encodings whose
 * text layout does not use ordinary inter-word spaces. */
qboolean Language_UsesSpaces(void)
{
    return cl_language->integer < LANGUAGE_TAIWANESE ||
           cl_language->integer > LANGUAGE_CHINESE;
}

/* Source: CoDUOMP.exe 0x00470ee0..0x00471041.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00470ee0_00471042.mcode.
 * Name and signature: exact same-module Mac symbol SEH_PrintStrlen. The
 * Windows optimizer inlines SEH_ReadCharFromString into this body. */
int32_t SEH_PrintStrlen(const char *text)
{
    int32_t length = 0;
    const char *cursor = text;

    if (cursor == NULL || *cursor == '\0')
        return 0;

    while (*cursor != '\0') {
        const int32_t character =
            SEH_ReadCharFromString(&cursor, NULL);

        if (character == '^' && cursor != NULL && *cursor != '^' &&
            *cursor >= '0' && *cursor <= '9') {
            ++cursor;
        } else if (character != '\n' && character != '\r') {
            ++length;
        }
    }

    return length;
}
