#if !defined(_WIN32) && !defined(_DEFAULT_SOURCE)
#define _DEFAULT_SOURCE
#endif

#include "server_namespace_provider.h"

#include "client/engine/platform/case_sensitive_fs.h"
#include "filesystem/filesystem.h"
#include "filesystem/filesystem_path_security.h"
#include "filesystem_services.h"
#include "qcommon/com_config.h"
#include "qcommon/q_endian.h"
#include "qcommon/q_memory.h"
#include "qcommon/q_string.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#if defined(_WIN32)
#include <windows.h>
#endif

enum {
    CODUOMP_NAMESPACE_SLUG_LENGTH = 40,
    CODUOMP_NAMESPACE_HASH_HEX_LENGTH = 16,
    CODUOMP_NAMESPACE_HASH_BYTE_LENGTH = 8,
    CODUOMP_NAMESPACE_ENDPOINT_VERSION = 1,
    CODUOMP_NAMESPACE_ENDPOINT_MAX_LENGTH = 14,
    CODUOMP_NAMESPACE_SHA256_DIGEST_SIZE = 32,
    CODUOMP_NAMESPACE_SHA256_BLOCK_SIZE = 64,
    CODUOMP_NAMESPACE_SHA256_WORD_COUNT = 64,
    CODUOMP_NAMESPACE_OFFICIAL_PAK_VARIANT_COUNT = 24
};

_Static_assert(CODUOMP_NAMESPACE_HASH_HEX_LENGTH ==
                   CODUOMP_NAMESPACE_HASH_BYTE_LENGTH * 2,
               "namespace hash text must encode every retained byte");

typedef struct coduomp_namespace_cvar_snapshot_s {
    char *name;
    char *value;
    char *latchedValue;
    uint32_t flags;
    qboolean modified;
    int32_t modificationCount;
} coduomp_namespace_cvar_snapshot_t;

typedef struct coduomp_namespace_state_s {
    qboolean active;
    char endpointHash[CODUOMP_NAMESPACE_HASH_HEX_LENGTH + 1];
    char stateRoot[MAX_OSPATH];
    char contentRoot[MAX_OSPATH];
    char frontendConfigGame[FS_PACK_NAME_SIZE];
    coduomp_namespace_cvar_snapshot_t *archivedCvars;
    size_t archivedCvarCount;
    char *frontendGame;
    char *frontendGameLatched;
    uint32_t frontendGameFlags;
    qboolean frontendGameModified;
    int32_t frontendGameModificationCount;
} coduomp_namespace_state_t;

static coduomp_namespace_state_t coduomp_namespace_state;

/* NOT_FROM_ORIGINAL_SOURCE: improved compatibility provider for crash-inert,
 * connection-owned server files. All functions in this file belong to the
 * coduomp compatibility namespace; no recovered original behavior is
 * represented here. */
static char *coduomp_namespace_duplicate(const char *text)
{
    const size_t length = strlen(text) + 1u;
    char *const copy = (char *)malloc(length);

    if (copy != NULL)
        memcpy(copy, text, length);
    return copy;
}

static void coduomp_namespace_free_archived_cvars(
    coduomp_namespace_cvar_snapshot_t *snapshot, size_t count)
{
    for (size_t index = 0; index < count; ++index) {
        free(snapshot[index].name);
        free(snapshot[index].value);
        free(snapshot[index].latchedValue);
    }
    free(snapshot);
}

static void coduomp_namespace_free_snapshot(void)
{
    coduomp_namespace_free_archived_cvars(
        coduomp_namespace_state.archivedCvars,
        coduomp_namespace_state.archivedCvarCount);
    free(coduomp_namespace_state.frontendGame);
    free(coduomp_namespace_state.frontendGameLatched);
    coduomp_namespace_state.archivedCvars = NULL;
    coduomp_namespace_state.archivedCvarCount = 0;
    coduomp_namespace_state.frontendGame = NULL;
    coduomp_namespace_state.frontendGameLatched = NULL;
}

static qboolean coduomp_namespace_collect_archived_cvars(
    coduomp_namespace_cvar_snapshot_t **snapshotOut,
    size_t *countOut)
{
    size_t count = 0;

    for (const cvar_t *cvar = cvar_vars;
         cvar != NULL; cvar = cvar->next) {
        if ((cvar->flags & CVAR_ARCHIVE) != 0)
            ++count;
    }

    coduomp_namespace_cvar_snapshot_t *snapshot = NULL;
    if (count != 0) {
        snapshot = (coduomp_namespace_cvar_snapshot_t *)calloc(
            count, sizeof(*snapshot));
        if (snapshot == NULL)
            return qfalse;
    }

    size_t index = 0;
    for (const cvar_t *cvar = cvar_vars;
         cvar != NULL; cvar = cvar->next) {
        if ((cvar->flags & CVAR_ARCHIVE) == 0)
            continue;
        snapshot[index].name = coduomp_namespace_duplicate(cvar->name);
        snapshot[index].value = coduomp_namespace_duplicate(cvar->string);
        if (cvar->latchedString != NULL) {
            snapshot[index].latchedValue =
                coduomp_namespace_duplicate(cvar->latchedString);
        }
        snapshot[index].flags = cvar->flags;
        snapshot[index].modified = cvar->modified;
        snapshot[index].modificationCount = cvar->modificationCount;
        if (snapshot[index].name == NULL ||
            snapshot[index].value == NULL ||
            (cvar->latchedString != NULL &&
             snapshot[index].latchedValue == NULL)) {
            coduomp_namespace_free_archived_cvars(snapshot, index + 1u);
            return qfalse;
        }
        ++index;
    }

    *snapshotOut = snapshot;
    *countOut = count;
    return qtrue;
}

static qboolean coduomp_namespace_capture_snapshot(void)
{
    if (coduomp_namespace_collect_archived_cvars(
            &coduomp_namespace_state.archivedCvars,
            &coduomp_namespace_state.archivedCvarCount) == qfalse) {
        return qfalse;
    }

    coduomp_namespace_state.frontendGame =
        coduomp_namespace_duplicate(fs_game->string);
    if (fs_game->latchedString != NULL) {
        coduomp_namespace_state.frontendGameLatched =
            coduomp_namespace_duplicate(fs_game->latchedString);
    }
    coduomp_namespace_state.frontendGameFlags = fs_game->flags;
    coduomp_namespace_state.frontendGameModified = fs_game->modified;
    coduomp_namespace_state.frontendGameModificationCount =
        fs_game->modificationCount;
    if (coduomp_namespace_state.frontendGame == NULL ||
        (fs_game->latchedString != NULL &&
         coduomp_namespace_state.frontendGameLatched == NULL)) {
        coduomp_namespace_free_snapshot();
        return qfalse;
    }

    const char *frontendConfigGame = fs_game->string;
    if (frontendConfigGame[0] == '\0')
        frontendConfigGame = fs_basegame->string;
    if (frontendConfigGame[0] == '\0')
        frontendConfigGame = fs_currentGameDir;
    if (coduo_compat_path_is_safe_relative(frontendConfigGame) == qfalse) {
        coduomp_namespace_free_snapshot();
        return qfalse;
    }
    Q_strncpyz(coduomp_namespace_state.frontendConfigGame,
               frontendConfigGame,
               sizeof(coduomp_namespace_state.frontendConfigGame));
    return qtrue;
}

static const coduomp_namespace_cvar_snapshot_t *
coduomp_namespace_find_snapshot(const char *name)
{
    for (size_t index = 0;
         index < coduomp_namespace_state.archivedCvarCount; ++index) {
        const coduomp_namespace_cvar_snapshot_t *const snapshot =
            &coduomp_namespace_state.archivedCvars[index];
        if (Q_stricmp(snapshot->name, name) == 0)
            return snapshot;
    }
    return NULL;
}

