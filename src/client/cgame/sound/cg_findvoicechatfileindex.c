// Source: uo_cgame_mp_x86.dll 0x30039d80..0x30039f0b
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_30039d80_30039f0b.mcode
//
// CG_FindVoiceChatFileIndex — resolve a voice-chat command file to its table index.
//
// Behavior: open the file named `fileName` (CG_FS_FOPEN_FILE, mode FS_READ). If the
// returned file handle is 0 (not found), print "voice chat file not found: %s\n" and
// return -1. If the byte length is >= 0x4000 (would overflow the 0x4000-byte read
// buffer), print "^1voice chat file too large: %s is %i, max allowed is %i", close
// the handle, and return -1. Otherwise read up to 0x4000 bytes into a stack buffer,
// NUL-terminate at [length], close the handle, then tokenize the first token with
// Com_Parse (honoring com_parseSession's one-token pushback). If the token is
// NULL/empty, return -1. Finally, walk the eight cg_voiceChatTables[] blocks and
// return the index of the first block whose leading name case-insensitively matches
// the token (Q_stricmpn with the 99999 == Q_stricmp limit); return -1 on no match.
//
// Naming: the .mcode size-match guess "BG_CalculateWeaponPosition_DamageKick" is
// REJECTED (win size 0x18b == some PPC function; zero behavioral basis, and it is a
// cgame function, not bg). There is no weapon-position/weapon-movement math anywhere in
// the body — it is va/Q_stricmpn/Com_Parse string work over the mp/*_chat.voice
// tables. Named CG_FindVoiceChatFileIndex from that proven role.
//
// Callees resolved by behavior / call graph (their size-guess names are rejected):
//   0x30060a30  __chkstk / _alloca_probe — MSVC stack-probe prologue that reserves
//               the 0x400c-byte frame (EAX=0x400c). Not a game call; the C frame
//               (a 0x4000 buffer + locals) is the portable equivalent.
//   0x3004e8a0  va(format, ...)            — already reconstructed.
//   0x3004d6b0  Com_ParseExt(char **data_p, qboolean allowLineBreaks) — the Com script
//               tokenizer core; already reconstructed.
//   0x3004e620  Q_stricmpn(s1, s2, limit)  — case-insensitive length-limited compare;
//               limit 0x1869f (99999) is the canonical Q_stricmp idiom.
//   0x30061639  __security_check_cookie (/GS) — validates the frame cookie loaded
//               from [0x30081650]; handled by the C frame, not re-expressed.
//   *0x30085e9c cgame_syscall — the cgame VM trap vector (FS_FOPEN_FILE/READ/
//               FCLOSE_FILE and CG_PRINT here).
//
// ABI: `fileName` arrives in ECX (fastcall-ish register parameter; the entry
// `MOV EDI,ECX` at 0x30039d9f forwards it to the file-open call with no stack arg).
// ESI/EDI/EBP are the callee-saved registers. Expressed here as plain C. Returns -1
// on any failure and the matched table index (0..7) on success.

#include "../client_recovered.h"
#include "../globals.h"

#include <stddef.h>
#include <stdint.h>

