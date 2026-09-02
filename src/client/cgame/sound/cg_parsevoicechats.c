// Source: uo_cgame_mp_x86.dll 0x300396f0..0x30039d25
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_300396f0_30039d25.mcode
//
// CG_ParseVoiceChats - load one .voice file into the fixed client voice-chat
// table. The former ClientSpawn label was a size-only server match.

#include "client/cgame/client_recovered.h"
#include "client/cgame/globals.h"

#include <stdint.h>
#include <string.h>

enum {
    CG_VOICE_FILE_BYTES = 0x4000,
    CG_VOICE_DEFAULT_ICON_SORT = 2
};

qboolean CG_ParseVoiceChats(const char *fileName, cgVoiceChatTable_t *table, int32_t maxVoiceChats)
{
    char fileText[CG_VOICE_FILE_BYTES + 1];
    char *parse;
    int32_t fileHandle;
    int32_t length;
    char *token;

    length = coduo_int32_from_bits((uint32_t)cgame_syscall(CG_FS_FOPEN_FILE, (intptr_t)fileName, (intptr_t)&fileHandle, FS_READ));
    if (fileHandle == 0) {
        cgame_syscall(CG_PRINT, (intptr_t)va("^1voice chat file not found: %s\n", fileName));
        return qfalse;
    }
    if (length >= CG_VOICE_FILE_BYTES) {
        cgame_syscall(CG_PRINT,
                      (intptr_t)va("^1voice chat file too large: %s is %i, max allowed is %i", fileName, length, CG_VOICE_FILE_BYTES));
        cgame_syscall(CG_FS_FCLOSE_FILE, fileHandle);
        return qfalse;
    }

    cgame_syscall(CG_FS_READ, (intptr_t)fileText, length, fileHandle);
    fileText[length] = '\0';
    cgame_syscall(CG_FS_FCLOSE_FILE, fileHandle);

    Q_strncpyz(table->fileName, fileName, sizeof(table->fileName));
    for (int32_t i = 0; i < maxVoiceChats; i = coduo_int32_from_bits((uint32_t)i + 1u)) {
        table->entries[i].name[0] = '\0';
    }

    parse = fileText;
    token = Com_ParseExt(&parse, qtrue);
    if (token == NULL || token[0] == '\0') {
        return qtrue;
    }
    if (Q_stricmpn(token, "female", 99999) == 0) {
        table->gender = CG_VOICE_GENDER_FEMALE;
    } else if (Q_stricmpn(token, "male", 99999) == 0) {
        table->gender = CG_VOICE_GENDER_MALE;
    } else if (Q_stricmpn(token, "neuter", 99999) == 0) {
        table->gender = CG_VOICE_GENDER_NEUTER;
    } else {
        cgame_syscall(CG_PRINT, (intptr_t)va("^1expected gender not found in voice chat file: %s\n", fileName));
        return qfalse;
    }

    table->entryCount = 0;
    while (table->entryCount < maxVoiceChats) {
        cgVoiceChatEntry_t *entry;

        token = Com_ParseExt(&parse, qtrue);
        if (token == NULL || token[0] == '\0') {
            return qtrue;
        }

        entry = &table->entries[table->entryCount];
        Q_strncpyz(entry->name, token, sizeof(entry->name));

        token = Com_ParseExt(&parse, qtrue);
        /* 0x30039959: the binary tests ONLY token==NULL here (cmp esi,edi; je 0x30039ca3)
         * and routes it -- exactly like a non-"{" token -- to the "expected {" error
         * block (qfalse). There is NO early return-qtrue and no token[0] empty check; an
         * empty token fails Q_stricmpn("{") and lands in the same block. A prior pass
         * added `token==NULL || token[0]=='\0' -> return qtrue`, turning a truncated file
         * (EOF right after an entry name) into a spurious SUCCESS. */
        if (token == NULL || Q_stricmpn(token, "{", 99999) != 0) {
            cgame_syscall(CG_PRINT, (intptr_t)va("^1expected { found %s in voice chat file: %s\n", token, fileName));
            return qfalse;
        }

        entry->variantCount = 0;
        while (entry->variantCount < CG_MAX_VOICE_CHATS) {
            int32_t variant = entry->variantCount;
            qhandle_t icon;

            token = Com_ParseExt(&parse, qtrue);
            if (token == NULL || token[0] == '\0') {
                return qtrue;
            }
            if (Q_stricmpn(token, "}", 99999) == 0) {
                table->entryCount = coduo_int32_from_bits((uint32_t)table->entryCount + 1u);
                break;
            }

            entry->sounds[variant] = trap_Com_SoundAliasString(token);

            token = Com_ParseExt(&parse, qtrue);
            if (token == NULL || token[0] == '\0') {
                return qtrue;
            }
            Q_strncpyz(entry->text[variant], token, sizeof(entry->text[variant]));

            token = Com_ParseOnLine(&parse);
            /* 0x30039c10-13: an EMPTY on-line token (token[0]==0) is routed to this
             * default block too (cmp byte[eax],0; je 0x30039b97), alongside token==NULL
             * (0x30039b52 je) and token=="}". The block registers headiconVoiceChat FIRST
             * (0x30039b9b) then sets the unget flag UNCONDITIONALLY (0x30039be6), even
             * when token==NULL. A prior pass omitted the empty-token case (it fell to the
             * else arm and called CG_RegisterMaterial("")), and guarded the unget with
             * token != NULL. */
            if (token == NULL || token[0] == '\0' || Q_stricmpn(token, "}", 99999) == 0) {
                icon = CG_RegisterMaterial("headiconVoiceChat", CG_VOICE_DEFAULT_ICON_SORT);
                Com_UngetToken();
            } else {
                icon = CG_RegisterMaterial(token, CG_VOICE_DEFAULT_ICON_SORT);
                if (icon == 0) {
                    icon = CG_RegisterMaterial("headiconVoiceChat", CG_VOICE_DEFAULT_ICON_SORT);
                }
            }
            entry->icons[variant] = icon;
            entry->variantCount = coduo_int32_from_bits((uint32_t)entry->variantCount + 1u);
        }
        if (entry->variantCount >= CG_MAX_VOICE_CHATS) {
            table->entryCount = coduo_int32_from_bits((uint32_t)table->entryCount + 1u);
        }
    }

    return qtrue;
}