static void coduomp_namespace_restore_snapshot(void)
{
    for (cvar_t *cvar = cvar_vars; cvar != NULL; cvar = cvar->next) {
        if ((cvar->flags & CVAR_ARCHIVE) != 0 &&
            coduomp_namespace_find_snapshot(cvar->name) == NULL) {
            cvar->flags &= ~(uint32_t)CVAR_ARCHIVE;
        }
    }

    for (size_t index = 0;
         index < coduomp_namespace_state.archivedCvarCount; ++index) {
        const coduomp_namespace_cvar_snapshot_t *const snapshot =
            &coduomp_namespace_state.archivedCvars[index];
        cvar_t *cvar = Cvar_FindVar(snapshot->name);
        if (cvar != NULL) {
            cvar->flags = snapshot->flags;
            if (cvar->latchedString != NULL) {
                Z_FreeInternal(cvar->latchedString);
                cvar->latchedString = NULL;
            }
        }
        cvar = Cvar_Set2(snapshot->name, snapshot->value, qtrue);
        if (cvar != NULL) {
            cvar->flags = snapshot->flags;
            if (snapshot->latchedValue != NULL) {
                cvar->latchedString =
                    CopyStringInternal(snapshot->latchedValue);
            }
            cvar->modified = snapshot->modified;
            cvar->modificationCount = snapshot->modificationCount;
        }
    }

    if (coduomp_namespace_state.frontendGame != NULL) {
        cvar_t *game = Cvar_FindVar("fs_game");
        if (game != NULL && game->latchedString != NULL) {
            Z_FreeInternal(game->latchedString);
            game->latchedString = NULL;
        }
        game = Cvar_Set2(
            "fs_game", coduomp_namespace_state.frontendGame, qtrue);
        if (game != NULL) {
            game->flags = coduomp_namespace_state.frontendGameFlags;
            if (coduomp_namespace_state.frontendGameLatched != NULL) {
                game->latchedString = CopyStringInternal(
                    coduomp_namespace_state.frontendGameLatched);
            }
            game->modified = coduomp_namespace_state.frontendGameModified;
            game->modificationCount =
                coduomp_namespace_state.frontendGameModificationCount;
        }
    }
    cvar_modifiedFlags |= CVAR_ARCHIVE;
}

static uint32_t coduomp_namespace_rotate_right(uint32_t value,
                                               uint32_t count)
{
    return (value >> count) | (value << (32u - count));
}

static uint32_t coduomp_namespace_load_be32(const uint8_t *bytes)
{
    return ((uint32_t)bytes[0] << 24u) |
           ((uint32_t)bytes[1] << 16u) |
           ((uint32_t)bytes[2] << 8u) |
           (uint32_t)bytes[3];
}

static void coduomp_namespace_store_be32(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t)(value >> 24u);
    bytes[1] = (uint8_t)(value >> 16u);
    bytes[2] = (uint8_t)(value >> 8u);
    bytes[3] = (uint8_t)value;
}

static void coduomp_namespace_sha256(const uint8_t *message,
                                     size_t messageLength,
                                     uint8_t digest[
                                         CODUOMP_NAMESPACE_SHA256_DIGEST_SIZE])
{
    static const uint32_t constants[CODUOMP_NAMESPACE_SHA256_WORD_COUNT] = {
        0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
        0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
        0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
        0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
        0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
        0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
        0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
        0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
        0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
        0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
        0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
        0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
        0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
        0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
        0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
        0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
    };
    uint8_t block[CODUOMP_NAMESPACE_SHA256_BLOCK_SIZE] = {0};
    uint32_t words[CODUOMP_NAMESPACE_SHA256_WORD_COUNT];
    uint32_t state[8] = {
        0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
        0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u
    };

    /* Endpoint identities are at most fourteen bytes, so one SHA-256 block
     * always contains the complete canonical address. */
    memcpy(block, message, messageLength);
    block[messageLength] = 0x80u;
    const uint64_t bitLength = (uint64_t)messageLength * 8u;
    for (size_t byte = 0; byte < 8u; ++byte)
        block[63u - byte] = (uint8_t)(bitLength >> (byte * 8u));

    for (size_t index = 0; index < 16u; ++index)
        words[index] = coduomp_namespace_load_be32(&block[index * 4u]);
    for (size_t index = 16u;
         index < CODUOMP_NAMESPACE_SHA256_WORD_COUNT; ++index) {
        const uint32_t s0 =
            coduomp_namespace_rotate_right(words[index - 15u], 7u) ^
            coduomp_namespace_rotate_right(words[index - 15u], 18u) ^
            (words[index - 15u] >> 3u);
        const uint32_t s1 =
            coduomp_namespace_rotate_right(words[index - 2u], 17u) ^
            coduomp_namespace_rotate_right(words[index - 2u], 19u) ^
            (words[index - 2u] >> 10u);
        words[index] = words[index - 16u] + s0 +
                       words[index - 7u] + s1;
    }

    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];
    uint32_t e = state[4];
    uint32_t f = state[5];
    uint32_t g = state[6];
    uint32_t h = state[7];
    for (size_t index = 0;
         index < CODUOMP_NAMESPACE_SHA256_WORD_COUNT; ++index) {
        const uint32_t upper0 =
            coduomp_namespace_rotate_right(a, 2u) ^
            coduomp_namespace_rotate_right(a, 13u) ^
            coduomp_namespace_rotate_right(a, 22u);
        const uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        const uint32_t upper1 =
            coduomp_namespace_rotate_right(e, 6u) ^
            coduomp_namespace_rotate_right(e, 11u) ^
            coduomp_namespace_rotate_right(e, 25u);
        const uint32_t choose = (e & f) ^ ((~e) & g);
        const uint32_t first = h + upper1 + choose +
                               constants[index] + words[index];
        const uint32_t second = upper0 + majority;
        h = g;
        g = f;
        f = e;
        e = d + first;
        d = c;
        c = b;
        b = a;
        a = first + second;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
    for (size_t index = 0; index < 8u; ++index)
        coduomp_namespace_store_be32(&digest[index * 4u], state[index]);
}

static qboolean coduomp_namespace_endpoint_hash(
    const netadr_t *address,
    char hash[CODUOMP_NAMESPACE_HASH_HEX_LENGTH + 1])
{
    static const char hex[] = "0123456789abcdef";
    uint8_t canonical[CODUOMP_NAMESPACE_ENDPOINT_MAX_LENGTH];
    size_t length = 0;

    canonical[length++] = CODUOMP_NAMESPACE_ENDPOINT_VERSION;
    canonical[length++] = (uint8_t)address->type;
    if (address->type == NA_IP) {
        memcpy(&canonical[length], address->ip, sizeof(address->ip));
        length += sizeof(address->ip);
    } else if (address->type == NA_IPX) {
        memcpy(&canonical[length], address->ipx, sizeof(address->ipx));
        length += sizeof(address->ipx);
    } else {
        return qfalse;
    }

    const uint16_t hostPort =
        (uint16_t)BigShort((int16_t)address->port);
    canonical[length++] = (uint8_t)(hostPort >> 8u);
    canonical[length++] = (uint8_t)hostPort;

    uint8_t digest[CODUOMP_NAMESPACE_SHA256_DIGEST_SIZE];
    coduomp_namespace_sha256(canonical, length, digest);
    for (size_t index = 0;
         index < CODUOMP_NAMESPACE_HASH_BYTE_LENGTH; ++index) {
        hash[index * 2u] = hex[digest[index] >> 4u];
        hash[index * 2u + 1u] = hex[digest[index] & 15u];
    }
    hash[CODUOMP_NAMESPACE_HASH_HEX_LENGTH] = '\0';
    return qtrue;
}

static qboolean coduomp_namespace_is_hex(char character)
{
    return (character >= '0' && character <= '9') ||
           (character >= 'a' && character <= 'f') ||
           (character >= 'A' && character <= 'F')
               ? qtrue
               : qfalse;
}

static size_t coduomp_namespace_skip_color_code(const uint8_t *text,
                                                size_t remaining)
{
    if (remaining < 2u || text[0] != '^')
        return 0;
    if (text[1] >= '0' && text[1] <= '9')
        return 2u;
    if (remaining >= 8u && (text[1] == 'x' || text[1] == '#')) {
        for (size_t index = 2u; index < 8u; ++index) {
            if (coduomp_namespace_is_hex((char)text[index]) == qfalse)
                return 0;
        }
        return 8u;
    }
    return 0;
}