int32_t CG_FindVoiceChatFileIndex(const char *fileName)
{
    // fileHandle out slot ([ESP+0xc] pre-push / read back at [ESP+0x18]).
    int32_t fileHandle;
    // 0x4000-byte file-text buffer (base [ESP+0x28]); +1 for the NUL at [length].
    char fileText[16385];
    // ESI = byte length returned by CG_FS_FOPEN_FILE.
    int32_t length;

    // 0x30039d98..0x30039daa
    //   length = cgame_syscall(CG_FS_FOPEN_FILE, fileName, &fileHandle, FS_READ);
    // FS_READ mode is the literal 0 pushed at 0x30039d98; ESI = length (MOV ESI,EAX).
    length = coduo_int32_from_bits((uint32_t)cgame_syscall(
        CG_FS_FOPEN_FILE,
        (intptr_t)fileName,
        (intptr_t)&fileHandle,
        FS_READ));

    // 0x30039dac MOV EAX,[fileHandle] ; 0x30039db3 TEST EAX,EAX ; 0x30039db5 JNZ
    // A zero handle means the file was not found.
    if (fileHandle == 0) {
        // 0x30039db7..0x30039dcb
        //   cgame_syscall(CG_PRINT, va("voice chat file not found: %s\n", fileName));
        cgame_syscall(CG_PRINT,
                      (intptr_t)va("voice chat file not found: %s\n",
                                            fileName));
        // 0x30039dcf OR EAX,0xffffffff ; ... RET
        return -1;
    }

    // 0x30039de6 CMP ESI,0x4000 ; 0x30039dec JL 0x30039e30
    // A file of 0x4000 bytes or more will not fit in the read buffer.
    if (length >= 0x4000) {
        // 0x30039dee..0x30039e08
        //   cgame_syscall(CG_PRINT,
        //       va("^1voice chat file too large: %s is %i, max allowed is %i",
        //          fileName, length, 0x4000));
        // The .rdata literal at 0x3007a2c4 has NO trailing '\n' (unlike the
        // not-found message above).
        cgame_syscall(CG_PRINT,
                      (intptr_t)va(
                          "^1voice chat file too large: %s is %i, max allowed is %i",
                          fileName, length, 0x4000));
        // 0x30039e08..0x30039e15
        //   cgame_syscall(CG_FS_FCLOSE_FILE, fileHandle);
        cgame_syscall(CG_FS_FCLOSE_FILE, fileHandle);
        // 0x30039e19 OR EAX,0xffffffff ; ... RET
        return -1;
    }

    // ---- success path (0x30039e30..) : read, terminate, close ----

    // 0x30039e30..0x30039e3f
    //   cgame_syscall(CG_FS_READ, fileText, length, fileHandle);
    cgame_syscall(CG_FS_READ,
                  (intptr_t)fileText,
                  length,
                  fileHandle);

    // 0x30039e46 MOV byte ptr [ESP + ESI + 0x28],0x0  => fileText[length] = '\0'.
    fileText[length] = '\0';

    // 0x30039e43..0x30039e4b
    //   cgame_syscall(CG_FS_FCLOSE_FILE, fileHandle);
    cgame_syscall(CG_FS_FCLOSE_FILE, fileHandle);

    // 0x30039e51..0x30039e8b : set up the Com parser data pointer and honor the
    // com_parseSession one-token pushback (Com_UnParse-style restore).
    //   data_p = fileText;                               ([ESP+0x24]/[ESP+0xc])
    //   if (com_parseSession->ungetToken) {
    //       data_p = com_parseSession->savedParse;       (+0x418)
    //       com_parseSession->line = com_parseSession->savedLine;  (+0x414/+0x400)
    //       com_parseSession->ungetToken = 0;            (+0x404)
    //   }
    char *data_p = fileText;
    if (com_parseSession->ungetToken != 0) {
        data_p = com_parseSession->savedParse;
        com_parseSession->line = com_parseSession->savedLine;
        com_parseSession->ungetToken = 0;
    }

    // 0x30039e8b..0x30039e99
    //   token = Com_ParseExt(&data_p, qtrue);   (allowLineBreaks = 1)
    // EDI = token (MOV EDI,EAX).
    char *token = Com_ParseExt(&data_p, qtrue);

    // 0x30039e9c TEST EDI,EDI ; JZ  -> return -1
    // 0x30039ea4 CMP byte ptr [EDI],0 ; JZ -> return -1
    // A NULL or empty first token means the file yielded no command name.
    if (token == NULL || token[0] == '\0') {
        return -1;
    }

    // 0x30039ead..0x30039ee8 : scan the eight voice-chat table blocks (base
    // 0x3016a9e0, stride 0x49148, sentinel 0x303b3420) and return the index of the
    // first block whose leading name case-insensitively equals the token.
    //   0x30039eae XOR EBP,EBP        : index = 0.
    //   0x30039eb0 MOV ESI,0x3016a9e0 : block cursor.
    //   0x30039eb5 TEST ESI,ESI / JZ  : cursor is always non-NULL here; the guard is
    //     a compiler artifact and can never take the JZ (drops to the ADD/INC step).
    //   0x30039eb9 MOV EAX,0x1869f ; MOV ECX,ESI ; MOV EDX,EDI ; CALL Q_stricmpn
    //     -> Q_stricmpn(block, token, 99999); JZ on match.
    for (int32_t index = 0; index < CG_VOICE_CHAT_TABLE_COUNT; index++) {
        if (Q_stricmpn(cg_voiceChatTables[index].fileName, token, 99999) == 0) {
            // 0x30039ef3..0x30039f0a : MOV EAX,EBP ; ... RET  -> return the index.
            return index;
        }
        // 0x30039ecb ADD ESI,0x49148 ; 0x30039ed1 INC EBP ;
        // 0x30039ed2 CMP ESI,0x303b3420 ; JL -> keep scanning.
    }

    // 0x30039eda..0x30039ef2 : OR EAX,0xffffffff ; ... RET  -> no matching table.
    return -1;
}
