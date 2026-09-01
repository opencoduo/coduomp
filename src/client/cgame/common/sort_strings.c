// Source: uo_cgame_mp_x86.dll 0x3001df90..0x3001dfa9
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3001df90_3001dfa9.mcode

#include "client/cgame/client_recovered.h"

/*
 * SortStringPtrsCaseInsensitive — qsort() string-pointer comparator.
 *
 * NAMING: the mechanical `# name G_ClientCanSpectateTeam` is a pure size match
 * (win size 0x19 == corpus size 0x19) and is REJECTED — the body is a two-level
 * (char **) comparator that tail-calls a string compare, not a client/team
 * spectate predicate. It is passed as a function pointer to qsort at 0x3001e1f2:
 *   push 0x3001df90 (compar); push 4 (element size); push count; push base;
 *   call 0x3005ba40 (qsort); add esp,0x10
 * Element size 4 and the double dereference prove each array element is a
 * `char *`, so the comparator receives two `const void *` that are really
 * `char **`.
 *
 * Machine code (0x3001df90..0x3001dfa9), all traced:
 *   MOV EAX,[ESP+0x4]   ; a = first arg  (char **)
 *   MOV ECX,[ESP+0x8]   ; b = second arg (char **)
 *   MOV EAX,[EAX]       ; *a  -> char *  (the first string)
 *   MOV ECX,[ECX]       ; *b  -> char *  (the second string)
 *   MOV [ESP+0x8],ECX   ; overwrite the incoming stack args in place so the
 *   MOV [ESP+0x4],EAX   ;   tail-call sees (s1=*a, s2=*b)
 *   JMP 0x30069275      ; tail-call coduo_crt_stricmp(s1, s2); its return and
 *                       ;   cdecl stack shape become this comparator's
 *
 * The JMP (not CALL) is an i386 tail-call: the CRT comparator is cdecl over the
 * same two stack slots, so its signed case-folded difference of the first
 * differing character is returned directly to qsort. Equal -> 0, which is the
 * standard comparator contract.
 *
 * Exact source name unresolved: no server proto is address-proven for this
 * comparator. The server bank has a same-shaped `compare_weaponfile_names`
 * (items.c) qsort comparator, but the caller's list contents are not proven to
 * be weapon-file names, so that name is not adopted here.
 */
int SortStringPtrsCaseInsensitive(const void *a, const void *b)
{
    const char *s1 = *(const char *const *)a;
    const char *s2 = *(const char *const *)b;
    return (int)coduo_crt_stricmp(s1, s2);
}