static size_t coduomp_namespace_decode_utf8(const uint8_t *text,
                                            size_t remaining,
                                            uint32_t *codepoint)
{
    const uint8_t first = text[0];
    if (first < 0x80u) {
        *codepoint = first;
        return 1u;
    }

    size_t count;
    uint32_t value;
    uint32_t minimum;
    if (first >= 0xc2u && first <= 0xdfu) {
        count = 2u;
        value = first & 0x1fu;
        minimum = 0x80u;
    } else if (first >= 0xe0u && first <= 0xefu) {
        count = 3u;
        value = first & 0x0fu;
        minimum = 0x800u;
    } else if (first >= 0xf0u && first <= 0xf4u) {
        count = 4u;
        value = first & 0x07u;
        minimum = 0x10000u;
    } else {
        return 0;
    }
    if (remaining < count)
        return 0;
    for (size_t index = 1u; index < count; ++index) {
        if ((text[index] & 0xc0u) != 0x80u)
            return 0;
        value = (value << 6u) | (text[index] & 0x3fu);
    }
    if (value < minimum || value > 0x10ffffu ||
        (value >= 0xd800u && value <= 0xdfffu)) {
        return 0;
    }
    *codepoint = value;
    return count;
}

static const char *coduomp_namespace_latin_transliteration(
    uint32_t codepoint)
{
    switch (codepoint) {
    case 0x00c0: case 0x00c1: case 0x00c2: case 0x00c3:
    case 0x00c4: case 0x00c5: case 0x00e0: case 0x00e1:
    case 0x00e2: case 0x00e3: case 0x00e4: case 0x00e5:
    case 0x0100: case 0x0101: case 0x0102: case 0x0103:
    case 0x0104: case 0x0105:
        return "a";
    case 0x00c6: case 0x00e6:
        return "ae";
    case 0x00c7: case 0x00e7: case 0x0106: case 0x0107:
    case 0x010c: case 0x010d:
        return "c";
    case 0x00d0: case 0x00f0: case 0x010e: case 0x010f:
        return "d";
    case 0x00c8: case 0x00c9: case 0x00ca: case 0x00cb:
    case 0x00e8: case 0x00e9: case 0x00ea: case 0x00eb:
    case 0x0112: case 0x0113: case 0x0118: case 0x0119:
    case 0x011a: case 0x011b:
        return "e";
    case 0x011e: case 0x011f:
        return "g";
    case 0x0128: case 0x0129: case 0x012a: case 0x012b:
    case 0x012e: case 0x012f: case 0x00cc: case 0x00cd:
    case 0x00ce: case 0x00cf: case 0x00ec: case 0x00ed:
    case 0x00ee: case 0x00ef:
        return "i";
    case 0x0141: case 0x0142:
        return "l";
    case 0x00d1: case 0x00f1: case 0x0143: case 0x0144:
    case 0x0147: case 0x0148:
        return "n";
    case 0x00d2: case 0x00d3: case 0x00d4: case 0x00d5:
    case 0x00d6: case 0x00d8: case 0x00f2: case 0x00f3:
    case 0x00f4: case 0x00f5: case 0x00f6: case 0x00f8:
    case 0x014c: case 0x014d: case 0x0150: case 0x0151:
        return "o";
    case 0x0152: case 0x0153:
        return "oe";
    case 0x0158: case 0x0159:
        return "r";
    case 0x015a: case 0x015b: case 0x0160: case 0x0161:
        return "s";
    case 0x00df:
        return "ss";
    case 0x0164: case 0x0165: case 0x00de: case 0x00fe:
        return "t";
    case 0x00d9: case 0x00da: case 0x00db: case 0x00dc:
    case 0x00f9: case 0x00fa: case 0x00fb: case 0x00fc:
    case 0x016a: case 0x016b: case 0x016e: case 0x016f:
    case 0x0170: case 0x0171: case 0x0172: case 0x0173:
        return "u";
    case 0x00dd: case 0x00fd: case 0x00ff:
        return "y";
    case 0x0179: case 0x017a: case 0x017b: case 0x017c:
    case 0x017d: case 0x017e:
        return "z";
    default:
        return NULL;
    }
}

static qboolean coduomp_namespace_append_slug_token(
    char slug[CODUOMP_NAMESPACE_SLUG_LENGTH + 1], size_t *length,
    const char *token)
{
    const size_t tokenLength = strlen(token);
    if (*length + tokenLength > CODUOMP_NAMESPACE_SLUG_LENGTH)
        return qfalse;
    memcpy(&slug[*length], token, tokenLength);
    *length += tokenLength;
    slug[*length] = '\0';
    return qtrue;
}

static void coduomp_namespace_append_slug_separator(
    char slug[CODUOMP_NAMESPACE_SLUG_LENGTH + 1], size_t *length)
{
    if (*length != 0 && slug[*length - 1u] != '-' &&
        *length < CODUOMP_NAMESPACE_SLUG_LENGTH) {
        slug[(*length)++] = '-';
        slug[*length] = '\0';
    }
}

static void coduomp_namespace_sanitize_name(
    const char *serverName,
    char slug[CODUOMP_NAMESPACE_SLUG_LENGTH + 1])
{
    static const char hex[] = "0123456789abcdef";
    const uint8_t *text = (const uint8_t *)(serverName != NULL
                                                ? serverName
                                                : "");
    size_t remaining = strlen((const char *)text);
    size_t length = 0;
    slug[0] = '\0';

    while (remaining != 0 && length < CODUOMP_NAMESPACE_SLUG_LENGTH) {
        const size_t colorLength =
            coduomp_namespace_skip_color_code(text, remaining);
        if (colorLength != 0) {
            text += colorLength;
            remaining -= colorLength;
            continue;
        }

        uint32_t codepoint;
        const size_t utf8Length =
            coduomp_namespace_decode_utf8(text, remaining, &codepoint);
        if (utf8Length == 0) {
            char token[4] = {'x', hex[text[0] >> 4u],
                             hex[text[0] & 15u], '\0'};
            coduomp_namespace_append_slug_separator(slug, &length);
            if (coduomp_namespace_append_slug_token(
                    slug, &length, token) == qfalse) {
                break;
            }
            coduomp_namespace_append_slug_separator(slug, &length);
            ++text;
            --remaining;
            continue;
        }

        text += utf8Length;
        remaining -= utf8Length;
        if ((codepoint >= '0' && codepoint <= '9') ||
            (codepoint >= 'A' && codepoint <= 'Z') ||
            (codepoint >= 'a' && codepoint <= 'z')) {
            char token[2] = {
                (char)tolower((unsigned char)codepoint), '\0'};
            if (coduomp_namespace_append_slug_token(
                    slug, &length, token) == qfalse) {
                break;
            }
            continue;
        }

        const char *const transliteration =
            coduomp_namespace_latin_transliteration(codepoint);
        if (transliteration != NULL) {
            if (coduomp_namespace_append_slug_token(
                    slug, &length, transliteration) == qfalse) {
                break;
            }
            continue;
        }

        if (codepoint < 0x80u) {
            coduomp_namespace_append_slug_separator(slug, &length);
            continue;
        }

        char token[9];
        (void)snprintf(token, sizeof(token), "u%x", codepoint);
        coduomp_namespace_append_slug_separator(slug, &length);
        if (coduomp_namespace_append_slug_token(
                slug, &length, token) == qfalse) {
            break;
        }
        coduomp_namespace_append_slug_separator(slug, &length);
    }

    while (length != 0 && slug[length - 1u] == '-')
        slug[--length] = '\0';
    if (length == 0)
        Q_strncpyz(slug, "server", CODUOMP_NAMESPACE_SLUG_LENGTH + 1);
}

static qboolean coduomp_namespace_has_hash_suffix(
    const char *directoryName, const char *hash)
{
    const size_t nameLength = strlen(directoryName);
    const size_t hashLength = strlen(hash);

    if (nameLength <= hashLength ||
        directoryName[nameLength - hashLength - 1u] != '-') {
        return qfalse;
    }
    return Q_stricmp(&directoryName[nameLength - hashLength], hash) == 0
               ? qtrue
               : qfalse;
}

static qboolean coduomp_namespace_directory_name_is_safe(
    const char *directoryName)
{
    const size_t length = strlen(directoryName);

    if (length == 0 || length >= MAX_QPATH ||
        directoryName[0] == '-' || directoryName[length - 1u] == '-') {
        return qfalse;
    }
    for (size_t index = 0; index < length; ++index) {
        const char character = directoryName[index];
        if ((character < 'a' || character > 'z') &&
            (character < '0' || character > '9') &&
            character != '-') {
            return qfalse;
        }
    }
    return qtrue;
}

static qboolean coduomp_namespace_select_directory(
    const char *homeRoot, const char *slug, const char *hash,
    char directoryName[MAX_QPATH])
{
    char cacheRoot[MAX_OSPATH];
    if (strlen(homeRoot) + strlen("server-cache/v1") + 3u > MAX_OSPATH)
        return qfalse;
    FS_BuildOSPath(homeRoot, "server-cache/v1", "", cacheRoot);
    cacheRoot[strlen(cacheRoot) - 1u] = '\0';

    int32_t directoryCount = 0;
    char **const directories = Sys_ListFiles(
        cacheRoot, NULL, NULL, &directoryCount, qtrue);
    for (int32_t index = 0; index < directoryCount; ++index) {
        if (coduomp_namespace_directory_name_is_safe(
                directories[index]) != qfalse &&
            coduomp_namespace_has_hash_suffix(
                directories[index], hash) != qfalse) {
            Q_strncpyz(directoryName, directories[index], MAX_QPATH);
            Sys_FreeFileList(directories);
            return qtrue;
        }
    }
    Sys_FreeFileList(directories);

    const int written = snprintf(
        directoryName, MAX_QPATH, "%s-%s", slug, hash);
    return written > 0 && written < MAX_QPATH ? qtrue : qfalse;
}

static qboolean coduomp_namespace_build_roots(
    const char *homeRoot, const char *directoryName)
{
    char relative[MAX_OSPATH];
    const int written = snprintf(relative, sizeof(relative),
                                 "server-cache/v1/%s/content",
                                 directoryName);
    if (written <= 0 || written >= (int)sizeof(relative))
        return qfalse;
    if (strlen(homeRoot) + strlen(relative) + 3u > MAX_OSPATH)
        return qfalse;
    FS_BuildOSPath(homeRoot, relative, "",
                   coduomp_namespace_state.contentRoot);
    coduomp_namespace_state.contentRoot[
        strlen(coduomp_namespace_state.contentRoot) - 1u] = '\0';
    if (strlen(coduomp_namespace_state.contentRoot) >=
        sizeof(((directory_t *)0)->path)) {
        return qfalse;
    }

    const int stateWritten = snprintf(
        relative, sizeof(relative), "server-cache/v1/%s/state",
        directoryName);
    if (stateWritten <= 0 || stateWritten >= (int)sizeof(relative) ||
        strlen(homeRoot) + strlen(relative) + 3u > MAX_OSPATH) {
        return qfalse;
    }
    FS_BuildOSPath(homeRoot, relative, "",
                   coduomp_namespace_state.stateRoot);
    coduomp_namespace_state.stateRoot[
        strlen(coduomp_namespace_state.stateRoot) - 1u] = '\0';
    if (strlen(coduomp_namespace_state.stateRoot) >=
        sizeof(((directory_t *)0)->path)) {
        return qfalse;
    }
    return qtrue;
}

static qboolean coduomp_namespace_path_equal(const char *left,
                                             const char *right)
{
    if (left == NULL || right == NULL)
        return qfalse;
    while (*left != '\0' && *right != '\0') {
        char leftCharacter = *left++;
        char rightCharacter = *right++;
        if (leftCharacter == '\\')
            leftCharacter = '/';
        if (rightCharacter == '\\')
            rightCharacter = '/';
        if (tolower((unsigned char)leftCharacter) !=
            tolower((unsigned char)rightCharacter)) {
            return qfalse;
        }
    }
    return *left == '\0' && *right == '\0' ? qtrue : qfalse;
}

static qboolean coduomp_namespace_path_within(const char *path,
                                              const char *root)
{
    if (path == NULL || root == NULL)
        return qfalse;
    while (*root != '\0') {
        if (*path == '\0')
            return qfalse;
        char pathCharacter = *path++;
        char rootCharacter = *root++;
        if (pathCharacter == '\\')
            pathCharacter = '/';
        if (rootCharacter == '\\')
            rootCharacter = '/';
        if (tolower((unsigned char)pathCharacter) !=
            tolower((unsigned char)rootCharacter)) {
            return qfalse;
        }
    }
    return *path == '\0' || *path == '/' || *path == '\\'
               ? qtrue
               : qfalse;
}

static qboolean coduomp_namespace_game_is_official(const char *gameName)
{
    return gameName != NULL &&
                   (Q_stricmp(gameName, "main") == 0 ||
                    Q_stricmp(gameName, fs_basegame->string) == 0)
               ? qtrue
               : qfalse;
}

static qboolean coduomp_namespace_pack_is_official(const pack_t *pack)
{
    if (pack == NULL)
        return qfalse;

    const qboolean mainGame =
        Q_stricmp(pack->pakGamename, "main") == 0 ? qtrue : qfalse;
    const qboolean baseGame =
        Q_stricmp(pack->pakGamename, fs_basegame->string) == 0
            ? qtrue
            : qfalse;
    if (mainGame == qfalse && baseGame == qfalse)
        return qfalse;

    if (mainGame != qfalse &&
        Q_stricmp(pack->pakBasename, "mp_bin") == 0) {
        return qtrue;
    }

    char expectedName[32];
    for (int32_t pakIndex = 0;
         pakIndex < CODUOMP_NAMESPACE_OFFICIAL_PAK_VARIANT_COUNT;
         ++pakIndex) {
        if (mainGame != qfalse) {
            (void)snprintf(expectedName, sizeof(expectedName),
                           "pak%x", pakIndex);
            if (Q_stricmp(pack->pakBasename, expectedName) == 0)
                return qtrue;
            (void)snprintf(expectedName, sizeof(expectedName),
                           "mp_pak%x", pakIndex);
            if (Q_stricmp(pack->pakBasename, expectedName) == 0)
                return qtrue;
            (void)snprintf(expectedName, sizeof(expectedName),
                           "sp_pak%x", pakIndex);
            if (Q_stricmp(pack->pakBasename, expectedName) == 0)
                return qtrue;
        }
        if (baseGame != qfalse) {
            (void)snprintf(expectedName, sizeof(expectedName),
                           "pakuo%x", pakIndex);
            if (Q_stricmp(pack->pakBasename, expectedName) == 0)
                return qtrue;
            (void)snprintf(expectedName, sizeof(expectedName),
                           "pakuo0%x", pakIndex);
            if (Q_stricmp(pack->pakBasename, expectedName) == 0)
                return qtrue;
        }
    }

    static const char localizedPrefix[] = "localized_";
    if (strncmp(pack->pakBasename, localizedPrefix,
                sizeof(localizedPrefix) - 1u) != 0) {
        return qfalse;
    }

    char localizedName[FS_PACK_NAME_SIZE];
    Q_strncpyz(localizedName,
               pack->pakBasename + sizeof(localizedPrefix) - 1u,
               sizeof(localizedName));
    Q_strlwr(localizedName);
    for (int32_t pakIndex = 0;
         pakIndex < CODUOMP_NAMESPACE_OFFICIAL_PAK_VARIANT_COUNT;
         ++pakIndex) {
        (void)snprintf(expectedName, sizeof(expectedName),
                       "_pak%x", pakIndex);
        if (strstr(localizedName, expectedName) != NULL)
            return qtrue;
        (void)snprintf(expectedName, sizeof(expectedName),
                       "_pakuo%x", pakIndex);
        if (strstr(localizedName, expectedName) != NULL)
            return qtrue;
        (void)snprintf(expectedName, sizeof(expectedName),
                       "_pakuo0%x", pakIndex);
        if (strstr(localizedName, expectedName) != NULL)
            return qtrue;
    }
    return qfalse;
}

static void coduomp_compat_server_namespace_promote_config(void)
{
    static const char generatedConfigHeader[] =
        "// generated by Call of Duty, do not modify\n";
    static const char automaticConfigName[] = "uoconfig_mp.cfg";
    coduomp_namespace_cvar_snapshot_t *promotedCvars = NULL;
    size_t promotedCvarCount = 0;
    char configQPath[MAX_OSPATH];

    if (coduomp_namespace_state.active == qfalse) {
        Com_Printf("No isolated server configuration is active.\n");
        return;
    }
    if (coduomp_namespace_collect_archived_cvars(
            &promotedCvars, &promotedCvarCount) == qfalse) {
        Com_Printf("Could not capture the current server configuration.\n");
        return;
    }

    const int written = snprintf(
        configQPath, sizeof(configQPath), "%s/%s",
        coduomp_namespace_state.frontendConfigGame,
        automaticConfigName);
    if (written <= 0 || written >= (int)sizeof(configQPath)) {
        coduomp_namespace_free_archived_cvars(
            promotedCvars, promotedCvarCount);
        Com_Printf("Global configuration path is too long.\n");
        return;
    }

    const int32_t fileHandle = coduomp_fs_root_fopen_file_write(
        fs_homepath->string, configQPath);
    if (fileHandle == 0) {
        coduomp_namespace_free_archived_cvars(
            promotedCvars, promotedCvarCount);
        Com_Printf("Could not write global configuration %s.\n",
                   configQPath);
        return;
    }

    FS_Printf(fileHandle, generatedConfigHeader);
    Key_WriteBindings(fileHandle);
    Cvar_WriteVariables(fileHandle);
    FS_FCloseFile(fileHandle);
    coduomp_case_path_cache_clear();

    coduomp_namespace_free_archived_cvars(
        coduomp_namespace_state.archivedCvars,
        coduomp_namespace_state.archivedCvarCount);
    coduomp_namespace_state.archivedCvars = promotedCvars;
    coduomp_namespace_state.archivedCvarCount = promotedCvarCount;
    Com_Printf("Promoted current server configuration to %s.\n",
               configQPath);
}

static qboolean coduomp_namespace_build_safe_child_path(
    const char *parentPath, const char *childName,
    qboolean wantDirectory, char childPath[MAX_OSPATH])
{
    if (childName == NULL || childName[0] == '\0' ||
        strpbrk(childName, "/\\") != NULL ||
        coduo_compat_path_is_safe_relative(childName) == qfalse) {
        return qfalse;
    }

    const int written = snprintf(
        childPath, MAX_OSPATH, "%s%c%s", parentPath,
        FS_HOST_PATH_SEPARATOR, childName);
    if (written <= 0 || written >= MAX_OSPATH)
        return qfalse;

#if defined(_WIN32)
    const DWORD attributes = GetFileAttributesA(childPath);
    if (attributes == INVALID_FILE_ATTRIBUTES ||
        (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 ||
        (wantDirectory != qfalse
             ? (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0
             : (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)) {
        return qfalse;
    }
#else
    struct stat status;
    if (lstat(childPath, &status) != 0 || S_ISLNK(status.st_mode) ||
        (wantDirectory != qfalse
             ? S_ISDIR(status.st_mode) == 0
             : S_ISREG(status.st_mode) == 0)) {
        return qfalse;
    }
#endif
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: recover the stable display-name slug from the
 * cache directory's <server-name>-<endpoint-id> identity. */
static qboolean coduomp_namespace_split_server_label(
    const char *directoryName, char serverLabel[MAX_QPATH])
{
    const size_t nameLength = strlen(directoryName);
    if (nameLength <= CODUOMP_NAMESPACE_HASH_HEX_LENGTH + 1u)
        return qfalse;

    const size_t delimiterIndex =
        nameLength - CODUOMP_NAMESPACE_HASH_HEX_LENGTH - 1u;
    if (directoryName[delimiterIndex] != '-')
        return qfalse;
    for (size_t index = delimiterIndex + 1u;
         index < nameLength; ++index) {
        if (coduomp_namespace_is_hex(directoryName[index]) == qfalse)
            return qfalse;
    }

    memcpy(serverLabel, directoryName, delimiterIndex);
    serverLabel[delimiterIndex] = '\0';
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: publish cached mod folders through the retail
 * paired-string mod-list ABI without exposing the endpoint hash as UI text. */
static int32_t coduomp_compat_server_namespace_append_cached_mods(
    char *listBuffer, int32_t bufferSize)
{
    if (listBuffer == NULL || bufferSize <= 0 || fs_homepath == NULL ||
        fs_homepath->string[0] == '\0' ||
        strlen(fs_homepath->string) + strlen("server-cache/v1") + 3u >
            MAX_OSPATH) {
        return 0;
    }
    listBuffer[0] = '\0';

    char cacheRoot[MAX_OSPATH];
    FS_BuildOSPath(fs_homepath->string, "server-cache/v1", "",
                   cacheRoot);
    cacheRoot[strlen(cacheRoot) - 1u] = '\0';

    int32_t namespaceCount = 0;
    char **const namespaces = Sys_ListFiles(
        cacheRoot, NULL, NULL, &namespaceCount, qtrue);
    int32_t modCount = 0;
    int32_t listBytes = 0;
    for (int32_t namespaceIndex = 0;
         namespaceIndex < namespaceCount; ++namespaceIndex) {
        const char *const namespaceName = namespaces[namespaceIndex];
        char serverLabel[MAX_QPATH];
        char namespacePath[MAX_OSPATH];
        char contentPath[MAX_OSPATH];
        if (coduomp_namespace_directory_name_is_safe(namespaceName) == qfalse ||
            coduomp_namespace_split_server_label(
                namespaceName, serverLabel) == qfalse ||
            coduomp_namespace_build_safe_child_path(
                cacheRoot, namespaceName, qtrue, namespacePath) == qfalse ||
            coduomp_namespace_build_safe_child_path(
                namespacePath, "content", qtrue, contentPath) == qfalse) {
            continue;
        }

        int32_t gameCount = 0;
        char **const gameDirectories = Sys_ListFiles(
            contentPath, NULL, NULL, &gameCount, qtrue);
        for (int32_t gameIndex = 0;
             gameIndex < gameCount; ++gameIndex) {
            const char *const gameName = gameDirectories[gameIndex];
            char gamePath[MAX_OSPATH];
            if (coduomp_namespace_game_is_official(gameName) != qfalse ||
                coduomp_namespace_build_safe_child_path(
                    contentPath, gameName, qtrue, gamePath) == qfalse) {
                continue;
            }

            int32_t pakCount = 0;
            char **const pakFiles = Sys_ListFiles(
                gamePath, ".pk3", NULL, &pakCount, qfalse);
            Sys_FreeFileList(pakFiles);
            if (pakCount < 1)
                continue;

            char launchDirectory[FS_PACK_NAME_SIZE];
            char description[FS_PACK_NAME_SIZE];
            const int launchWritten = snprintf(
                launchDirectory, sizeof(launchDirectory),
                "server-cache/v1/%s/content/%s",
                namespaceName, gameName);
            const int descriptionWritten = snprintf(
                description, sizeof(description), "%s/%s",
                serverLabel, gameName);
            if (launchWritten <= 0 ||
                launchWritten >= (int)sizeof(launchDirectory) ||
                descriptionWritten <= 0 ||
                descriptionWritten >= (int)sizeof(description) ||
                coduo_compat_path_is_safe_relative(
                    launchDirectory) == qfalse ||
                strlen(fs_homepath->string) +
                        (size_t)launchWritten + 3u >
                    MAX_OSPATH) {
                continue;
            }

            const int32_t launchBytes = launchWritten + 1;
            const int32_t descriptionBytes = descriptionWritten + 1;
            if (listBytes + launchBytes + descriptionBytes + 2 >=
                bufferSize) {
                Sys_FreeFileList(gameDirectories);
                Sys_FreeFileList(namespaces);
                return modCount;
            }

            memcpy(listBuffer, launchDirectory,
                   (size_t)launchBytes);
            listBuffer += launchBytes;
            memcpy(listBuffer, description,
                   (size_t)descriptionBytes);
            listBuffer += descriptionBytes;
            listBytes += launchBytes + descriptionBytes;
            ++modCount;
        }
        Sys_FreeFileList(gameDirectories);
    }
    Sys_FreeFileList(namespaces);
    return modCount;
}

/* NOT_FROM_ORIGINAL_SOURCE: distinguish user-installed root paks from every
 * connection-owned server-cache namespace. */
static qboolean coduomp_namespace_pack_is_from_ordinary_root(
    const pack_t *pack, const char *cacheRoot)
{
    if (pack == NULL ||
        coduomp_namespace_path_within(
            pack->pakFilename, cacheRoot) != qfalse) {
        return qfalse;
    }

    return coduomp_namespace_path_within(
               pack->pakFilename, fs_homepath->string) != qfalse ||
                   coduomp_namespace_path_within(
                       pack->pakFilename, fs_basepath->string) != qfalse ||
                   coduomp_namespace_path_within(
                       pack->pakFilename, fs_cdpath->string) != qfalse
               ? qtrue
               : qfalse;
}

/* NOT_FROM_ORIGINAL_SOURCE: release a temporary FS_LoadZipFile catalog
 * without publishing it as a search path or retaining its file count. */
static void coduomp_namespace_free_probed_pak(pack_t *pack)
{
    fs_packFiles -= pack->numFiles;
    filesystem_compat_pack_close(pack);
    Z_FreeInternal(pack->fileList);
    Z_FreeInternal(pack);
}

/* NOT_FROM_ORIGINAL_SOURCE: verify an unmounted cache candidate by the same
 * pak checksum used for the server's referenced-pak comparison. */
static qboolean coduomp_namespace_cached_pak_matches(
    const char *candidateQPath, int32_t checksum)
{
    if (strlen(coduomp_namespace_state.contentRoot) +
            strlen(candidateQPath) + 3u >
        MAX_OSPATH) {
        return qfalse;
    }

    char candidatePath[MAX_OSPATH];
    FS_BuildOSPath(coduomp_namespace_state.contentRoot,
                   candidateQPath, "", candidatePath);
    candidatePath[strlen(candidatePath) - 1u] = '\0';

    char resolvedPath[MAX_OSPATH];
    if (coduomp_resolve_case_path(
            coduomp_namespace_state.contentRoot,
            candidatePath, resolvedPath,
            sizeof(resolvedPath)) != qfalse) {
        Q_strncpyz(candidatePath, resolvedPath,
                   sizeof(candidatePath));
    }

    pack_t *const pack = FS_LoadZipFile(
        candidatePath, candidateQPath);
    if (pack == NULL)
        return qfalse;

    const qboolean matches = pack->checksum == checksum
                                 ? qtrue : qfalse;
    coduomp_namespace_free_probed_pak(pack);
    return matches;
}

/* NOT_FROM_ORIGINAL_SOURCE: reuse an unmounted checksum-matching cache file;
 * otherwise retain wrong or older files while choosing a nonconflicting name
 * for a known-good root pak. */
static qboolean coduomp_namespace_choose_cache_pak_path(
    const char *pakName, int32_t checksum,
    char destinationQPath[MAX_OSPATH], qboolean *needsCopy)
{
    *needsCopy = qfalse;

    const int baseWritten = snprintf(
        destinationQPath, MAX_OSPATH, "%s.pk3", pakName);
    if (baseWritten <= 0 || baseWritten >= MAX_OSPATH)
        return qfalse;
    if (coduomp_fs_root_file_exists(
            coduomp_namespace_state.contentRoot,
            destinationQPath) == qfalse) {
        *needsCopy = qtrue;
        return qtrue;
    }
    if (coduomp_namespace_cached_pak_matches(
            destinationQPath, checksum) != qfalse) {
        return qtrue;
    }

    const int alternateWritten = snprintf(
        destinationQPath, MAX_OSPATH, "%s.%08x.pk3", pakName,
        (uint32_t)checksum);
    if (alternateWritten <= 0 || alternateWritten >= MAX_OSPATH)
        return qfalse;
    if (coduomp_fs_root_file_exists(
            coduomp_namespace_state.contentRoot,
            destinationQPath) == qfalse) {
        *needsCopy = qtrue;
        return qtrue;
    }
    if (coduomp_namespace_cached_pak_matches(
            destinationQPath, checksum) != qfalse) {
        return qtrue;
    }

    const int reuseWritten = snprintf(
        destinationQPath, MAX_OSPATH, "%s.root-%08x.pk3", pakName,
        (uint32_t)checksum);
    if (reuseWritten <= 0 || reuseWritten >= MAX_OSPATH)
        return qfalse;
    if (coduomp_fs_root_file_exists(
            coduomp_namespace_state.contentRoot,
            destinationQPath) == qfalse) {
        *needsCopy = qtrue;
        return qtrue;
    }
    return coduomp_namespace_cached_pak_matches(
        destinationQPath, checksum);
}

/* NOT_FROM_ORIGINAL_SOURCE: stage and atomically install a checksum-matched
 * root pak; any failed attempt removes only its temporary copy. */
static qboolean coduomp_namespace_copy_root_pak(
    const pack_t *pack, const char *pakName, int32_t checksum)
{
    char destinationQPath[MAX_OSPATH];
    qboolean needsCopy;
    if (coduomp_namespace_choose_cache_pak_path(
            pakName, checksum, destinationQPath,
            &needsCopy) == qfalse) {
        return qfalse;
    }
    if (needsCopy == qfalse)
        return qtrue;

    char temporaryQPath[MAX_OSPATH];
    const int temporaryWritten = snprintf(
        temporaryQPath, sizeof(temporaryQPath), "%s.root-reuse.tmp",
        destinationQPath);
    if (temporaryWritten <= 0 ||
        temporaryWritten >= (int)sizeof(temporaryQPath) ||
        strlen(coduomp_namespace_state.contentRoot) +
                (size_t)temporaryWritten + 3u >
            MAX_OSPATH ||
        strlen(coduomp_namespace_state.contentRoot) +
                strlen(destinationQPath) + 3u >
            MAX_OSPATH) {
        return qfalse;
    }

    char temporaryPath[MAX_OSPATH];
    char destinationPath[MAX_OSPATH];
    FS_BuildOSPath(coduomp_namespace_state.contentRoot,
                   temporaryQPath, "", temporaryPath);
    FS_BuildOSPath(coduomp_namespace_state.contentRoot,
                   destinationQPath, "", destinationPath);
    temporaryPath[strlen(temporaryPath) - 1u] = '\0';
    destinationPath[strlen(destinationPath) - 1u] = '\0';

    FS_Copyfiles(pack->pakFilename, temporaryPath);

    struct stat sourceStatus;
    struct stat temporaryStatus;
    if (stat(pack->pakFilename, &sourceStatus) != 0 ||
        stat(temporaryPath, &temporaryStatus) != 0 ||
        sourceStatus.st_size < 0 ||
        sourceStatus.st_size != temporaryStatus.st_size ||
        rename(temporaryPath, destinationPath) != 0) {
        FS_Remove(temporaryPath);
        Com_Printf("Could not reuse root pak %s; falling back to download.\n",
                   pakName);
        return qfalse;
    }

    coduomp_case_path_cache_clear();
    Com_Printf("Reused root pak %s in the active server cache.\n",
               pakName);
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: validate the exact server-named PK3 below one
 * ordinary root, copy it only on checksum equality, and release the probe. */
static qboolean coduomp_namespace_probe_and_copy_root_pak(
    const char *root, const char *cacheRoot, const char *pakName,
    int32_t checksum)
{
    if (root == NULL || root[0] == '\0')
        return qfalse;

    char sourceQPath[MAX_OSPATH];
    const int qpathWritten = snprintf(
        sourceQPath, sizeof(sourceQPath), "%s.pk3", pakName);
    if (qpathWritten <= 0 ||
        qpathWritten >= (int)sizeof(sourceQPath) ||
        strlen(root) + (size_t)qpathWritten + 3u > MAX_OSPATH) {
        return qfalse;
    }

    char sourcePath[MAX_OSPATH];
    FS_BuildOSPath(root, sourceQPath, "", sourcePath);
    sourcePath[strlen(sourcePath) - 1u] = '\0';
    char resolvedPath[MAX_OSPATH];
    if (coduomp_resolve_case_path(
            root, sourcePath, resolvedPath,
            sizeof(resolvedPath)) != qfalse) {
        Q_strncpyz(sourcePath, resolvedPath, sizeof(sourcePath));
    }
    if (coduomp_namespace_path_within(
            sourcePath, cacheRoot) != qfalse) {
        return qfalse;
    }
    struct stat sourceStatus;
    if (stat(sourcePath, &sourceStatus) != 0)
        return qfalse;

    const char *basename = strrchr(sourceQPath, '/');
    const char *const backslash = strrchr(sourceQPath, '\\');
    if (backslash != NULL &&
        (basename == NULL || backslash > basename)) {
        basename = backslash;
    }
    if (basename != NULL)
        ++basename;
    else
        basename = sourceQPath;

    pack_t *const pack = FS_LoadZipFile(sourcePath, basename);
    if (pack == NULL)
        return qfalse;

    qboolean copied = qfalse;
    if (pack->checksum == checksum) {
        copied = coduomp_namespace_copy_root_pak(
            pack, pakName, checksum);
    }
    coduomp_namespace_free_probed_pak(pack);
    return copied;
}

/* NOT_FROM_ORIGINAL_SOURCE: match server-published checksums against paks in
 * the ordinary roots and seed the active cache before download comparison. */
static qboolean coduomp_compat_server_namespace_cache_referenced_paks(void)
{
    if (coduomp_namespace_state.active == qfalse ||
        fs_homepath == NULL || fs_basepath == NULL || fs_cdpath == NULL) {
        return qfalse;
    }

    char cacheRoot[MAX_OSPATH];
    if (strlen(fs_homepath->string) + strlen("server-cache/v1") + 3u >
        sizeof(cacheRoot)) {
        return qfalse;
    }
    FS_BuildOSPath(fs_homepath->string, "server-cache/v1", "",
                   cacheRoot);
    cacheRoot[strlen(cacheRoot) - 1u] = '\0';

    qboolean copiedAny = qfalse;
    for (int32_t pakIndex = 0;
         pakIndex < fs_numServerReferencedPaks; ++pakIndex) {
        const char *const pakName =
            fs_serverReferencedPakNames[pakIndex];
        if (pakName == NULL || pakName[0] == '\0' ||
            strchr(pakName, '@') != NULL ||
            strpbrk(pakName, "/\\") == NULL ||
            coduo_compat_path_is_safe_relative(pakName) == qfalse ||
            FS_idPak(pakName, "main", fs_basegame->string) != qfalse ||
            FS_serverPak(pakName) != qfalse) {
            continue;
        }

        const int32_t expectedChecksum =
            fs_serverReferencedPaks[pakIndex];
        const pack_t *rootPack = NULL;
        qboolean alreadyCached = qfalse;
        qboolean officialRootPackAvailable = qfalse;
        for (searchpath_t *search = fs_searchpaths;
             search != NULL; search = search->next) {
            const pack_t *const pack = search->pack;
            if (pack == NULL || pack->checksum != expectedChecksum)
                continue;
            if (coduomp_namespace_path_within(
                    pack->pakFilename,
                    coduomp_namespace_state.contentRoot) != qfalse) {
                alreadyCached = qtrue;
                continue;
            }
            if (coduomp_namespace_pack_is_from_ordinary_root(
                    pack, cacheRoot) == qfalse)
                continue;
            if (coduomp_namespace_pack_is_official(pack) != qfalse)
                officialRootPackAvailable = qtrue;
            else if (rootPack == NULL)
                rootPack = pack;
        }

        /* Stock accepts an installed pak by checksum without relocating it
         * to the server-published qpath. Relocating an official main pak under
         * an alias such as uo/pak5 changes search priority and can override
         * expansion assets that precede main/pak5 in the stock layout. */
        if (officialRootPackAvailable != qfalse)
            continue;

        if (alreadyCached == qfalse && rootPack != NULL &&
            coduomp_namespace_copy_root_pak(
                rootPack, pakName, expectedChecksum) != qfalse) {
            copiedAny = qtrue;
            continue;
        }
        if (alreadyCached != qfalse || rootPack != NULL)
            continue;

        if (coduomp_namespace_probe_and_copy_root_pak(
                fs_homepath->string, cacheRoot, pakName,
                expectedChecksum) != qfalse ||
            (Q_stricmp(fs_basepath->string,
                       fs_homepath->string) != 0 &&
             coduomp_namespace_probe_and_copy_root_pak(
                 fs_basepath->string, cacheRoot, pakName,
                 expectedChecksum) != qfalse) ||
            (Q_stricmp(fs_cdpath->string,
                       fs_homepath->string) != 0 &&
             Q_stricmp(fs_cdpath->string,
                       fs_basepath->string) != 0 &&
             coduomp_namespace_probe_and_copy_root_pak(
                 fs_cdpath->string, cacheRoot, pakName,
                 expectedChecksum) != qfalse)) {
            copiedAny = qtrue;
        }
    }
    return copiedAny;
}

static void coduomp_namespace_clear_config_tree(
    const char *directoryPath, int32_t *removedCount,
    int32_t *failedCount)
{
    int32_t configCount = 0;
    char **const configs = Sys_ListFiles(
        directoryPath, ".cfg", NULL, &configCount, qfalse);
    for (int32_t index = 0; index < configCount; ++index) {
        char configPath[MAX_OSPATH];
        if (coduomp_namespace_build_safe_child_path(
                directoryPath, configs[index], qfalse,
                configPath) == qfalse ||
            remove(configPath) != 0) {
            ++*failedCount;
            Com_Printf("Could not remove server config %s%c%s.\n",
                       directoryPath, FS_HOST_PATH_SEPARATOR,
                       configs[index]);
            continue;
        }
        ++*removedCount;
    }
    Sys_FreeFileList(configs);

    int32_t directoryCount = 0;
    char **const directories = Sys_ListFiles(
        directoryPath, NULL, NULL, &directoryCount, qtrue);
    for (int32_t index = 0; index < directoryCount; ++index) {
        char childPath[MAX_OSPATH];
        if (coduomp_namespace_build_safe_child_path(
                directoryPath, directories[index], qtrue,
                childPath) != qfalse) {
            coduomp_namespace_clear_config_tree(
                childPath, removedCount, failedCount);
        }
    }
    Sys_FreeFileList(directories);
}

static void coduomp_compat_server_namespace_clear_configs(void)
{
    char cacheRoot[MAX_OSPATH];
    if (fs_homepath == NULL || fs_homepath->string[0] == '\0' ||
        strlen(fs_homepath->string) + strlen("server-cache/v1") + 3u >
            sizeof(cacheRoot)) {
        Com_Printf("Server configuration cache path is unavailable.\n");
        return;
    }
    FS_BuildOSPath(fs_homepath->string, "server-cache/v1", "",
                   cacheRoot);
    cacheRoot[strlen(cacheRoot) - 1u] = '\0';

    int32_t namespaceCount = 0;
    char **const namespaces = Sys_ListFiles(
        cacheRoot, NULL, NULL, &namespaceCount, qtrue);
    int32_t removedCount = 0;
    int32_t failedCount = 0;
    for (int32_t index = 0; index < namespaceCount; ++index) {
        if (coduomp_namespace_directory_name_is_safe(
                namespaces[index]) == qfalse) {
            continue;
        }

        char namespacePath[MAX_OSPATH];
        if (coduomp_namespace_build_safe_child_path(
                cacheRoot, namespaces[index], qtrue,
                namespacePath) == qfalse) {
            continue;
        }
        char statePath[MAX_OSPATH];
        if (coduomp_namespace_build_safe_child_path(
                namespacePath, "state", qtrue,
                statePath) != qfalse) {
            coduomp_namespace_clear_config_tree(
                statePath, &removedCount, &failedCount);
        }
    }
    Sys_FreeFileList(namespaces);
    if (removedCount != 0)
        coduomp_case_path_cache_clear();

    Com_Printf("Cleared %d isolated server config file%s.\n",
               removedCount, removedCount == 1 ? "" : "s");
    if (failedCount != 0) {
        Com_Printf("Could not clear %d isolated server config file%s.\n",
                   failedCount, failedCount == 1 ? "" : "s");
    }
}

static void coduomp_compat_server_namespace_reset(void)
{
    coduomp_namespace_free_snapshot();
    memset(&coduomp_namespace_state, 0, sizeof(coduomp_namespace_state));
}

static qboolean coduomp_compat_server_namespace_activate(
    const netadr_t *address, const char *serverName,
    qboolean eligibleRemoteServer)
{
    char hash[CODUOMP_NAMESPACE_HASH_HEX_LENGTH + 1];
    char slug[CODUOMP_NAMESPACE_SLUG_LENGTH + 1];
    char directoryName[MAX_QPATH];

    if (eligibleRemoteServer == qfalse)
        return qfalse;
    if (address == NULL || fs_homepath == NULL ||
        fs_homepath->string[0] == '\0' ||
        coduomp_namespace_endpoint_hash(address, hash) == qfalse) {
        Com_Error(ERR_DROP,
                  "Cannot isolate files for this server address\n");
    }
    if (coduomp_namespace_state.active != qfalse &&
        strcmp(coduomp_namespace_state.endpointHash, hash) == 0) {
        return qfalse;
    }
    if (coduomp_namespace_state.active != qfalse) {
        coduomp_namespace_restore_snapshot();
        coduomp_namespace_free_snapshot();
        coduomp_namespace_state.active = qfalse;
    }

    /* Flush the user's current global state before the writable root changes.
     * Thereafter every automatic write lands in the transient server root. */
    Com_WriteConfiguration();
    if (coduomp_namespace_capture_snapshot() == qfalse) {
        Com_Error(ERR_DROP,
                  "Could not isolate server configuration\n");
    }

    coduomp_namespace_sanitize_name(serverName, slug);
    if (coduomp_namespace_select_directory(
            fs_homepath->string, slug, hash, directoryName) == qfalse ||
        coduomp_namespace_build_roots(
            fs_homepath->string, directoryName) == qfalse) {
        coduomp_namespace_free_snapshot();
        Com_Error(ERR_DROP,
                  "Server namespace path is too long\n");
    }

    Q_strncpyz(coduomp_namespace_state.endpointHash, hash,
               sizeof(coduomp_namespace_state.endpointHash));
    coduomp_namespace_state.active = qtrue;
    Com_Printf("Server files isolated in %s\n", directoryName);
    return qtrue;
}

static qboolean coduomp_compat_server_namespace_deactivate(void)
{
    if (coduomp_namespace_state.active == qfalse)
        return qfalse;

    coduomp_namespace_restore_snapshot();
    coduomp_namespace_free_snapshot();
    coduomp_namespace_state.active = qfalse;
    coduomp_namespace_state.endpointHash[0] = '\0';
    coduomp_namespace_state.stateRoot[0] = '\0';
    coduomp_namespace_state.contentRoot[0] = '\0';
    return qtrue;
}

static qboolean coduomp_compat_server_namespace_is_active(void)
{
    return coduomp_namespace_state.active;
}

static const char *coduomp_compat_server_namespace_state_root(
    const char *ordinaryHomeRoot)
{
    return coduomp_namespace_state.active != qfalse
               ? coduomp_namespace_state.stateRoot
               : ordinaryHomeRoot;
}

static const char *coduomp_compat_server_namespace_content_root(
    const char *ordinaryHomeRoot)
{
    return coduomp_namespace_state.active != qfalse
               ? coduomp_namespace_state.contentRoot
               : ordinaryHomeRoot;
}

/* NOT_FROM_ORIGINAL_SOURCE: limit duplicate-check scans to cached names that
 * can be server-published aliases of the retail main/basegame pak families. */
static qboolean coduomp_namespace_pack_name_may_alias_official(
    const pack_t *pack)
{
    return pack != NULL &&
                   (Q_stricmpn(pack->pakBasename, "pak", 3) == 0 ||
                    Q_stricmpn(pack->pakBasename, "pakuo", 5) == 0 ||
                    Q_stricmpn(pack->pakBasename, "mp_pak", 6) == 0 ||
                    Q_stricmpn(pack->pakBasename, "sp_pak", 6) == 0 ||
                    Q_stricmp(pack->pakBasename, "mp_bin") == 0 ||
                    Q_stricmpn(pack->pakBasename, "localized_", 10) == 0)
               ? qtrue
               : qfalse;
}

/* NOT_FROM_ORIGINAL_SOURCE: cached aliases of an installed official pak must
 * not gain the alias directory's search priority. Stock satisfies the
 * server's checksum with the already-mounted official pak in its canonical
 * game directory and never creates the relocated copy. This also makes stale
 * aliases produced by older builds inert without deleting user cache data. */
static qboolean coduomp_namespace_pack_duplicates_official_root(
    const pack_t *pack)
{
    if (pack == NULL)
        return qfalse;

    for (const searchpath_t *search = fs_searchpaths;
         search != NULL; search = search->next) {
        const pack_t *const candidate = search->pack;
        if (candidate == NULL || candidate == pack ||
            candidate->checksum != pack->checksum ||
            coduomp_namespace_pack_is_official(candidate) == qfalse ||
            coduomp_namespace_path_within(
                candidate->pakFilename,
                coduomp_namespace_state.contentRoot) != qfalse ||
            coduomp_namespace_path_within(
                candidate->pakFilename,
                coduomp_namespace_state.stateRoot) != qfalse) {
            continue;
        }
        return qtrue;
    }
    return qfalse;
}

static qboolean coduomp_compat_server_namespace_allows(
    const searchpath_t *searchpath)
{
    if (coduomp_namespace_state.active == qfalse || searchpath == NULL)
        return qtrue;

    if (searchpath->pack != NULL) {
        const pack_t *const pack = searchpath->pack;
        const qboolean cachedPack =
            coduomp_namespace_path_within(
                pack->pakFilename,
                coduomp_namespace_state.contentRoot) != qfalse ||
            coduomp_namespace_path_within(
                pack->pakFilename,
                coduomp_namespace_state.stateRoot) != qfalse;

        /* FS_idPak builds its comparison strings in shared va() storage.
         * This policy runs inside FS_UseSearchPath, whose active qpath may
         * itself occupy that storage, so calling FS_idPak here corrupts the
         * lookup in progress. Keep classification side-effect-free. */
        if (cachedPack != qfalse) {
            return coduomp_namespace_pack_name_may_alias_official(pack) ==
                               qfalse ||
                           coduomp_namespace_pack_duplicates_official_root(
                               pack) == qfalse
                       ? qtrue : qfalse;
        }
        return coduomp_namespace_pack_is_official(pack);
    }

    if (searchpath->dir != NULL) {
        const directory_t *const directory = searchpath->dir;
        if (coduomp_namespace_path_equal(
                directory->path,
                coduomp_namespace_state.contentRoot) != qfalse ||
            coduomp_namespace_path_equal(
                directory->path,
                coduomp_namespace_state.stateRoot) != qfalse) {
            return qtrue;
        }
        if (coduomp_namespace_game_is_official(directory->gamedir) != qfalse &&
            (coduomp_namespace_path_equal(
                 directory->path, fs_basepath->string) != qfalse ||
             coduomp_namespace_path_equal(
                 directory->path, fs_cdpath->string) != qfalse)) {
            return qtrue;
        }
    }
    return qfalse;
}

const coduomp_server_namespace_provider_t
    coduomp_server_namespace_provider = {
        coduomp_compat_server_namespace_reset,
        coduomp_compat_server_namespace_activate,
        coduomp_compat_server_namespace_deactivate,
        coduomp_compat_server_namespace_is_active,
        coduomp_compat_server_namespace_cache_referenced_paks,
        coduomp_compat_server_namespace_append_cached_mods,
        coduomp_compat_server_namespace_state_root,
        coduomp_compat_server_namespace_content_root,
        coduomp_compat_server_namespace_allows,
        coduomp_compat_server_namespace_promote_config,
        coduomp_compat_server_namespace_clear_configs,
    };
