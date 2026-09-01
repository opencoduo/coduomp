#include "globals.h"
#include "client_recovered.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* NOT_FROM_ORIGINAL_SOURCE: immutable copy of the PE initializer for the one
 * statically initialized table whose weapon-registration pass rewrites rows. */
static const gitem_t cgame_compat_bg_itemlist_load_state[] = {
#include "recovered_initializers/bg_itemlist.inc"
};
_Static_assert(sizeof(cgame_compat_bg_itemlist_load_state) ==
                   sizeof(bg_itemlist),
               "bg_itemlist load-state template must cover the whole table");

/* NOT_FROM_ORIGINAL_SOURCE: immutable copy of the five PE-initialized flame
 * sprite words. CG_InitFlameChunks changes the second word from one to zero. */
static const uint32_t cgame_compat_flame_sprite_load_state[] = {
    1u, 1u, UINT32_C(0x3f800000), UINT32_C(0x3f800000),
    UINT32_C(0x3f800000)
};
_Static_assert(sizeof(cgame_compat_flame_sprite_load_state) ==
                   sizeof(cg_flameSpriteState),
               "flame sprite load-state template must cover every word");

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of CG_Init's original
 * monolithic cgs_t clear. The recovered globals are independent native objects,
 * so crossing from cg_gameState into adjacent linker symbols is not valid C. */
void cgame_compat_reset_recovered_cgs_state(void)
{
#define CG_ZERO_ORIGINAL_OBJECT(object_) memset(&(object_), 0, sizeof(object_))
#include "recovered_initializers/cg_init_cgs_reset.inc"
#undef CG_ZERO_ORIGINAL_OBJECT
}

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of CG_Init's original
 * monolithic cg_t clear; see cgame_compat_reset_recovered_cgs_state. */
void cgame_compat_reset_recovered_cg_state(void)
{
#define CG_ZERO_ORIGINAL_OBJECT(object_) memset(&(object_), 0, sizeof(object_))
#include "recovered_initializers/cg_init_cg_reset.inc"
#undef CG_ZERO_ORIGINAL_OBJECT
}

/* NOT_FROM_ORIGINAL_SOURCE: retail reaches dllEntry through a freshly mapped
 * PE image. Native loaders are allowed to retain a closed image, so dllEntry
 * explicitly restores the behavior-bearing load state that CG_Init does not
 * reconstruct on every platform. Scratch payload bytes whose cursors are reset
 * or which are overwritten before every read do not need clearing. The syscall
 * carrier is deliberately excluded; dllEntry replaces it immediately. */
void cgame_compat_reset_module_load_state(void)
{
    int32_t index;
    keywordHash_t *itemKeyword;
    menuKeywordHash_t *menuKeyword;
    bg_indexed_string_t *indexedStringTables[] = {
        animStateStr, bgAnimGroupStrings, bgAnimEventStrings,
        animBodyPartsStr, animMountedStr, animVehicleMotionStr,
        animVehicleStr, animWeaponClassStr, animWeaponPositionStr,
        animStrafeStateStr, bgAnimConditionTypeStrings,
        bgAnimParseSectionStrings
    };

    cgame_compat_reset_recovered_cgs_state();
    cgame_compat_reset_recovered_cg_state();

#define CG_ZERO_LOAD_OBJECT(object_) memset(&(object_), 0, sizeof(object_))
#include "recovered_initializers/cgame_module_load_reset.inc"
#undef CG_ZERO_LOAD_OBJECT

    PMDebugLastWeaponState = -1;
    PMDebugLastWeaponAnim = UINT32_MAX;
    cg_stanceHintChangeTime = -1;
    cg_stanceHintFlags = UINT32_MAX;
    cg_stanceHintExpireTime = -1;
    cg_statBarHoldSeed = 1;
    cg_statBarHoldTimer = 1;
    cg_statBarLastClientNum = -1;
    cg_scoreboardLeadTeam = 2;
    sharedRandSeed = UINT32_C(0x89abcdef);
    bgPlayerAnimScriptPath = "mp/playeranim.script";

    memcpy(bg_itemlist, cgame_compat_bg_itemlist_load_state,
           sizeof(bg_itemlist));
    memcpy(cg_flameSpriteState, cgame_compat_flame_sprite_load_state,
           sizeof(cg_flameSpriteState));
    for (index = 0; index < CONTROL_BINDING_COUNT; ++index) {
        g_bindings[index].bind1 = -1;
        g_bindings[index].bind2 = 0;
    }
    for (itemKeyword = itemParseKeywords;; ++itemKeyword) {
        itemKeyword->next = NULL;
        if (itemKeyword->keyword == NULL)
            break;
    }
    for (menuKeyword = menuParseKeywords;; ++menuKeyword) {
        menuKeyword->next = NULL;
        if (menuKeyword->keyword == NULL)
            break;
    }
    for (index = 0;
         index < (int32_t)(sizeof(indexedStringTables) /
                           sizeof(indexedStringTables[0]));
         ++index) {
        bg_indexed_string_t *entry = indexedStringTables[index];

        for (;;) {
            entry->hash = BG_INDEXED_STRING_HASH_UNSET;
            if (entry->name == NULL)
                break;
            ++entry;
        }
    }

    com_parseSessions[0].line = 1;
    com_parseSessions[0].spaceDelimited = qtrue;
    com_parseSessions[0].parseNegativeNumbers = qtrue;
    com_parseSessions[0].savedLine = 1;
    com_parseSession = &com_parseSessions[0];

    cgame_compat_reset_presentation_state();
    srand(1);
}

/*
 * Source address: uo_cgame_mp_x86.dll 0x30085e9c.
 *
 * The image initializes it to (cgame_syscall_t)-1; dllEntry replaces it with
 * its first argument, and syscall wrappers call through the same address.
 */
#if UINTPTR_MAX == UINT32_MAX
/* Source address: uo_cgame_mp_x86.dll 0x30085e9c. */
cgame_syscall_t cgame_syscall = (cgame_syscall_t)(intptr_t)-1;
#else
/* NOT_FROM_ORIGINAL_SOURCE: native-width vector form of the original
 * command-plus-stack-dwords trap callback. */
cgame_syscall_t cgame_syscall_vector =
    (cgame_syscall_t)(intptr_t)-1;
#endif

/* Source: uo_cgame_mp_x86.dll 0x30538600: first 0x100-byte trap string buffer. */
char cg_trapStringBufferA[CG_HUD_STRING_BUFFER_SIZE];
/* Source: uo_cgame_mp_x86.dll 0x30538700: adjacent second trap string buffer. */
char cg_trapStringBufferB[CG_HUD_STRING_BUFFER_SIZE];

/* Code-referenced owned data. */
/* 0x3006f258 is an interior byte view of this 16-byte image object, used as an
 * indexed signed-byte table base at 0x3005cc39. */
/* Source: uo_cgame_mp_x86.dll 0x30071548 (.rdata); refs=1 width=imm; first=0x30005ab6; owner=g_freevehicle.
 * Example: 30005ab6   b8 48 15 07 30               MOV EAX,0x30071548
 */
const char bg_rightWeaponTagName[17] = "tag_weapon_right";
/* Source: uo_cgame_mp_x86.dll 0x3007155c (.rdata); refs=1 width=imm; first=0x30005aaf; owner=g_freevehicle.
 * Example: 30005aaf   b8 5c 15 07 30               MOV EAX,0x3007155c
 */
const char bg_leftWeaponTagName[16] = "tag_weapon_left";
/* Source: uo_cgame_mp_x86.dll 0x3007156c (.rdata); refs=2 width=imm; first=0x300059b1; owner=g_freevehicle.
 * Example: 300059b1   68 6c 15 07 30               PUSH 0x3007156c | 30005a11   68 6c 15 07 30               PUSH 0x3007156c
 * Mechanical export captured this NUL-terminated string as a truncated first-dword
 * scalar (0x756f4315); repaired in place to the real string. objdump .rdata bytes:
 *   3007156c: 15 43 6f 75 6c 64 20 6e 6f 74 20 6c 6f 61 64 20 6d 6f 64 65 6c 20 27 25 73 27 00
 * i.e. "\x15Could not load model '%s'" (0x15 = console-severity prefix). */
const char bg_couldNotLoadModelErrorFormat[27] = "\x15" "Could not load model '%s'";
/* Source: uo_cgame_mp_x86.dll 0x30071588 (.rdata); refs=2 width=imm; first=0x30005816; owner=normaltolatlong.
 * Example: 30005816   68 88 15 07 30               PUSH 0x30071588 | 3001f0bb   68 88 15 07 30               PUSH 0x30071588
 */
const char cg_originTagName[11] = "tag_origin";
/* Source: uo_cgame_mp_x86.dll 0x30071780 (.rdata) — the cgame client console-command
 * dispatch table (25 entries + NULL terminator, 8-byte stride). The mechanical
 * export split this array into three g_const_u32_* symbols (0x30071780/84/88);
 * they are superseded here by the real cg_consoleCommands[] array. Command names
 * live in .rdata; handlers are provisional (declared in globals.h). The "mr" entry
 * (0x30071840) has a NULL handler, which is why CG_ConsoleCommand null-checks the
 * function pointer before calling. Consumed by CG_ConsoleCommand (0x300178c0) and
 * registered/removed by CG_InitConsoleCommands (0x30017920).
 * PE_RELOCATION_VALUES_VERIFIED: all name and handler pointers match the PE. */
const consoleCommand_t cg_consoleCommands[] = {
    { "viewpos", CG_PrintViewOriginAndSpin_f },
    { "+scores", CG_ScoresDown_f },
    { "-scores", CG_ScoresUp_f },
    { "sizeup", CG_SizeUp_f },
    { "sizedown", CG_SizeDown_f },
    { "weapnext", CG_NextWeapon_f },
    { "weapprev", CG_PrevWeapon_f },
    { "weapalt", CG_AltWeapon_f },
    { "weaponslot", CG_WeaponSlot_f },
    { "tcmd", CG_Tcmd_f },
    { "loadhud", CG_LoadHud_f },
    { "fade", CG_Fade_f },
    { "fxSetTestPosition", CG_FxSetTestPosition },
    { "fxTest", CG_FxTest },
    { "fxRestart", CG_FxRestart },
    { "cg_shellshock", CG_ShellShock_f },
    { "cg_shellshock_load", CG_ShellShock_Load_f },
    { "cg_shellshock_save", CG_ShellShock_Save_f },
    { "tell_target", CG_TellTarget_f },
    { "mp_QuickMessage", CG_QuickMessage_f },
    { "mp_quickmap", CG_QuickMap_f },
    { "VoiceChat", CG_VoiceChat_f },
    { "VoiceTeamChat", CG_TeamVoiceChat_f },
    { "mp_Purchase", CG_OpenWMPurchase_f },
    { "mr", NULL },     /* registered name-only; NULL handler */
    { NULL, NULL }
};
/* Source: uo_cgame_mp_x86.dll 0x30071854 (.rdata) — cg_damageDirShaderParams, the
 * static rotated-quad shader/texcoord param block for the damage-direction HUD arrow
 * (floats {1,1,0,1,0,0,1,0} verified via objdump -s -j .rdata). Consumed by
 * CG_DrawDamageDirectionIndicators (0x3001aad5 PUSHes its address). Supersedes the mechanical
 * single-dword export g_const_float_one_30071854 (which saw only the leading 1.0f). */
const float cg_damageDirShaderParams[8] = {
    1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f
};
/* Source: uo_cgame_mp_x86.dll 0x30071874..0x300718b4 (.rdata); refs=1; first=0x3001ccbd.
 * cg_rotatedPicShaderParams: the static shader/texcoord param block CG_DrawRotatedPic
 * (0x3001cb60) hands by address (PUSH 0x30071874) to CG_R_DRAW_ROTATED_QUAD. Sixteen floats, dumped
 * exactly via objdump -s -j .rdata (values {0,0,1,0, 1,1,0,1, -1,-1,1,-1, 1,1,-1,1}).
 * Supersedes the mechanical single-dword export g_const_u32_00000000_30071874.
 */
const float cg_rotatedPicShaderParams[16] = {
     0.0f,  0.0f,  1.0f,  0.0f,
     1.0f,  1.0f,  0.0f,  1.0f,
    -1.0f, -1.0f,  1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,  1.0f,
};
/* Source: uo_cgame_mp_x86.dll 0x300718b8..0x30071918 (.rdata); consumed by
 * CG_DebugBox (0x3001d970), which indexes it with ESI = 0,8,...,0x58.
 *   3001d9b5   8b 86 bc 18 07 30   MOV EAX,dword ptr [ESI + 0x300718bc]  (edge[i][1])
 *   3001d9c4   8b 86 b8 18 07 30   MOV EAX,dword ptr [ESI + 0x300718b8]  (edge[i][0])
 * These are the 12 edges of an axis-aligned box: each record is a pair of
 * corner indices (0..7) into the 8-corner array. Values verified byte-for-byte
 * from the .rdata dump. Supersedes the two mislabeled mechanical scalars. */
const uint32_t cg_debugBoxEdges[12][2] = {
    { 0, 1 }, { 0, 2 }, { 0, 4 }, { 1, 3 },
    { 1, 5 }, { 2, 3 }, { 2, 6 }, { 3, 7 },
    { 4, 5 }, { 4, 6 }, { 5, 7 }, { 6, 7 },
};
/* Source: uo_cgame_mp_x86.dll 0x30071918..0x30071a38 (.rdata); consumed by
 * CG_DObjGetBoneBoundsWireframe (0x30020020), whose loop does
 *   30020092   ba 18 19 07 30   MOV EDX,0x30071918   (table base)
 *   300200a0   8b 3a            MOV EDI,[EDX]         (selector D)
 *   300200a2   8d 3c 7f         LEA EDI,[EDI+EDI*2]   (D*3)
 *   300200a5   d9 04 be         FLD [ESI+EDI*4]       (src[D*3] = mins/maxs.x)
 * reading three dwords per box endpoint. These are the 24 endpoints of the 12
 * box edges (companion to cg_debugBoxEdges), each { selX, selY, selZ } with each
 * selector 0 (mins) or 1 (maxs). Values verified byte-for-byte from the .rdata
 * dump. Supersedes the mislabeled single-dword scalar g_const_u32_*_30071918. */
const uint32_t cg_boxCornerSelectors[24][3] = {
    { 0, 0, 0 }, { 1, 0, 0 },  /* edge 0: mins.x .. maxs.x  (y=min, z=min) */
    { 0, 0, 0 }, { 0, 1, 0 },  /* edge 1: mins.y .. maxs.y  (x=min, z=min) */
    { 1, 1, 0 }, { 1, 0, 0 },  /* edge 2 */
    { 1, 1, 0 }, { 0, 1, 0 },  /* edge 3 */
    { 0, 0, 1 }, { 1, 0, 1 },  /* edge 4 */
    { 0, 0, 1 }, { 0, 1, 1 },  /* edge 5 */
    { 1, 1, 1 }, { 1, 0, 1 },  /* edge 6 */
    { 1, 1, 1 }, { 0, 1, 1 },  /* edge 7 */
    { 0, 0, 0 }, { 0, 0, 1 },  /* edge 8: mins.z .. maxs.z  (x=min, y=min) */
    { 1, 0, 0 }, { 1, 0, 1 },  /* edge 9 */
    { 0, 1, 0 }, { 0, 1, 1 },  /* edge 10 */
    { 1, 1, 0 }, { 1, 1, 1 },  /* edge 11 */
};
/* Source: uo_cgame_mp_x86.dll 0x30071a3c..0x30071a5c (.rdata); refs=1; first=0x30031f4a.
 * Example: 30031f4a   68 3c 1a 07 30               PUSH 0x30071a3c
 * Bytes (little-endian): 3f800000 3f800000 3f800000 00000000 00000000 00000000
 * 00000000 3f800000 = four texture-coordinate pairs {{1,1},{1,0},{0,0},{0,1}}.
 * CG_DrawTankBarrel hands the table to the trap-0x4c draw callee at 0x3001ccf0;
 * original RE_DrawQuadPic copies all four pairs at 0x004f0468..0x004f04cb. The
 * dword at 0x30071a5c is alignment padding before the next datum at 0x30071a60.
 */
const float cg_turretTagShaderParams[8] = {
    1.0f, 1.0f,
    1.0f, 0.0f,
    0.0f, 0.0f,
    0.0f, 1.0f
};
_Static_assert(sizeof(cg_turretTagShaderParams) == sizeof(vec2_t) * 4u,
               "turret tag must provide one texture coordinate per vertex");
/* Source: uo_cgame_mp_x86.dll 0x30071a60 (.rdata); first=0x30037314.
 * cg_scoreboardColumnValueSelect[5] — per-column value-source selector lane read
 * by CG_DrawScoreboardTeamHeader (0x30037314, stride 0x10); see globals.h. The
 * lane starts at 0x30071a60 (the column table base minus 4): element 0 is the
 * standalone dword 4, elements 1..4 are
 * cg_scoreboardColumns[0..3].nextColumnValueSelect
 * (0,1,2,3). Supersedes the mechanical single-dword g_const_u32_00000004_30071a60.
 * Example: 30037314   8b 86 60 1a 07 30            MOV EAX,dword ptr [ESI + 0x30071a60]
 */
/*
 * Source: uo_cgame_mp_x86.dll 0x30071a60..0x30071ab3 (.rdata) — the leading
 * selector plus scoreboard column-header table. Supersedes the mechanical
 * per-field split at 0x30071a64 (widthFraction 0.05f, 0x3d4ccccd),
 * 0x30071a68 (headerRef 0x30074a0c = ""), and 0x30071a6c (mode 0), which were
 * one .rdata table, not three constants. Field values are the literal .rdata
 * bytes:
 *   entry widthFraction  headerRef          mode
 *     0   0.05f          ""                  0
 *     1   0.41f          ""                  0
 *     2   0.16f          "CGAME_SB_SCORE"    2
 *     3   0.20f          "CGAME_SB_DEATHS"   2
 *     4   0.18f          "CGAME_SB_PING"     2
 * The +0x0c nextColumnValueSelect word is consumed for the following column.
 * Referenced by CG_DrawScoreboard_ListColumnHeaders (0x30036d60) and the
 * per-client score-row drawer (0x30037314/0x30037379/0x3003734a; base loaded to
 * EBP at 0x30037593).
 * PE_RELOCATION_VALUES_VERIFIED: header string pointers match the PE.
 */
const cgScoreboardLayout_t cg_scoreboardLayout = {
  4,
  {
    { 0.05f, "",                0, 0 },
    { 0.41f, "",                0, 1 },
    { 0.16f, "CGAME_SB_SCORE",  2, 2 },
    { 0.20f, "CGAME_SB_DEATHS", 2, 3 },
  },
  { 0.18f, "CGAME_SB_PING", 2 }
};
/* Source: uo_cgame_mp_x86.dll 0x30071b18 (.rdata); refs=2; first=0x30036352.
 * cg_shellshockRandomTable: precomputed 2-D noise/direction table of (x,y) float
 * pairs driving the shellshock screen-blur displacement. Consumed by the per-frame
 * screen-blur update (0x3003c630), which cubic-interpolates a 4-pair window; ref
 * 0x30036352 only CMPs against the base address. 131 pairs = 128 unique + 3
 * duplicated guard rows (0x30071f18 repeats 0x30071b18). Repaired in place from the
 * mechanical single-uint32 g_const_float_neg_0_56355101 (wrong type). Exact source
 * symbol name unresolved; named by proven role. */
const float cg_shellshockRandomTable[CG_SHELLSHOCK_RANDOM_TABLE_ROWS][2] = {
    { -0.563551f, -0.00443f },
    { -0.282062f, -0.757933f },
    { 0.413047f, 0.244246f },
    { 0.527895f, -0.723897f },
    { -0.329777f, 0.6698f },
    { -0.394248f, -0.76309f },
    { 0.126207f, 0.497769f },
    { 0.004986f, -0.01413f },
    { 0.559913f, 0.112825f },
    { -0.333089f, -0.573283f },
    { 0.335404f, -0.107176f },
    { -0.56906f, -0.213141f },
    { -0.166676f, 0.785084f },
    { 0.299592f, 0.037593f },
    { -0.516867f, 0.510759f },
    { 0.138009f, 0.034823f },
    { -0.156167f, 0.829048f },
    { -0.999458f, 0.020317f },
    { 0.300029f, 0.252944f },
    { 0.030215f, -0.295732f },
    { -0.917362f, -0.050711f },
    { 0.044177f, -0.269289f },
    { 0.588424f, 0.362577f },
    { -0.379913f, 0.619214f },
    { 0.204432f, -0.019423f },
    { 0.018499f, 0.468079f },
    { 0.916187f, -0.247878f },
    { 0.003799f, 0.10821f },
    { 0.057363f, 0.60624f },
    { 0.324595f, 0.158733f },
    { -0.130529f, -0.183388f },
    { 0.715672f, -0.363858f },
    { 0.984258f, 0.106096f },
    { -0.003313f, 0.345535f },
    { -0.320351f, -0.573936f },
    { 0.063455f, -0.003239f },
    { -0.570173f, -0.759313f },
    { 0.106456f, 0.283726f },
    { -0.668163f, 0.142388f },
    { -0.501119f, -0.720006f },
    { -0.253281f, 0.524032f },
    { -0.064084f, -0.165943f },
    { -0.194672f, 0.43355f },
    { -0.2818f, -0.417744f },
    { 0.045786f, 0.402986f },
    { 0.105064f, -0.558937f },
    { 0.312244f, 0.688318f },
    { -0.263294f, -0.256811f },
    { 0.659186f, 0.070672f },
    { 0.093625f, -0.046812f },
    { -0.87502f, 0.288509f },
    { 0.329359f, 0.105941f },
    { -0.181309f, 0.259865f },
    { 0.261597f, -0.07407f },
    { -0.296082f, 0.031858f },
    { 0.038584f, 0.565947f },
    { -0.253445f, -0.717865f },
    { -0.211836f, 0.336521f },
    { 0.890123f, 0.00495f },
    { -0.979825f, -0.17079f },
    { 0.045346f, 0.02224f },
    { -0.345796f, 0.522712f },
    { 0.108525f, 0.165424f },
    { -0.572796f, -0.473399f },
    { 0.368605f, -0.865844f },
    { 0.075571f, -0.327703f },
    { -0.466353f, -0.565594f },
    { -0.358837f, 0.610302f },
    { 0.603884f, 0.440023f },
    { 0.002465f, -0.144449f },
    { -0.294915f, 0.79997f },
    { -0.028347f, -0.112071f },
    { -0.009472f, 0.686061f },
    { 0.07115f, 0.01991f },
    { 0.96269f, 0.024926f },
    { 0.309208f, 0.871549f },
    { -0.123782f, -0.312301f },
    { -0.433055f, -0.895981f },
    { 0.962495f, -0.263777f },
    { -0.51146f, -0.359478f },
    { -0.044013f, 0.02021f },
    { -0.10934f, -0.76123f },
    { 0.171003f, -0.107461f },
    { 0.418912f, 0.435294f },
    { 0.44494f, -0.139643f },
    { 0.518574f, 0.365965f },
    { -0.506997f, 0.655597f },
    { 0.510525f, 0.508961f },
    { -0.296173f, -0.675837f },
    { 0.851332f, 0.307192f },
    { -0.008474f, -0.188744f },
    { 0.552703f, 0.427086f },
    { 0.080334f, -0.002805f },
    { 0.035656f, 0.610991f },
    { 0.770593f, 0.398874f },
    { -0.522137f, 0.324362f },
    { 0.006045f, 0.042788f },
    { 0.482456f, 0.848994f },
    { 0.226058f, -0.522367f },
    { -0.674606f, -0.547814f },
    { -0.441998f, 0.59884f },
    { -0.183957f, -0.270234f },
    { 0.51885f, 0.634946f },
    { 0.430386f, 0.125257f },
    { -0.185496f, -0.264459f },
    { 0.02369f, 0.312978f },
    { -0.444287f, 0.849928f },
    { 0.291978f, -0.897679f },
    { -0.045826f, -0.047128f },
    { -0.114246f, 0.511975f },
    { 0.738133f, 0.607667f },
    { -0.786889f, -0.384057f },
    { 0.182993f, 0.265086f },
    { -0.39945f, -0.309031f },
    { -0.482895f, 0.265662f },
    { 0.059671f, 0.09776f },
    { 0.793174f, -0.015972f },
    { 0.201658f, 0.492445f },
    { -0.707371f, -0.02619f },
    { -0.320882f, 0.37228f },
    { 0.572813f, -0.537255f },
    { 0.337619f, 0.116293f },
    { -0.606537f, 0.173373f },
    { -0.166593f, -0.335112f },
    { -0.583993f, 0.182916f },
    { -0.573519f, -0.623348f },
    { -0.392707f, 0.449474f },
    { 0.151474f, 0.840401f },
    { -0.563551f, -0.00443f },
    { -0.282062f, -0.757933f },
    { 0.413047f, 0.244246f },
};
/* Source: uo_cgame_mp_x86.dll 0x30071f30..0x30071f57 (.rdata); refs=1;
 * first=0x3003c0f3. Ten consecutive 1.0f values restore every alias-channel
 * volume through trap 220 / MSS_FadeSelectSounds. The following address
 * 0x30071f58 begins vec3_origin and is not part of this table. */
const float cg_soundChannelFullVolumes[SND_ALIAS_CHANNEL_COUNT] = {
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f
};
/* Source: uo_cgame_mp_x86.dll 0x30071f58 (.rdata); refs=32; first=0x300057b2.
 * A shared read-only zero vec3 {0,0,0}. The .rdata bytes are three consecutive
 * 0x00000000 dwords (0x30071f58/5c/60), NOT 1.0f: objdump -s -j .rdata around
 * this address shows 0x30071f50/54 = 00 00 80 3f (the two genuine 1.0f dwords,
 * the tail of the run at 0x30071f30..) and 0x30071f58.. = 00 00 00 00 x3.
 *
 * All 32 references only TAKE ITS ADDRESS (PUSH/MOV imm 0x30071f58) and pass it
 * where a const vec3_t* / const float* is expected; no site ever does
 * `flds 0x30071f58` (there are zero scalar loads of the value in the binary), so
 * it is never read as a 1.0f scalar. Two proofs it is a 3-float vector, not a
 * scalar:
 *   - The callee at 0x3001fbb0 (reached from the wheel-effects/DObj sites, e.g.
 *     0x30020b7e) copies THREE consecutive dwords out of the pointer:
 *       mov 0x0(%ebp),%ecx -> 0x14(%esi)   ; x
 *       mov 0x4(%ebp),%edx -> 0x18(%esi)   ; y
 *       mov 0x8(%ebp),%eax -> 0x1c(%esi)   ; z
 *     i.e. it treats 0x30071f58 as a source vec3 = {0,0,0}.
 *   - CG_Vehicle_DoControllers (FUN_30020540) passes &0x30071f58 as a vec3
 *     origin {0,0,0}; the config-string-3 handler at 0x3002c9db and the
 *     shellshock path at 0x3003c14f pass it as the const-pointer arg to cgame
 *     traps 0xc4/0xc6 (a "no offset / at origin" vector).
 *
 * This is the canonical Quake vec3_origin, now defined by src/math. The
 * mechanical export had truncated the run to a single 0x00000000 u32 and
 * mislabeled it; the three-dword read pattern above proves the shared type. */
/* Source: uo_cgame_mp_x86.dll 0x30071f74..0x30071f83 (.rdata). */
const vec4_t bg_proneColorRed = { 1.0f, 0.0f, 0.0f, 1.0f };
/* Source: uo_cgame_mp_x86.dll 0x30071f84..0x30071f93 (.rdata). */
const vec4_t bg_proneColorGreen = { 0.0f, 1.0f, 0.0f, 1.0f };
/* Source: uo_cgame_mp_x86.dll 0x30071fb4..0x30071fc3 (.rdata). */
const vec4_t bg_proneColorYellow = { 1.0f, 1.0f, 0.0f, 1.0f };
/* Source: uo_cgame_mp_x86.dll 0x30071fe4..0x30071ff3 (.rdata). */
const vec4_t bg_proneColorMagenta = { 1.0f, 0.0f, 1.0f, 1.0f };
/* Source: uo_cgame_mp_x86.dll 0x30071ff4..0x30072003 (.rdata). */
const vec4_t bg_proneColorCyan = { 0.0f, 1.0f, 1.0f, 1.0f };
/* Source: uo_cgame_mp_x86.dll 0x30072014..0x30072023 (.rdata). Client-specific
 * medium cyan: raw component bits are 0, 0x3f000000, 0x3f000000, 0x3f800000. */
const vec4_t bg_proneColorMediumCyan = { 0.0f, 0.5f, 0.5f, 1.0f };
/* Source: uo_cgame_mp_x86.dll 0x30072034 (.rdata); refs=7 width=imm; first=0x3000ccf6; owner=veh_findvaliddismountspot.
 * Example: 3000ccf6   68 34 20 07 30               PUSH 0x30072034 | 3000cd71   68 34 20 07 30               PUSH 0x30072034 | 30017ebb   68 34 20 07 30               PUSH 0x30072034
 */
const vec4_t cg_colorWhite = { 1.0f, 1.0f, 1.0f, 1.0f };
/* Source: uo_cgame_mp_x86.dll 0x3007285c (.rdata); refs=4 width=imm; first=0x300016e5; owner=bg_calculateweaponposition_sway.
 * Example: 300016e5   68 5c 28 07 30               PUSH 0x3007285c | 30005b55   68 5c 28 07 30               PUSH 0x3007285c | 3001e989   68 5c 28 07 30               PUSH 0x3007285c
 */
const char bg_rootAnimationName[5] = "root";
/* Source: uo_cgame_mp_x86.dll 0x30072a40 (.rdata); refs=1 width=imm; first=0x30001a50; owner=cmd_give_f.
 * Example: 30001a50   b9 40 2a 07 30               MOV ECX,0x30072a40
 */
const char bg_conditionNotKeyword[4] = "NOT";
/* Source: uo_cgame_mp_x86.dll 0x30072b30 (.rdata); refs=1 width=imm; first=0x3001e973; owner=objectivestateindexfromstring.
 * Example: 3001e973   68 30 2b 07 30               PUSH 0x30072b30
 */
const char bg_mg42WeaponName[5] = "MG42";
/* Source: uo_cgame_mp_x86.dll 0x30072d10 (.rdata); refs=2 width=4; first=0x300081c2; owner=gscr_getnumparts.
 * Example: 300081c2   c7 44 24 18 10 2d 07 30      MOV dword ptr [ESP + 0x18],0x30072d10 | 300407e7   b8 10 2d 07 30               MOV EAX,0x30072d10
 */
const char bg_passenger4TagName[15] = "tag_passenger4";
/* Source: uo_cgame_mp_x86.dll 0x30072d20 (.rdata); refs=2 width=4; first=0x300081ba; owner=gscr_getnumparts.
 * Example: 300081ba   c7 44 24 14 20 2d 07 30      MOV dword ptr [ESP + 0x14],0x30072d20 | 300407e1   b8 20 2d 07 30               MOV EAX,0x30072d20
 */
const char bg_passenger3TagName[15] = "tag_passenger3";
/* Source: uo_cgame_mp_x86.dll 0x30072d30 (.rdata); refs=2 width=4; first=0x300081b2; owner=gscr_getnumparts.
 * Example: 300081b2   c7 44 24 10 30 2d 07 30      MOV dword ptr [ESP + 0x10],0x30072d30 | 300407db   b8 30 2d 07 30               MOV EAX,0x30072d30
 */
const char bg_passenger2TagName[15] = "tag_passenger2";
/* Source: uo_cgame_mp_x86.dll 0x30072d40 (.rdata); refs=2 width=4; first=0x300081aa; owner=gscr_getnumparts.
 * Example: 300081aa   c7 44 24 0c 40 2d 07 30      MOV dword ptr [ESP + 0xc],0x30072d40 | 300407d5   b8 40 2d 07 30               MOV EAX,0x30072d40
 */
const char bg_passengerTagName[14] = "tag_passenger";
/* Source: uo_cgame_mp_x86.dll 0x30072d50 (.rdata); refs=2 width=4; first=0x300081a2; owner=gscr_getnumparts.
 * Example: 300081a2   c7 44 24 08 50 2d 07 30      MOV dword ptr [ESP + 0x8],0x30072d50 | 30020af6   68 50 2d 07 30               PUSH 0x30072d50
 */
const char bg_secondaryPlayerTagName[21] = "tag_secondary_player";
/* Source: uo_cgame_mp_x86.dll 0x30072d68 (.rdata); refs=4 width=4; first=0x3000819a; owner=gscr_getnumparts.
 * Example: 3000819a   c7 44 24 04 68 2d 07 30      MOV dword ptr [ESP + 0x4],0x30072d68 | 300405f1   b8 68 2d 07 30               MOV EAX,0x30072d68 | 300415c0   b8 68 2d 07 30               MOV EAX,0x30072d68
 */
const char bg_playerTagName[11] = "tag_player";
/* Source: uo_cgame_mp_x86.dll 0x30072d74 (.rdata); refs=1 width=4; first=0x30008193; owner=gscr_getnumparts.
 * Example: 30008193   c7 04 24 74 2d 07 30         MOV dword ptr [ESP],0x30072d74
 */
const char bg_unusedBoneName[9] = "*unused*";
/* Source: uo_cgame_mp_x86.dll 0x30072ddc (.rdata); NUL-terminated string; first=0x30005eff.
 * Repaired from the mechanical first-dword uint32 g_const_u32_5f474215_30072ddc: the
 * datum is the "\x15BG_CanItemBeGrabbed: IT_BAD" Com_Error format taken by address at
 * 30005eff (PUSH 0x30072ddc). Leading '\x15' is the CoD error-channel marker byte.
 */
const char bg_canItemBeGrabbedInvalidItemErrorMessage[29] =
    "\x15" "BG_CanItemBeGrabbed: IT_BAD";
/* Source: uo_cgame_mp_x86.dll 0x30072dfc (.rdata); NUL-terminated string; first=0x30005e18.
 * Repaired from the mechanical first-dword uint32 g_const_u32_5f474215_30072dfc: the
 * datum is the "\x15BG_CanItemBeGrabbed: index out of range" Com_Error format taken by
 * address at 30005e18 (PUSH 0x30072dfc).
 */
const char bg_canItemBeGrabbedIndexOutOfRangeErrorMessage[41] =
    "\x15" "BG_CanItemBeGrabbed: index out of range";
/* Source: uo_cgame_mp_x86.dll 0x30074a0c (.rdata); refs=60 width=1..4; first=0x3000fe23.
 * Repaired: the mechanical export truncated a NUL-terminated .rdata string to its
 * first dword (0x00000000) as a uint32_t. Machine code proves it is the shared
 * empty-string literal "": call sites take its address as a cvar-default value
 * (e.g. CG_CloseScriptMenu 0x3003a950 passes it to trap_Cvar_Set) and load its
 * first byte as the NUL terminator (MOV AL,[0x30074a0c]). */
const char g_str_empty[1] = "";
/* Source: uo_cgame_mp_x86.dll 0x30074e7c (.rdata); refs=1 width=imm; first=0x30011129.
 * Mechanical export captured only the first dword (0x6c756f43 = "Coul"); the real
 * datum is a NUL-terminated format string. Full bytes (objdump -s -j .rdata):
 * 436f756c 646e2774 2066696e 64207765 61706f6e 20222573 220a00.
 * Consumed by BG_GetWeaponIndexForName (0x300110f0) as a Com_DPrintf format string.
 * Example: 30011129   68 7c 4e 07 30               PUSH 0x30074e7c
 */
const char g_str_couldnt_find_weapon[27] = "Couldn't find weapon \"%s\"\n";
/* Source: uo_cgame_mp_x86.dll 0x300769e0 (.rdata); refs=19 width=imm; first=0x30017b4a; owner=script_method_player_setfatigue.
 * Example: 30017b4a   68 e0 69 07 30               PUSH 0x300769e0 | 30017c6c   68 e0 69 07 30               PUSH 0x300769e0 | 30022697   68 e0 69 07 30               PUSH 0x300769e0
 */
/* Source: uo_cgame_mp_x86.dll 0x30076bf8 (.rdata); refs=1 width=imm; first=0x3001ae33; owner=info_setvalueforkey.
 * Example: 3001ae33   68 f8 6b 07 30               PUSH 0x30076bf8
 */
const char cg_soundChannelDebugFormat[65] = "%2i %-50s vol:^3%04.2f ^7rvol:^3%04.2f ^7pit:^3%04.2f ^7hz:^3%5i";
/* Source: uo_cgame_mp_x86.dll 0x30076c3c (.rdata); refs=4 width=imm; first=0x3001ae08; owner=info_setvalueforkey.
 * Example: 3001ae08   68 3c 6c 07 30               PUSH 0x30076c3c | 30030d86   68 3c 6c 07 30               PUSH 0x30076c3c | 30031b70   68 3c 6c 07 30               PUSH 0x30076c3c
 */
const char cg_soundChannelIndexFormat[4] = "%2i";
/* Source: uo_cgame_mp_x86.dll 0x30076c40 (.rdata); refs=1 width=imm; first=0x3001adb4; owner=info_setvalueforkey.
 * Example: 3001adb4   68 40 6c 07 30               PUSH 0x30076c40
 */
const char cg_soundDebugHeaderFormat[70] = "CPU: ^3%%%i ^73D provider: ^3%s ^7bits: ^3%i ^7kHz: ^3%i ^7chan: ^3%i";
/* Source: uo_cgame_mp_x86.dll 0x30076c88 (.rdata); refs=1 width=imm; first=0x3001ad7d; owner=info_setvalueforkey.
 * Example: 3001ad7d   68 88 6c 07 30               PUSH 0x30076c88
 */
const char mss_stereoCvarName[11] = "mss_stereo";
/* Source: uo_cgame_mp_x86.dll 0x30076c94 (.rdata); refs=1 width=imm; first=0x3001ad56; owner=info_setvalueforkey.
 * Example: 3001ad56   68 94 6c 07 30               PUSH 0x30076c94
 */
const char mss_khzCvarName[8] = "mss_khz";
/* Source: uo_cgame_mp_x86.dll 0x30076c9c (.rdata); refs=1 width=imm; first=0x3001ad2d; owner=info_setvalueforkey.
 * Example: 3001ad2d   68 9c 6c 07 30               PUSH 0x30076c9c
 */
const char mss_bitsCvarName[9] = "mss_bits";
/* Source: uo_cgame_mp_x86.dll 0x30076ca8 (.rdata); refs=1 width=imm; first=0x3001ad13; owner=info_setvalueforkey.
 * Example: 3001ad13   68 a8 6c 07 30               PUSH 0x30076ca8
 */
const char mss_3dProviderCvarName[16] = "mss_3d_provider";
/* Source: uo_cgame_mp_x86.dll 0x30076cb8 (.rdata); refs=1 width=imm; first=0x3001ab34; owner=pm_weapon_finishweaponbreakdown.
 * Example: 3001ab34   68 b8 6c 07 30               PUSH 0x30076cb8
 */
const char g_str_cl_conXOffset[14] = "cl_conXOffset";
/* Source: uo_cgame_mp_x86.dll 0x30076cc8 (.rdata); refs=27 width=imm; first=0x3001ab2f; owner=pm_weapon_finishweaponbreakdown.
 * Example: 3001ab2f   68 c8 6c 07 30               PUSH 0x30076cc8 | 3001c197   68 c8 6c 07 30               PUSH 0x30076cc8 | 30022b8b   68 c8 6c 07 30               PUSH 0x30076cc8
 */
const char g_str_zero[2] = "0";
/* Source: uo_cgame_mp_x86.dll 0x30076ccc (.rdata); refs=1 width=imm; first=0x3001ab1a; owner=pm_weapon_finishweaponbreakdown.
 * Example: 3001ab1a   bb cc 6c 07 30               MOV EBX,0x30076ccc
 */
const char g_str_voiceMenu[10] = "voiceMenu";
/* Source: uo_cgame_mp_x86.dll 0x30076cec (.rdata); refs=1 width=imm; first=0x30019005; owner=cmd_veh_fireturret.
 * Example: 30019005   68 ec 6c 07 30               PUSH 0x30076cec
 */
const char cg_lagometerSyncLabel[4] = "snc";
/* Source: uo_cgame_mp_x86.dll 0x30076cf0 (.rdata); refs=2 width=imm; first=0x30018b93; owner=script_method_player_setreverb.
 * Example: 30018b93   68 f0 6c 07 30               PUSH 0x30076cf0 | 3002baf2   68 f0 6c 07 30               PUSH 0x30076cf0
 */
const char cg_connectionInterruptedIconPath[15] = "gfx/2d/net.tga";
/* Source: uo_cgame_mp_x86.dll 0x30076d00 (.rdata); refs=1 width=imm; first=0x30018af6; owner=script_method_player_setreverb.
 * Example: 30018af6   b9 00 6d 07 30               MOV ECX,0x30076d00
 */
const char cg_connectionInterruptedLocalizationKey[28] = "CGAME_CONNECTIONINTERRUPTED";
/* Source: uo_cgame_mp_x86.dll 0x30076d88 (.rdata); refs=1 width=imm; first=0x30017f9e; owner=g_checkpointinsidetriggermount.
 * Example: 30017f9e   68 88 6d 07 30               PUSH 0x30076d88
 */
const char cg_scriptStringUsageDebugFormat[17] = "string usage: %d";
/* Source: uo_cgame_mp_x86.dll 0x30076d9c (.rdata); refs=1 width=imm; first=0x30017f1d; owner=g_checkpointinsidetriggermount.
 * Example: 30017f1d   68 9c 6d 07 30               PUSH 0x30076d9c
 */
const char cg_scriptNumThreadsDebugFormat[16] = "num threads: %d";
/* Source: uo_cgame_mp_x86.dll 0x30076dac (.rdata); refs=1 width=imm; first=0x30017e9f; owner=g_checkpointinsidetriggermount.
 * Example: 30017e9f   68 ac 6d 07 30               PUSH 0x30076dac
 */
const char cg_scriptNumVarsDebugFormat[16] = "num vars:    %d";
/* Source: uo_cgame_mp_x86.dll 0x30077138 (.rdata); refs=1; first=0x30022217.
 * Example: 30022217   68 38 71 07 30               PUSH 0x30077138
 * NUL-terminated .rdata string, not a dword: the mechanical export truncated it
 * to its first dword ("Bad " = 0x20646142). Restored to the real string, proven
 * by its consumer CG_AddCEntity (0x30022170) which pushes it as the
 * Com_ErrorMessage("Bad entity type: %i\n", eType) format for an out-of-range
 * currentState.eType. Bytes verified: 42 61 64 20 65 6e 74 69 74 79 20 74 79 70
 * 65 3a 20 25 69 0a 00 ("Bad entity type: %i\n"). */
const char cg_badEntityTypeErrorFormat[21] = "Bad entity type: %i\n";
/* Source: uo_cgame_mp_x86.dll 0x300771a4 (.rdata); refs=1 width=imm; first=0x30020c07; owner=bg_animparseanimscript.
 * Example: 30020c07   68 a4 71 07 30               PUSH 0x300771a4
 */
const char cg_vehicleSteeringWheelTagName[18] = "tag_steeringwheel";
/* Source: uo_cgame_mp_x86.dll 0x300771b8 (.rdata); refs=3 width=imm; first=0x30020ace; owner=bg_animparseanimscript.
 * Example: 30020ace   68 b8 71 07 30               PUSH 0x300771b8 | 30020b3f   68 b8 71 07 30               PUSH 0x300771b8 | 300407cf   b8 b8 71 07 30               MOV EAX,0x300771b8
 */
const char cg_vehicleSecondaryGunTagName[18] = "tag_secondary_gun";
/* Source: uo_cgame_mp_x86.dll 0x300771cc (.rdata); refs=1 width=imm; first=0x30020a85; owner=bg_animparseanimscript.
 * Example: 30020a85   68 cc 71 07 30               PUSH 0x300771cc
 */
const char cg_vehicleSecondaryBaseTagName[19] = "tag_secondary_base";
/* Source: uo_cgame_mp_x86.dll 0x300771e0 (.rdata); refs=2 width=imm; first=0x30020777; owner=bg_animparseanimscript.
 * Example: 30020777   68 e0 71 07 30               PUSH 0x300771e0 | 3004130f   b8 e0 71 07 30               MOV EAX,0x300771e0
 */
const char cg_vehicleBarrelTagName[11] = "tag_barrel";
/* Source: uo_cgame_mp_x86.dll 0x300771ec (.rdata); refs=3 width=imm; first=0x3002072e; owner=bg_animparseanimscript.
 * Example: 3002072e   68 ec 71 07 30               PUSH 0x300771ec | 30031e93   b8 ec 71 07 30               MOV EAX,0x300771ec | 300407ed   b8 ec 71 07 30               MOV EAX,0x300771ec
 */
const char cg_vehicleTurretTagName[11] = "tag_turret";
/* Source: uo_cgame_mp_x86.dll 0x300771f8 (.rdata); refs=2 width=imm; first=0x300206a3; owner=bg_animparseanimscript.
 * Example: 300206a3   68 f8 71 07 30               PUSH 0x300771f8 | 30040d39   b8 f8 71 07 30               MOV EAX,0x300771f8
 */
const char cg_vehicleBodyTagName[9] = "tag_body";
/* Source: uo_cgame_mp_x86.dll 0x300772c0 (.rdata); refs=7 width=imm; first=0x3001ebc6; owner=fire_artillery.
 * Example: 3001ebc6   68 c0 72 07 30               PUSH 0x300772c0 | 3003465f   b8 c0 72 07 30               MOV EAX,0x300772c0 | 3004625b   68 c0 72 07 30               PUSH 0x300772c0
 */
const char cg_muzzleFlashTagName[10] = "tag_flash";
/* Source: uo_cgame_mp_x86.dll 0x300772cc (.rdata); refs=1 width=imm; first=0x3001eb68; owner=fire_artillery.
 * Example: 3001eb68   68 cc 72 07 30               PUSH 0x300772cc
 */
const char cg_animatedAimTagName[17] = "tag_aim_animated";
/* Source: uo_cgame_mp_x86.dll 0x300772e0 (.rdata); refs=2 width=imm; first=0x3001eb1c; owner=fire_artillery.
 * Example: 3001eb1c   68 e0 72 07 30               PUSH 0x300772e0 | 300341f5   b8 e0 72 07 30               MOV EAX,0x300772e0
 */
const char cg_aimTagName[8] = "tag_aim";
/* Source: uo_cgame_mp_x86.dll 0x30077398 (.rdata); refs=11 width=imm; first=0x30022bc6; owner=cmd_callvote_f.
 * Example: 30022bc6   68 98 73 07 30               PUSH 0x30077398 | 30027d4f   68 98 73 07 30               PUSH 0x30077398 | 30027d6b   68 98 73 07 30               PUSH 0x30077398
 */
const char cvarEnabledValue[2] = "1";
/* Source: uo_cgame_mp_x86.dll 0x30077418 (.rdata); refs=6 width=imm; first=0x300226a8; owner=vectordistance2d.
 * Example: 300226a8   68 18 74 07 30               PUSH 0x30077418 | 30034e7b   68 18 74 07 30               PUSH 0x30077418 | 3004268d   68 18 74 07 30               PUSH 0x30077418
 */
const char cg_weaponSelectCvarName[16] = "cg_weaponSelect";
/* Source: uo_cgame_mp_x86.dll 0x30077594 (.rdata); refs=1 width=imm; first=0x30027d70; owner=cg_fireflamechunks.
 * Example: 30027d70   68 94 75 07 30               PUSH 0x30077594
 */
const char r_overbrightBitsCvarName[17] = "r_overbrightbits";
/* Source: uo_cgame_mp_x86.dll 0x300775a8 (.rdata); refs=1 width=imm; first=0x30027d54; owner=cg_fireflamechunks.
 * Example: 30027d54   68 a8 75 07 30               PUSH 0x300775a8
 */
const char r_fullscreenCvarName[13] = "r_fullscreen";
/* The word/byte loads at 0x3002a05e/0x3002a064 copy the tail of this same
 * seven-byte string; they are not separate constants. */
/* Source: uo_cgame_mp_x86.dll 0x30077828 (.rdata); refs=6 width=imm; first=0x3002a8c9; owner=pm_laddermove.
 * Example: 3002a8c9   68 28 78 07 30               PUSH 0x30077828 | 30043ae4   68 28 78 07 30               PUSH 0x30077828 | 30043b2b   68 28 78 07 30               PUSH 0x30077828
 */
const char com_pathWithExtensionFormat[5] = "%s%s";
/* Source: uo_cgame_mp_x86.dll 0x30077880 (.rdata); refs=16 width=imm; first=0x3001c1b8; owner=g_getnonpvsfriendlyinfo.
 * Example: 3001c1b8   bb 80 78 07 30               MOV EBX,0x30077880 | 3002a665   bb 80 78 07 30               MOV EBX,0x30077880 | 3002be58   bb 80 78 07 30               MOV EBX,0x30077880
 */
const char mapNameInfoKey[8] = "mapname";
/* Source: uo_cgame_mp_x86.dll 0x30077888 (.rdata); refs=4 width=imm; first=0x3002a5e0; owner=pm_laddermove.
 * Example: 3002a5e0   68 88 78 07 30               PUSH 0x30077888 | 3002a5fc   68 88 78 07 30               PUSH 0x30077888 | 30039624   68 88 78 07 30               PUSH 0x30077888
 */
const char ui_scriptMenuAllowResponseCvarName[27] = "ui_scriptMenuAllowResponse";
/* Source: uo_cgame_mp_x86.dll 0x300778a4 (.rdata); refs=17 width=imm; first=0x3001c19c; owner=g_getnonpvsfriendlyinfo.
 * Example: 3001c19c   68 a4 78 07 30               PUSH 0x300778a4 | 3002a5c0   68 a4 78 07 30               PUSH 0x300778a4 | 3002a623   68 a4 78 07 30               PUSH 0x300778a4
 */
const char cl_serverLoadWaitingCvarName[21] = "cl_serverloadwaiting";
/* Source: uo_cgame_mp_x86.dll 0x30077908 (.rdata); refs=5 width=imm; first=0x3002aa87; owner=pm_weaponuseammo.
 * Example: 3002aa87   68 08 79 07 30               PUSH 0x30077908 | 3002acaa   68 08 79 07 30               PUSH 0x30077908 | 3002ad24   68 08 79 07 30               PUSH 0x30077908
 */
const char cg_freeLocalEntityInactiveErrorMessage[31] = "CG_FreeLocalEntity: not active";
/* Source: uo_cgame_mp_x86.dll 0x300779b4 (.rdata); refs=8 width=imm; first=0x3002df96; owner=g_runframe.
 * Example: 3002df96   68 b4 79 07 30               PUSH 0x300779b4 | 300362a9   68 b4 79 07 30               PUSH 0x300779b4 | 30036ccd   68 b4 79 07 30               PUSH 0x300779b4
 */
const char cg_whiteMaterialName[6] = "white";
/* Source: uo_cgame_mp_x86.dll 0x300779d8 (.rdata); refs=1 width=imm; first=0x3002ddd7; owner=playercmd_takeweapon.
 * Example: 3002ddd7   68 d8 79 07 30               PUSH 0x300779d8
 */
const char cg_uiSliderThumbMaterialPath[27] = "ui/assets/sliderbutt_1.tga";
/* Source: uo_cgame_mp_x86.dll 0x300779f4 (.rdata); refs=1 width=imm; first=0x3002ddb9; owner=playercmd_takeweapon.
 * Example: 3002ddb9   68 f4 79 07 30               PUSH 0x300779f4
 */
const char cg_uiSliderTrackMaterialPath[22] = "ui/assets/slider2.tga";
/* Source: uo_cgame_mp_x86.dll 0x30077a0c (.rdata); refs=1 width=imm; first=0x3002dd9e; owner=playercmd_takeweapon.
 * Example: 3002dd9e   68 0c 7a 07 30               PUSH 0x30077a0c
 */
const char cg_uiScrollThumbMaterialPath[30] = "ui/assets/scrollbar_thumb.tga";
/* Source: uo_cgame_mp_x86.dll 0x30077a2c (.rdata); refs=1 width=imm; first=0x3002dd83; owner=playercmd_takeweapon.
 * Example: 3002dd83   68 2c 7a 07 30               PUSH 0x30077a2c
 */
const char cg_uiScrollRightArrowMaterialPath[36] = "ui/assets/scrollbar_arrow_right.tga";
/* Source: uo_cgame_mp_x86.dll 0x30077a50 (.rdata); refs=1 width=imm; first=0x3002dd68; owner=playercmd_takeweapon.
 * Example: 3002dd68   68 50 7a 07 30               PUSH 0x30077a50
 */
const char cg_uiScrollLeftArrowMaterialPath[35] = "ui/assets/scrollbar_arrow_left.tga";
/* Source: uo_cgame_mp_x86.dll 0x30077a74 (.rdata); refs=1 width=imm; first=0x3002dd4a; owner=playercmd_takeweapon.
 * Example: 3002dd4a   68 74 7a 07 30               PUSH 0x30077a74
 */
const char cg_uiScrollUpArrowMaterialPath[35] = "ui/assets/scrollbar_arrow_up_a.tga";
/* Source: uo_cgame_mp_x86.dll 0x30077a98 (.rdata); refs=1 width=imm; first=0x3002dd2f; owner=playercmd_takeweapon.
 * Example: 3002dd2f   68 98 7a 07 30               PUSH 0x30077a98
 */
const char cg_uiScrollDownArrowMaterialPath[36] = "ui/assets/scrollbar_arrow_dwn_a.tga";
/* Source: uo_cgame_mp_x86.dll 0x30077abc (.rdata); refs=1 width=imm; first=0x3002dd14; owner=playercmd_takeweapon.
 * Example: 3002dd14   68 bc 7a 07 30               PUSH 0x30077abc
 */
const char cg_uiScrollBarMaterialPath[24] = "ui/assets/scrollbar.tga";
/* Source: uo_cgame_mp_x86.dll 0x30077ad4 (.rdata); refs=1 width=imm; first=0x3002dcf9; owner=playercmd_takeweapon.
 * Example: 3002dcf9   68 d4 7a 07 30               PUSH 0x30077ad4
 */
const char cg_uiGradientBarMaterialPath[27] = "ui/assets/gradientbar2.tga";
/* Source: uo_cgame_mp_x86.dll 0x30077b18 (.rdata); refs=2 width=imm; first=0x30029cc2; owner=cg_updateshellshocksound.
 * Example: 30029cc2   68 18 7b 07 30               PUSH 0x30077b18 | 3002d837   68 18 7b 07 30               PUSH 0x30077b18
 */
const char cg_hudElemLocalizationContext[15] = "hudelem string";
/* Source: uo_cgame_mp_x86.dll 0x30077b28 (.rdata); refs=33 width=imm; first=0x300177bd; owner=pm_interuptweaponwithsprintmove.
 * Example: 300177bd   b8 28 7b 07 30               MOV EAX,0x30077b28 | 30018af1   b8 28 7b 07 30               MOV EAX,0x30077b28 | 3001b724   b8 28 7b 07 30               MOV EAX,0x30077b28
 */
const char cg_localizationContext[6] = "cgame";
/* The interior dword and trailing-byte reads are the compiler's unrolled copies
 * of this string, not float constants. */
/* Source: uo_cgame_mp_x86.dll 0x30077d90 (.rdata); refs=28 width=imm; first=0x3001e85f; owner=bg_takeplayerweapon.
 * Example: 3001e85f   68 90 7d 07 30               PUSH 0x30077d90 | 3001e882   68 90 7d 07 30               PUSH 0x30077d90 | 300218b3   68 90 7d 07 30               PUSH 0x30077d90
 */
const char cg_configStringBadIndexFmt[31] = "CG_ConfigString: bad index: %i";
/* Source: uo_cgame_mp_x86.dll 0x30078854 (.rdata); refs=5 width=imm; first=0x3002b21d; owner=cg_playsoundaliasbyname.
 * Example: 3002b21d   68 54 88 07 30               PUSH 0x30078854 | 30034e9f   68 54 88 07 30               PUSH 0x30078854 | 30039606   68 54 88 07 30               PUSH 0x30078854
 */
const char cl_runCvarName[7] = "cl_run";
/* Source: uo_cgame_mp_x86.dll 0x30078f2c (.rdata); refs=6 width=imm; first=0x3003a6ff; owner=bg_calculateweaponposition_gunreco.
 * Example: 3003a6ff   68 2c 8f 07 30               PUSH 0x30078f2c | 3003a8d7   68 2c 8f 07 30               PUSH 0x30078f2c | 3003a91e   68 2c 8f 07 30               PUSH 0x30078f2c
 */
const char g_str_minus_one[3] = "-1";
/* Source: uo_cgame_mp_x86.dll 0x30078f70 (.rdata); refs=1 width=imm; first=0x300383b0; owner=pm_weapon_addfiringaimspreadscale.
 * Example: 300383b0   bb 70 8f 07 30               MOV EBX,0x30078f70
 */
const char g_gametypeInfoKey[11] = "g_gametype";
/* Source: uo_cgame_mp_x86.dll 0x30079058 (.rdata); refs=3 width=imm; first=0x3002b206; owner=cg_playsoundaliasbyname.
 * Example: 3002b206   68 58 90 07 30               PUSH 0x30079058 | 30034e8d   68 58 90 07 30               PUSH 0x30079058 | 300395f4   68 58 90 07 30               PUSH 0x30079058
 */
const char cl_stanceCvarName[10] = "cl_stance";
/* Source: uo_cgame_mp_x86.dll 0x300795f0 (.rdata); refs=1 width=imm; first=0x30057f99; owner=veh_updatepath.
 * Example: 30057f99   68 f0 95 07 30               PUSH 0x300795f0
 */
const char cg_hudAlphaCvarName[12] = "cg_hudAlpha";
/* Source: uo_cgame_mp_x86.dll 0x30079760 (.rdata); refs=2 width=imm; first=0x30031538; owner=menuparse_itemdef.
 * Example: 30031538   68 60 97 07 30               PUSH 0x30079760 | 30031608   68 60 97 07 30               PUSH 0x30079760
 */
const char cg_hudStatUnsetText[2] = "-";
/* Source: uo_cgame_mp_x86.dll 0x300797e0 (.rdata); refs=2 width=imm; first=0x30030e29; owner=spectatorthink.
 * Example: 30030e29   68 e0 97 07 30               PUSH 0x300797e0 | 30030e4a   68 e0 97 07 30               PUSH 0x300797e0
 * Renamed from g_string_text_300797e0: the HUD weapon-ammo-count clip/reserve
 * separator "|", consumed only by CG_DrawPlayerAmmoValue (FUN_30030c60_30030f10).
 */
const char cg_ammoCountSeparator[2] = "|";
/* Source: uo_cgame_mp_x86.dll 0x30079a88 (.rdata); refs=1 width=imm; first=0x300339f1; owner=pm_checkduck.
 * Example: 300339f1   68 88 9a 07 30               PUSH 0x30079a88
 */
const char cg_missingTurretTagWarning[] =
    "WARNING: aborting player positioning on turret since '%s' does not exist\n";
/* Source: uo_cgame_mp_x86.dll 0x30079ad4 (.rdata); refs=10 width=imm; first=0x30033116; owner=pm_checkduck.
 * Example: 30033116   68 d4 9a 07 30               PUSH 0x30079ad4 | 3003314c   68 d4 9a 07 30               PUSH 0x30079ad4 | 300333f0   68 d4 9a 07 30               PUSH 0x30079ad4
 */
const char cg_playerAnimNoChildrenError[] =
    "\x15Player anim '%s' has no children";
/* Source: uo_cgame_mp_x86.dll 0x30079bbc (.rdata); refs=2 width=imm; first=0x30037b17; owner=item_textfield_paint.
 * Example: 30037b17   68 bc 9b 07 30               PUSH 0x30079bbc | 30037f77   68 bc 9b 07 30               PUSH 0x30079bbc
 */
const char cg_scoreboardScrollDownKeyMaterialName[28] = "hudScoreboardScroll_DownKey";
/* Source: uo_cgame_mp_x86.dll 0x30079bd8 (.rdata); refs=2 width=imm; first=0x30037ae7; owner=item_textfield_paint.
 * Example: 30037ae7   68 d8 9b 07 30               PUSH 0x30079bd8 | 30037f61   68 d8 9b 07 30               PUSH 0x30079bd8
 */
const char cg_scoreboardScrollDownArrowMaterialName[30] = "hudScoreboardScroll_DownArrow";
/* Source: uo_cgame_mp_x86.dll 0x30079bf8 (.rdata); refs=2 width=imm; first=0x30037aa2; owner=item_textfield_paint.
 * Example: 30037aa2   68 f8 9b 07 30               PUSH 0x30079bf8 | 30037f48   68 f8 9b 07 30               PUSH 0x30079bf8
 */
const char cg_scoreboardScrollUpKeyMaterialName[26] = "hudScoreboardScroll_UpKey";
/* Source: uo_cgame_mp_x86.dll 0x30079c14 (.rdata); refs=2 width=imm; first=0x30037a72; owner=item_textfield_paint.
 * Example: 30037a72   68 14 9c 07 30               PUSH 0x30079c14 | 30037f32   68 14 9c 07 30               PUSH 0x30079c14
 */
const char cg_scoreboardScrollUpArrowMaterialName[28] = "hudScoreboardScroll_UpArrow";
/* Source: uo_cgame_mp_x86.dll 0x30079c30 (.rdata); refs=1 width=imm; first=0x3003719f; owner=g_damage.
 * Example: 3003719f   68 30 9c 07 30               PUSH 0x30079c30
 */
const char cg_scoreboardSpectatorsLocalizationKey[17] = "CGAME_SPECTATORS";
/* Source: uo_cgame_mp_x86.dll 0x30079c44 (.rdata); refs=2 width=imm; first=0x30037189; owner=g_damage.
 * Example: 30037189   68 44 9c 07 30               PUSH 0x30079c44 | 30037f8e   68 44 9c 07 30               PUSH 0x30079c44
 */
const char cg_scoreboardSpectatorsBannerCvarName[26] = "g_ScoresBanner_Spectators";
/* Source: uo_cgame_mp_x86.dll 0x30079c60 (.rdata); refs=1 width=imm; first=0x300371e0; owner=g_damage.
 * Example: 300371e0   68 60 9c 07 30               PUSH 0x30079c60
 */
const char cg_scoreboardAlliesTeamNameCvarName[18] = "g_TeamName_Allies";
/* Source: uo_cgame_mp_x86.dll 0x30079c74 (.rdata); refs=2 width=imm; first=0x300371c9; owner=g_damage.
 * Example: 300371c9   68 74 9c 07 30               PUSH 0x30079c74 | 30037feb   68 74 9c 07 30               PUSH 0x30079c74
 */
const char cg_scoreboardAlliesBannerCvarName[22] = "g_ScoresBanner_Allies";
/* Source: uo_cgame_mp_x86.dll 0x30079c8c (.rdata); refs=1 width=imm; first=0x300371b0; owner=g_damage.
 * Example: 300371b0   68 8c 9c 07 30               PUSH 0x30079c8c
 */
const char cg_scoreboardTeamNameFormat[8] = "%s (%s)";
/* Source: uo_cgame_mp_x86.dll 0x30079c94 (.rdata); refs=3 width=imm; first=0x3003719a; owner=g_damage.
 * Example: 3003719a   68 94 9c 07 30               PUSH 0x30079c94 | 300371f1   68 94 9c 07 30               PUSH 0x30079c94 | 3003722d   68 94 9c 07 30               PUSH 0x30079c94
 */
const char cg_scoreboardTeamNameLocalizationContext[21] = "scoreboard team name";
/* Source: uo_cgame_mp_x86.dll 0x30079cac (.rdata); refs=1 width=imm; first=0x3003721c; owner=g_damage.
 * Example: 3003721c   68 ac 9c 07 30               PUSH 0x30079cac
 */
const char cg_scoreboardAxisTeamNameCvarName[16] = "g_TeamName_Axis";
/* Source: uo_cgame_mp_x86.dll 0x30079cbc (.rdata); refs=2 width=imm; first=0x30037205; owner=g_damage.
 * Example: 30037205   68 bc 9c 07 30               PUSH 0x30079cbc | 30037fbe   68 bc 9c 07 30               PUSH 0x30079cbc
 */
const char cg_scoreboardAxisBannerCvarName[20] = "g_ScoresBanner_Axis";
/* Source: uo_cgame_mp_x86.dll 0x30079cd0 (.rdata); refs=2 width=imm; first=0x30037244; owner=g_damage.
 * Example: 30037244   68 d0 9c 07 30               PUSH 0x30079cd0 | 3003801b   68 d0 9c 07 30               PUSH 0x30079cd0
 */
const char cg_scoreboardNoneBannerCvarName[20] = "g_ScoresBanner_None";
/* Source: uo_cgame_mp_x86.dll 0x30079dc4 (.rdata); refs=6 width=imm; first=0x3003621c; owner=g_getactivateent.
 * Example: 3003621c   68 c4 9d 07 30               PUSH 0x30079dc4 | 30037523   68 c4 9d 07 30               PUSH 0x30079dc4 | 300378d6   68 c4 9d 07 30               PUSH 0x30079dc4
 */
const char cg_blackMaterialName[6] = "black";
/* Source: uo_cgame_mp_x86.dll 0x30079f90 (.rdata); refs=1 width=imm; first=0x3003a981; owner=bg_findanimtrees.
 * Example: 3003a981   68 90 9f 07 30               PUSH 0x30079f90
 */
const char g_str_ui_scriptMenuIndex[19] = "ui_scriptMenuIndex";
/* Source: uo_cgame_mp_x86.dll 0x30079fa4 (.rdata); refs=1 width=imm; first=0x3003a96f; owner=bg_findanimtrees.
 * Example: 3003a96f   68 a4 9f 07 30               PUSH 0x30079fa4
 */
const char g_str_ui_scriptMenu[14] = "ui_scriptMenu";
/* Source: uo_cgame_mp_x86.dll 0x30079fb4 (.rdata); refs=4 width=imm; first=0x3003a7b0; owner=bg_calculateweaponposition_gunreco.
 * Example: 3003a7b0   68 b4 9f 07 30               PUSH 0x30079fb4 | 3003a88c   68 b4 9f 07 30               PUSH 0x30079fb4 | 3003a8ee   68 b4 9f 07 30               PUSH 0x30079fb4
 */
const char g_str_ui_waitingScriptMenuNoMouse[28] = "ui_waitingScriptMenuNoMouse";
/* Source: uo_cgame_mp_x86.dll 0x30079fe0 (.rdata); refs=5 width=imm; first=0x3003a755; owner=bg_calculateweaponposition_gunreco.
 * Example: 3003a755   68 e0 9f 07 30               PUSH 0x30079fe0 | 3003a797   68 e0 9f 07 30               PUSH 0x30079fe0 | 3003a863   68 e0 9f 07 30               PUSH 0x30079fe0
 */
const char g_str_ui_waitingScriptMenuIndex[26] = "ui_waitingScriptMenuIndex";
/* Source: uo_cgame_mp_x86.dll 0x30079ffc (.rdata); refs=5 width=imm; first=0x3003a71b; owner=bg_calculateweaponposition_gunreco.
 * Example: 3003a71b   68 fc 9f 07 30               PUSH 0x30079ffc | 3003a77e   68 fc 9f 07 30               PUSH 0x30079ffc | 3003a82c   68 fc 9f 07 30               PUSH 0x30079ffc
 */
const char g_str_ui_waitingScriptMenu[21] = "ui_waitingScriptMenu";
/* Source: uo_cgame_mp_x86.dll 0x3007a014 (.rdata); refs=3 width=imm; first=0x3003a6d5; owner=bg_calculateweaponposition_gunreco.
 * Example: 3003a6d5   68 14 a0 07 30               PUSH 0x3007a014 | 3003a8b1   68 14 a0 07 30               PUSH 0x3007a014 | 3003a950   68 14 a0 07 30               PUSH 0x3007a014
 */
const char g_str_UIMENU_SCRIPT_POPUP[20] = "UIMENU_SCRIPT_POPUP";
/* Source: uo_cgame_mp_x86.dll 0x3007a028 (.rdata); refs=3 width=imm; first=0x3003a6ce; owner=bg_calculateweaponposition_gunreco.
 * Example: 3003a6ce   68 28 a0 07 30               PUSH 0x3007a028 | 3003a8aa   68 28 a0 07 30               PUSH 0x3007a028 | 3003a95d   68 28 a0 07 30               PUSH 0x3007a028
 */
const char g_str_UIMENU_SCRIPT_POPUP_NO_MOUSE[29] = "UIMENU_SCRIPT_POPUP_NO_MOUSE";
/* Source: uo_cgame_mp_x86.dll 0x3007a048 (.rdata); refs=5 width=imm; first=0x3003a6ba; owner=bg_calculateweaponposition_gunreco.
 * Example: 3003a6ba   68 48 a0 07 30               PUSH 0x3007a048 | 3003a704   68 48 a0 07 30               PUSH 0x3007a048 | 3003a875   68 48 a0 07 30               PUSH 0x3007a048
 */
const char g_str_ui_newScriptMenuIndex[22] = "ui_newScriptMenuIndex";
/* Source: uo_cgame_mp_x86.dll 0x3007a060 (.rdata); refs=5 width=imm; first=0x3003a6a1; owner=bg_calculateweaponposition_gunreco.
 * Example: 3003a6a1   68 60 a0 07 30               PUSH 0x3007a060 | 3003a6f2   68 60 a0 07 30               PUSH 0x3007a060 | 3003a84c   68 60 a0 07 30               PUSH 0x3007a060
 */
const char g_str_ui_newScriptMenu[17] = "ui_newScriptMenu";
/* Source: uo_cgame_mp_x86.dll 0x30079fd0..0x30079fdf (.rdata). */
const char g_str_cmd_mr_noop_fmt[16] = "cmd mr %s noop\n";
/* Source: uo_cgame_mp_x86.dll 0x3007a074..0x3007a0ab (.rdata). */
const char g_str_scriptMenuNotLoadedFmt[57] =
    "Server tried to open a non-loaded script menu index: %i\n";
/* Source: uo_cgame_mp_x86.dll 0x3007a0b0..0x3007a0bd (.rdata). */
const char g_str_cmd_mr_bad_fmt[15] = "cmd mr %i bad\n";
/* Source: uo_cgame_mp_x86.dll 0x3007a0c0..0x3007a0f0 (.rdata). */
const char g_str_scriptMenuBadIndexFmt[50] =
    "Server tried to open a bad script menu index: %i\n";
/* Source: uo_cgame_mp_x86.dll 0x3007a0f4 (.rdata); refs=1 width=imm; first=0x3003a56e; owner=bg_canitembegrabbed.
 * Example: 3003a56e   bf f4 a0 07 30               MOV EDI,0x3007a0f4
 */
const char cg_voiceChatPraiseCommandName[7] = "praise";
/* Source: uo_cgame_mp_x86.dll 0x3007a0fc (.rdata); refs=1 width=imm; first=0x3003a559; owner=bg_canitembegrabbed.
 * Example: 3003a559   bf fc a0 07 30               MOV EDI,0x3007a0fc
 */
const char cg_voiceChatGauntletKillCommandName[14] = "kill_gauntlet";
/* Source: uo_cgame_mp_x86.dll 0x3007a10c (.rdata); refs=1 width=imm; first=0x3003a544; owner=bg_canitembegrabbed.
 * Example: 3003a544   bf 0c a1 07 30               MOV EDI,0x3007a10c
 */
const char cg_voiceChatDeathInsultCommandName[13] = "death_insult";
/* Source: uo_cgame_mp_x86.dll 0x3007a11c (.rdata); refs=1 width=imm; first=0x3003a52f; owner=bg_canitembegrabbed.
 * Example: 3003a52f   bf 1c a1 07 30               MOV EDI,0x3007a11c
 */
const char cg_voiceChatTauntCommandName[6] = "taunt";
/* Source: uo_cgame_mp_x86.dll 0x3007a124 (.rdata); refs=1 width=imm; first=0x3003a51a; owner=bg_canitembegrabbed.
 * Example: 3003a51a   bf 24 a1 07 30               MOV EDI,0x3007a124
 */
const char cg_voiceChatKillInsultCommandName[12] = "kill_insult";
/* Source: uo_cgame_mp_x86.dll 0x3007a340 (.rdata); refs=2; first=0x300387d0.
 * "Could not load script menu file '%s'\n" — trap(CG_R_REGISTERMENU) load-failure
 * format used by CG_RegisterConfigStringMenu (0x30038790) and its sibling at
 * 0x30038826. owner=q_stricmp was a wrong size-guess first-touch label.
 */
const char cg_couldNotLoadScriptMenuFmt[38] = "Could not load script menu file '%s'\n";
/* Source: uo_cgame_mp_x86.dll 0x3007a368 (.rdata); refs=1 width=imm; first=0x30038408; owner=pm_weapon_addfiringaimspreadscale.
 * Example: 30038408   68 68 a3 07 30               PUSH 0x3007a368
 */
const char mpMapBspPathFormat[15] = "maps/mp/%s.bsp";
/* Source: uo_cgame_mp_x86.dll 0x3007a378 (.rdata); refs=1 width=imm; first=0x300383e4; owner=pm_weapon_addfiringaimspreadscale.
 * Example: 300383e4   bb 78 a3 07 30               MOV EBX,0x3007a378
 */
const char sv_maxClientsInfoKey[14] = "sv_maxclients";
/* Source: uo_cgame_mp_x86.dll 0x3007a388 (.rdata); refs=1 width=imm; first=0x30038394; owner=pm_weapon_addfiringaimspreadscale.
 * Example: 30038394   bb 88 a3 07 30               MOV EBX,0x3007a388
 */
const char sv_hostnameInfoKey[12] = "sv_hostname";
/* Source: uo_cgame_mp_x86.dll 0x3007a394 (.rdata); refs=1 width=imm; first=0x3003c4e5; owner=cg_getviewfov.
 * Example: 3003c4e5   68 94 a3 07 30               PUSH 0x3007a394
 */
const char cg_shellshockEndAliasName[15] = "shellshock_end";
/* Source: uo_cgame_mp_x86.dll 0x3007a3a4 (.rdata); refs=1 width=imm; first=0x3003c42d; owner=cg_getviewfov.
 * Example: 3003c42d   68 a4 a3 07 30               PUSH 0x3007a3a4
 */
const char cg_shellshockSilentLoopAliasName[23] = "shellshock_loop_silent";
/* Source: uo_cgame_mp_x86.dll 0x3007a3bc (.rdata); refs=1 width=imm; first=0x3003c418; owner=cg_getviewfov.
 * Example: 3003c418   68 bc a3 07 30               PUSH 0x3007a3bc
 */
const char cg_shellshockLoopAliasName[16] = "shellshock_loop";
/* Source: uo_cgame_mp_x86.dll 0x3007a3cc (.rdata); refs=2 width=imm; first=0x3003c133; owner=player_getmethod.
 * Example: 3003c133   68 cc a3 07 30               PUSH 0x3007a3cc | 3003c4be   68 cc a3 07 30               PUSH 0x3007a3cc
 */
const char cg_shellshockEndAbortAliasName[21] = "shellshock_end_abort";
/* Source: uo_cgame_mp_x86.dll 0x3007a3e4 (.rdata); refs=2 width=imm; first=0x3003c112; owner=player_getmethod.
 * Example: 3003c112   68 e4 a3 07 30               PUSH 0x3007a3e4 | 3003c3e7   68 e4 a3 07 30               PUSH 0x3007a3e4
 */
const char cg_genericShellshockAliasName[8] = "generic";
/* Source: uo_cgame_mp_x86.dll 0x3007a4e0 (.rdata); full ASCII payload proven
 * by globals.mcode and the original bytes. */
const char cg_clientFrameDiagnostic[] = "cg.clientFrame:%i\n";
/* Source: uo_cgame_mp_x86.dll 0x3007a4f8 (.rdata); adjacent NUL-terminated
 * diagnostic string proven by the original bytes through 0x3007a53b. */
const char cg_invalidWeaponSelectWarning[] =
    "WARNING: Invalid cg_weaponSelect setting %i (out of range 0 - %i)\n";
/* Source: uo_cgame_mp_x86.dll 0x3007a698 (.rdata); fatal view-builder diagnostic.
 * Preserve the original misspelling and newline exactly. */
const char cg_cinematicCameraUnavailableMessage[52] =
    "Cinimatic Cameras are not available in the MP exe.\n";
/* Source: uo_cgame_mp_x86.dll 0x3007a6cc (.rdata); refs=1 width=imm; first=0x300415d1; owner=g_moverpush.
 * Example: 300415d1   68 cc a6 07 30               PUSH 0x3007a6cc
 */
const char cg_turretMissingTagPlayerError[33] =
    "\x15Turret has no bone: tag_player\n";
/* Source: uo_cgame_mp_x86.dll 0x3007a850 (.rdata); refs=2 width=imm; first=0x30047c10; owner=script_method_scriptbuiltin_detach.
 * Example: 30047c10   68 50 a8 07 30               PUSH 0x3007a850 | 30047d68   68 50 a8 07 30               PUSH 0x3007a850
 */
const char cg_weaponIndexOutOfRangeErrorMessage[48] = "CG_FireWeapon: ent->weapon > BG_GetNumWeapons()";
/* Source: uo_cgame_mp_x86.dll 0x3007a8bc (.rdata); refs=1 width=imm; first=0x30046a55; owner=scr_objective_current.
 * Example: 30046a55   68 bc a8 07 30               PUSH 0x3007a8bc
 */
const char cg_weaponSlotCvarNameFormat[14] = "weaponslot %s";
/* Source: uo_cgame_mp_x86.dll 0x3007a8cc (.rdata); refs=3 width=imm; first=0x3004611f; owner=item_listbox_paint.
 * Example: 3004611f   68 cc a8 07 30               PUSH 0x3007a8cc | 30047c68   68 cc a8 07 30               PUSH 0x3007a8cc | 30047cce   68 cc a8 07 30               PUSH 0x3007a8cc
 */
const char cg_brassEjectTagName[10] = "tag_brass";
/* Source: uo_cgame_mp_x86.dll 0x3007a8d8 (.rdata); CG_FakeTrajectoryEffects
 * diagnostic emitted when the camera aim trace starts in solid. */
const char cg_fakeTrajectoryStartedInSolidMessage[29] =
    "%i: bullet started in solid\n";
/* Source: uo_cgame_mp_x86.dll 0x3007a8f8 (.rdata); refs=1 width=imm; first=0x30044861; owner=bg_checkpronevalid.
 * Example: 30044861   68 f8 a8 07 30               PUSH 0x3007a8f8
 */
const char cg_registerWeaponWarnAiOverlay[] = "^3WARNING: Weapon %s: Could not translate AI overlay description \"%s\"\n";
/* Source: uo_cgame_mp_x86.dll 0x3007a940 (.rdata); refs=1 width=imm; first=0x30044848; owner=bg_checkpronevalid.
 * Example: 30044848   68 40 a9 07 30               PUSH 0x3007a940
 */
const char cg_registerWeaponErrorAiOverlay[] = "Weapon %s: Could not translate AI overlay description \"%s\"";
/* Source: uo_cgame_mp_x86.dll 0x3007a97c (.rdata); refs=1 width=imm; first=0x300447ff; owner=bg_checkpronevalid.
 * Example: 300447ff   68 7c a9 07 30               PUSH 0x3007a97c
 */
const char cg_registerWeaponWarnModeName[] = "^3WARNING: Weapon %s: Could not translate mode name \"%s\"\n";
/* Source: uo_cgame_mp_x86.dll 0x3007a9b8 (.rdata); refs=1 width=imm; first=0x300447e6; owner=bg_checkpronevalid.
 * Example: 300447e6   68 b8 a9 07 30               PUSH 0x3007a9b8
 */
const char cg_registerWeaponErrorModeName[] = "Weapon %s: Could not translate mode name \"%s\"";
/* Source: uo_cgame_mp_x86.dll 0x3007a9e8 (.rdata); refs=1 width=imm; first=0x3004479d; owner=bg_checkpronevalid.
 * Example: 3004479d   68 e8 a9 07 30               PUSH 0x3007a9e8
 */
const char cg_registerWeaponWarnDisplayName[] = "^3WARNING: Weapon %s: Could not translate display name \"%s\"\n";
/* Source: uo_cgame_mp_x86.dll 0x3007aa28 (.rdata); refs=1 width=imm; first=0x30044784; owner=bg_checkpronevalid.
 * Example: 30044784   68 28 aa 07 30               PUSH 0x3007aa28
 */
const char cg_registerWeaponErrorDisplayName[] = "Weapon %s: Could not translate display name \"%s\"";
/* Source: uo_cgame_mp_x86.dll 0x3007aa5c (.rdata); refs=1 width=imm; first=0x30044646; owner=bg_checkpronevalid.
 * Example: 30044646   68 5c aa 07 30               PUSH 0x3007aa5c
 */
const char cg_registerWeaponInvalidProjectileModel[] = "Weapon %s does not specify a valid projectile model (%s)\n";
/* Source: uo_cgame_mp_x86.dll 0x3007aae0 (.rdata); refs=1 width=imm; first=0x30043bf3; owner=bg_checkpronevalid.
 * Example: 30043bf3   68 e0 aa 07 30               PUSH 0x3007aae0
 */
const char cg_registerWeaponWorldModelWarning[] = "WARNING: Weapon %s could not load world model\n";
/* Source: uo_cgame_mp_x86.dll 0x3007ab18 (.rdata); refs=2 width=imm; first=0x30043a95; owner=bg_checkpronevalid.
 * Example: 30043a95   68 18 ab 07 30               PUSH 0x3007ab18 | 30043ac5   68 18 ab 07 30               PUSH 0x3007ab18
 */
const char cg_registerWeaponLoopingAdsError[] = "\x15" "CG_RegisterWeapon: ADS anim [%s] cannot be looping";
/* Source: uo_cgame_mp_x86.dll 0x3007ab58 (.rdata); refs=1 width=imm; first=0x3004369e; owner=bg_checkpronevalid.
 * Example: 3004369e   68 58 ab 07 30               PUSH 0x3007ab58
 */
const char cg_registerWeaponMissingIdleError[] = "\x15" "CG_RegisterWeapon: No idle anim specified for [%s]";
/* Source: uo_cgame_mp_x86.dll 0x3007ab8c (.rdata); refs=1 width=imm; first=0x3004367f; owner=bg_checkpronevalid.
 * Example: 3004367f   68 8c ab 07 30               PUSH 0x3007ab8c
 */
const char cg_registerWeaponMissingHandError[] = "\x15" "CG_RegisterWeapon: No hand model specified for [%s]";
/* Source: uo_cgame_mp_x86.dll 0x3007abc4 (.rdata); refs=1 width=imm; first=0x30042f45; owner=CG_WeaponRunXModelAnims.
 * Example: 30042f45   68 c4 ab 07 30               PUSH 0x3007abc4
 * Repaired mechanical global -> the real datum is a Com_Printf format string, its
 * bytes dumped exactly via objdump -s -j .rdata @0x3007abc4:
 *   3007abc4  43475f57 6561706f 6e52756e 584d6f64  CG_WeaponRunXMod
 *   3007abd4  656c416e 696d733a 20556e6b 6e6f776e  elAnims: Unknown
 *   3007abe4  20776561 706f6e20 616e696d 6174696f   weapon animatio
 *   3007abf4  6e202569 0a00                         n %i\0
 */
const char cg_weaponRunXModelAnimsInvalidAnimFmt[54] =
    "CG_WeaponRunXModelAnims: Unknown weapon animation %i\n";
/* Source: uo_cgame_mp_x86.dll 0x3007ace0 (.rdata); refs=1; first=0x3004d3b7.
 * Mechanical owner g_dropartillery is wrong and the width was wrong: this is a
 * NUL-terminated format string the exporter truncated to its first dword
 * (0x6c694615 = bytes 15 46 69 6c). Verified bytes: 0x15 'F' 'i' 'l' 'e' ' ' '%'
 * 's' ',' ' ' 'l' 'i' 'n' 'e' ' ' '%' 'i' ':' ' ' '%' 's' 0x00 (22 bytes). The
 * leading 0x15 is a CoD console error-channel marker byte emitted verbatim.
 * Consumed by Com_ScriptError (0x3004d370) as the Com_Error format string.
 * Example: 3004d3b7   68 e0 ac 07 30               PUSH 0x3007ace0
 */
const char com_scriptErrorWithSourceFormat[22] = "\x15" "File %s, line %i: %s";
/* Source: uo_cgame_mp_x86.dll 0x3007ad20 (.rdata); refs=1; first=0x3004d25a.
 * The Com_BeginParseSession session-overflow error text. The mechanical export
 * truncated this NUL-terminated string to its first dword (0x6d6f4315); the real
 * bytes are 0x15 'C' 'o' 'm' ... verified via objdump on the .rdata image. The
 * leading 0x15 is the CoD console error-channel marker byte.
 * Example: 3004d25a   68 20 ad 07 30               PUSH 0x3007ad20
 */
const char com_parseSessionOverflowErrorMessage[] =
    "\x15" "Com_BeginParseSession: session overflow";
/* Source: uo_cgame_mp_x86.dll 0x3007afc8 (.rdata); refs=1 first=0x3004e8d5; consumed by va.
 * Example: 3004e8d5   68 c8 af 07 30               PUSH 0x3007afc8
 * The mechanical export truncated this NUL-terminated string to its first dword
 * (0x74744115 = bytes 15 41 74 74). It is the va() overflow message; the leading
 * 0x15 is the CoD console error-channel marker byte Com_Error emits verbatim.
 */
const char va_overrunErrorString[] = "\x15" "Attempted to overrun string in call to va()\n";
/* Source: uo_cgame_mp_x86.dll 0x3007bc40 (.rdata); refs=1; first=0x3004ff0c.
 * Example: 3004ff0c   68 40 bc 07 30               PUSH 0x3007bc40
 * NUL-terminated .rdata string; the mechanical export truncated it to its first dword
 * (0x72745315). Consumed on the String_Alloc (0x3004fe00) pool-overflow path as the
 * Com_Error(ERR_FATAL, ...) format. \x15 / \x14 are CoD console channel-marker
 * bytes emitted verbatim; EXE_ERR_OUT_OF_MEMORY is a localization token. */
const char stringAlloc_outOfMemory_msg[] = "\x15String_Alloc: \x14" "EXE_ERR_OUT_OF_MEMORY";
/* Source: uo_cgame_mp_x86.dll 0x3007bc68 (.rdata); refs=1; first=0x3004ff02.
 * Example: 3004ff02   68 68 bc 07 30               PUSH 0x3007bc68
 * NUL-terminated .rdata string; the mechanical export truncated it to its first dword
 * (0x69727453). Consumed on the String_Alloc (0x3004fe00) pool-overflow path as the
 * Com_Printf(fmt, size+1) format. */
const char stringAlloc_allocFailed_fmt[] = "String_Alloc: failed to allocate %d bytes\n";
/* Source: uo_cgame_mp_x86.dll 0x3007bc94 (.rdata); refs=1; first=0x3004fd75.
 * Example: 3004fd75   68 94 bc 07 30               PUSH 0x3007bc94
 * NUL-terminated .rdata string; the mechanical export truncated it to its first dword
 * (0x5f495515). Consumed on the UI_Alloc (0x3004fd50) pool-overflow path as the
 * Com_Error(ERR_DROP, ...) format. \x15 / \x14 are CoD console channel-marker
 * bytes emitted verbatim; EXE_ERR_OUT_OF_MEMORY is a localization token. */
const char uiAlloc_outOfMemory_msg[] = "\x15UI_Alloc: \x14" "EXE_ERR_OUT_OF_MEMORY";
/* Source: uo_cgame_mp_x86.dll 0x3007bcb8 (.rdata); refs=1; first=0x3004fd61.
 * Example: 3004fd61   68 b8 bc 07 30               PUSH 0x3007bcb8
 * NUL-terminated .rdata string; the mechanical export truncated it to its first dword
 * (0x415f4955) and mislabeled it a float. Consumed on the UI_Alloc (0x3004fd50)
 * pool-overflow path as the Com_Printf(fmt, size) format. */
const char uiAlloc_allocFailed_fmt[] = "UI_Alloc: failed to allocate %d bytes\n";
/* Source: uo_cgame_mp_x86.dll 0x3007bce0 (.rdata); refs=472 width=4; first=0x30005065; owner=cg_drawweaponselect.
 * Example: 30005065   d9 05 e0 bc 07 30            FLD float ptr [0x3007bce0] | 300056e8   d8 15 e0 bc 07 30            FCOM float ptr [0x3007bce0] | 300067d2   d8 1d e0 bc 07 30            FCOMP float ptr [0x3007bce0]
 */
const float floatOne = 1.0f;
/* Source: uo_cgame_mp_x86.dll 0x3007bce8 (.rdata); refs=155 width=4; first=0x300043a4; owner=bg_setupsharedammoindexes.
 * Example: 300043a4   d8 1d e8 bc 07 30            FCOMP float ptr [0x3007bce8] | 30004ef9   d8 0d e8 bc 07 30            FMUL float ptr [0x3007bce8] | 30005007   d8 0d e8 bc 07 30            FMUL float ptr [0x3007bce8]
 */
const float floatOneHalf = 0.5f;
/* Source: uo_cgame_mp_x86.dll 0x3007bcec (.rdata); refs=576 width=4; first=0x30003909; owner=cg_registersounds.
 * Example: 30003909   d8 1d ec bc 07 30            FCOMP float ptr [0x3007bcec] | 30003e32   d9 05 ec bc 07 30            FLD float ptr [0x3007bcec] | 300042fa   d9 05 ec bc 07 30            FLD float ptr [0x3007bcec]
 */
const float floatZero = 0.0f;
/* Source: uo_cgame_mp_x86.dll 0x3007bcf8 (.rdata); refs=66 width=8; first=0x3000640a; owner=pm_weapon_startfiring.
 * Example: 3000640a   dc 1d f8 bc 07 30            FCOMP double ptr [0x3007bcf8] | 3000a32e   dd 05 f8 bc 07 30            FLD double ptr [0x3007bcf8] | 3000a5bb   dd 05 f8 bc 07 30            FLD double ptr [0x3007bcf8]
 */
/* Bytes 00 00 00 00 00 00 f0 3f == IEEE-754 double 1.0; superseded from the
 * mechanical uint8_t[8] capture to its real double type (see globals.h). */
const double doubleOne = 1.0; /* 0x3007bcf8 */
/* Source: uo_cgame_mp_x86.dll 0x3007bd64 (.rdata); refs=20 width=4; first=0x30008e8c; owner=cg_drawcrosshairnames.
 * Example: 30008e8c   d8 1d 64 bd 07 30            FCOMP float ptr [0x3007bd64] | 30013bf5   d8 0d 64 bd 07 30            FMUL float ptr [0x3007bd64] | 30013c34   d8 1d 64 bd 07 30            FCOMP float ptr [0x3007bd64]
 */
const float colorByteScale = 255.0f; /* 0x437f0000 */
/* Source: uo_cgame_mp_x86.dll 0x3007bd70 (.rdata); refs=24 width=4; first=0x3000cc94; owner=veh_findvaliddismountspot.
 * Example: 3000cc94   d8 0d 70 bd 07 30            FMUL float ptr [0x3007bd70] | 3000ccbe   d8 0d 70 bd 07 30            FMUL float ptr [0x3007bd70] | 30019386   d8 0d 70 bd 07 30            FMUL float ptr [0x3007bd70]
 */
/* Bit pattern 0x3c8efa35 == 0.017453292f == M_PI/180: the engine degrees->radians
 * scale. All 24 refs are `FMUL float ptr` operands, proving float width (the
 * mechanical uint32_t truncation is superseded here). Named deg2rad by role. */
/* Source: uo_cgame_mp_x86.dll 0x3007bd70. */
const float deg2rad = 0.017453292f;
/* Source: uo_cgame_mp_x86.dll 0x3007bdb4 (.rdata); refs=20 width=4; first=0x3000416d; owner=launchitem.
 * Example: 3000416d   d8 1d b4 bd 07 30            FCOMP float ptr [0x3007bdb4] | 3000ae52   d8 0d b4 bd 07 30            FMUL float ptr [0x3007bdb4] | 30013b80   d8 0d b4 bd 07 30            FMUL float ptr [0x3007bdb4]
 */
const float floatOneHundredth = 0.01f;
/* Source: uo_cgame_mp_x86.dll 0x3007be24 (.rdata); width=4.
 * Bit pattern 0x3b808081. CG_GetHudElemInfo multiplies every packed color byte
 * by this binary32 operand at 0x30029e23/48/6d/92 and 0x30029eae/c3/d8/ed. */
const float colorByteToUnitScale = 0.00392156886f;
/* Source: uo_cgame_mp_x86.dll 0x3007be44 (.rdata); refs=6 width=4; first=0x30005f81; owner=veh_updateweapon.
 * Example: 30005f81   d8 0d 44 be 07 30            FMUL float ptr [0x3007be44] | 30015322   d8 05 44 be 07 30            FADD float ptr [0x3007be44] | 30015bd9   d8 05 44 be 07 30            FADD float ptr [0x3007be44]
 */
const float twoPi = 6.2831855f;
/* Source: uo_cgame_mp_x86.dll 0x3007be48 (.rdata); refs=2 width=4; first=0x3001531c; owner=bg_gettotalammoreserve.
 * Example: 3001531c   d8 0d 48 be 07 30            FMUL float ptr [0x3007be48] | 30015bd1   d8 0d 48 be 07 30            FMUL float ptr [0x3007be48]
 */
const float bobCycleRadiansPerStep = 0.024639944f;
/* Source: uo_cgame_mp_x86.dll 0x3007be4c (.rdata); refs=2 width=4; first=0x30013bcb; owner=parseconfigstringtostruct.
 * Example: 30013bcb   d8 05 4c be 07 30            FADD float ptr [0x3007be4c] | 30013bd1   d8 05 4c be 07 30            FADD float ptr [0x3007be4c]
 */
const float weaponIdleAirborneAdd = 1.28f;
/* Source: uo_cgame_mp_x86.dll 0x3007be58 (.rdata); width=4.
 * Bit pattern 0x3e800000. CG_GetHudElemInfo uses it as an m32 multiplier at
 * 0x30029c62 for the small-font scale. */
const float floatOneQuarter = 0.25f;
/* Source: uo_cgame_mp_x86.dll 0x3007be70 (.rdata); refs=12 width=4; first=0x30005234; owner=cg_drawweaponselect.
 * Example: 30005234   d8 0d 70 be 07 30            FMUL float ptr [0x3007be70] | 30005242   d8 0d 70 be 07 30            FMUL float ptr [0x3007be70] | 300056db   d8 2d 70 be 07 30            FSUBR float ptr [0x3007be70]
 */
const float floatThreeHalves = 1.5f;
/* Source: uo_cgame_mp_x86.dll 0x3007be78 (.rdata); refs=9 width=4; first=0x3001949e; owner=cg_parseimpacteffects.
 * Example: 3001949e   d8 0d 78 be 07 30            FMUL float ptr [0x3007be78] | 300194e7   d8 0d 78 be 07 30            FMUL float ptr [0x3007be78] | 3001fbe7   d8 0d 78 be 07 30            FMUL float ptr [0x3007be78]
 */
/* Source: uo_cgame_mp_x86.dll 0x3007be78. */
const float DEG_TO_HALF_RAD = 0.0087266462f;
/* Source: uo_cgame_mp_x86.dll 0x3007bf80 (.rdata); width=4.
 * Bit pattern 0x3eaaaaab. CG_GetHudElemInfo uses this binary32 multiplier at
 * 0x30029c23 and 0x30029c46 for the two large-font modes. */
const float floatOneThird = 0.33333334f;
/* Source: uo_cgame_mp_x86.dll 0x3007c018 (.rdata); refs=8 width=4; first=0x30019b5b; owner=stuckinclient.
 * Example: 30019b5b   d8 0d 18 c0 07 30            FMUL float ptr [0x3007c018] | 300271f0   d8 1d 18 c0 07 30            FCOMP float ptr [0x3007c018] | 30027205   d9 05 18 c0 07 30            FLD float ptr [0x3007c018]
 */
const float markFadeFraction = 0.15f; /* 0x3e19999a */
/* Source: uo_cgame_mp_x86.dll 0x3007c090 (.rdata); refs=2 width=4; first=0x300402db; owner=veh_setupcollmap.
 * Example: 300402db   d8 0d 90 c0 07 30            FMUL float ptr [0x3007c090] | 300420bb   d8 0d 90 c0 07 30            FMUL float ptr [0x3007c090]
 */
/* Source: uo_cgame_mp_x86.dll 0x3007c090. */
const float HALF_RAD_TO_DEG = 114.59155f;
/* Source: uo_cgame_mp_x86.dll 0x3007c094 (.rdata); refs=1 width=4; first=0x30040300; owner=veh_setupcollmap.
 * Example: 30040300   d8 0d 94 c0 07 30            FMUL float ptr [0x3007c094]
 */
/* Source: uo_cgame_mp_x86.dll 0x3007c094. */
const float PULSE_RATE_2500MS = 0.0025132743f;
/* The complete animation-script parser tables now have one source in
 * src/bg/bg_animation_script_data.c. */
/* Source: uo_cgame_mp_x86.dll 0x30082774 (.data).
 * PE_RELOCATION_VALUES_VERIFIED. */
const char *const *cg_eventNamesPtr = cg_eventNames;
/* Source: uo_cgame_mp_x86.dll 0x30082774 (.data); refs=6 width=4; first=0x30022874; owner=cmd_callvote_f.
 * Example: 30022874   8b 15 74 27 08 30            MOV EDX,dword ptr [0x30082774] | 30022b5e   8b 0d 74 27 08 30            MOV ECX,dword ptr [0x30082774] | 30022b9a   a1 74 27 08 30               MOV EAX,[0x30082774]
 */
/* Source: uo_cgame_mp_x86.dll 0x30082428..0x30082774 (.data).
 * PE_DIRECT_RELOCATION_INITIALIZER: recover_string_pointer_initializers.py emits
 * all 211 string pointers directly from the PE table. */
const char *cg_eventNames[EV_MAX_EVENTS] = {
#include "qcommon/entity_event_names.inc"
};
/* Source: uo_cgame_mp_x86.dll 0x300827a0..0x300840c0 (.data).
 * bg_itemlist[] — shared item-definition table (gitem_t, 0x30 bytes, 134 rows,
 * 1-based item index; row 0 unused). Rebuilt/finished by
 * BG_FillInWeaponItems (0x300103e0) from bg_weaponInfos[]. Supersedes the
 * mechanical exporter's per-field splits of this contiguous array: 0x300827a0
 * (boxdistsqrdexceeds, refs 0x30005e3f/0x3001e6dd/0x3004362d), 0x300827c0 and
 * 0x300827c4 (vectordistance2d, row 0's kind/weapon fields), and 0x300827d0
 * (pm_setweaponreloadaddammodelay, row 1's base). The original .data seeds each
 * row's string fields with the "emptyitem_..." (0x30074a10/0x30074a0c) defaults;
 * PE_DIRECT_RELOCATION_INITIALIZER: recover_string_pointer_initializers.py emits
 * every string pointer and scalar field from all 134 PE records.
 */
gitem_t bg_itemlist[134] = {
#include "recovered_initializers/bg_itemlist.inc"
};
/* Source: uo_cgame_mp_x86.dll 0x3008425c..0x30084280 (.data). */
const pmLerpEntry_t pm_viewHeightCrouchToProneLinear[3] = {
    { 0, 40.0f, 0 }, { 100, 11.0f, 0 },
    { PM_LERP_TABLE_END, 0.0f, 0 }
};
/* Source: uo_cgame_mp_x86.dll 0x300842e0 (.data); refs=2 width=4; first=0x30014a9a.
 * Example: 30014a9a   8b 15 e0 42 08 30            MOV EDX,dword ptr [0x300842e0] | 30014bd7   8b 15 e0 42 08 30            MOV EDX,dword ptr [0x300842e0]
 * Debug tag string pointer read as the "%s" of the shared " %i %s_" line prefix.
 * Static initializer holds .rdata 0x30074ab4 = "CG" (the cgame subsystem tag).
 * PE_RELOCATION_VALUES_VERIFIED. */
const char *PMDebugPrefix = "CG";
/* Source: uo_cgame_mp_x86.dll 0x300842e4 (.data); refs=1 width=4; first=0x3000ffa0; owner=pm_setmovementdir.
 * Example: 3000ffa0   a1 e4 42 08 30               MOV EAX,[0x300842e4]
 * PE_RELOCATION_VALUES_VERIFIED: pointer target is "weapons/mp".
 */
const char *bg_weaponDefsPath = "weapons/mp";
/* Source: uo_cgame_mp_x86.dll 0x30085164 (.data); refs=2 width=4; first=0x30014a8e.
 * Example: 30014a8e   39 35 64 51 08 30            CMP dword ptr [0x30085164],ESI | 30014ab4   89 35 64 51 08 30            MOV dword ptr [0x30085164],ESI
 * Last-logged weaponState cached by PM_Weapon_PrintWeaponState (0x30014a80).
 * Initialized to -1 (no state logged yet); written only by that logger. */
int32_t PMDebugLastWeaponState = -1;
/* Source: uo_cgame_mp_x86.dll 0x30085168 (.data); refs=1 width=4; first=0x30014bfe.
 * Example: 30014bfe   89 35 68 51 08 30            MOV dword ptr [0x30085168],ESI
 * Last-logged weapon-anim pose latched by PM_Weapon_PrintWeaponAnim (0x30014bd0),
 * write-only (never read back). Initialized to -1. The anim-pose twin of
 * PMDebugLastWeaponState; repaired from the mechanical uint32_t/owner label. */
uint32_t PMDebugLastWeaponAnim = (uint32_t)0xffffffffu;
/* Source: uo_cgame_mp_x86.dll 0x3008516c..0x300851ac (.data). */
const vec4_t cg_hudColorTable[4] = {
    { 1.0f, 0.2f, 0.2f, 1.0f }, { 0.2f, 0.2f, 1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.7f, 0.7f, 0.7f, 1.0f }
};
/* Source: uo_cgame_mp_x86.dll 0x300851ac..0x300851c4 (.data), six pointers.
 * Raw pointer order (objdump): 0x30077278, 0x30077260, 0x3007724c,
 * 0x30077234, 0x3007721c, 0x30077204. CG_Vehicle_DoControllers walks exactly
 * these six cells and passes each pointed-to string directly to trap 0xb2.
 * PE_RELOCATION_VALUES_VERIFIED. */
const char *const cg_vehicleWheelTags[6] = {
    "tag_wheel_front_left",
    "tag_wheel_front_right",
    "tag_wheel_back_left",
    "tag_wheel_back_right",
    "tag_wheel_middle_left",
    "tag_wheel_middle_right",
};
/* Source: uo_cgame_mp_x86.dll 0x300851c4 (.data); refs=4 width=imm; first=0x3002132f; owner=bg_animparseanimscript.
 * Example: 3002132f   3d c4 51 08 30               CMP EAX,0x300851c4 | 3002449e   68 c4 51 08 30               PUSH 0x300851c4 | 300260f5   ba c4 51 08 30               MOV EDX,0x300851c4
 */
vec3_t cg_flameTraceMins = { -2.0f, -2.0f, -2.0f };
/* Source: uo_cgame_mp_x86.dll 0x300851d0 (.data); refs=4 width=4; first=0x300244c4; owner=cg_addflamechunks.
 * Example: 300244c4   bb d0 51 08 30               MOV EBX,0x300851d0 | 3002606b   d8 1d d0 51 08 30            FCOMP float ptr [0x300851d0] | 300260f0   b9 d0 51 08 30               MOV ECX,0x300851d0
 */
vec3_t cg_flameTraceMaxs = { 2.0f, 2.0f, 2.0f };
/* Source: uo_cgame_mp_x86.dll 0x300851dc (.data); refs=1 width=4; first=0x30026db1; owner=cg_addflamespritetoscene.
 * Example: 30026db1   a1 dc 51 08 30               MOV EAX,[0x300851dc]
 */
uint32_t cg_flameSpriteState[5] = {
    1u, 1u, 0x3f800000u, 0x3f800000u, 0x3f800000u
};
/* Source: uo_cgame_mp_x86.dll 0x300851e0 (.data); refs=1 width=4; first=0x30027a99; owner=bg_finditem.
 * Example: 30027a99   c7 05 e0 51 08 30 00 00 00 00 MOV dword ptr [0x300851e0],0x0
 * Renamed cg_flameInitStateReset; role provisional (single write to 0, no reader).
 * Mechanical initial value 1 preserved (the .data image seed). */
/* Source: uo_cgame_mp_x86.dll 0x300851f0..0x30085d70 (.data).
 * Complete 184-entry cgame cvar registration table, recovered from its four
 * initialized dwords per entry. Pointer targets, strings, defaults, flags, count,
 * and 0x10 stride are all checked against the PE image.
 * PE_DIRECT_RELOCATION_INITIALIZER: recover_cvar_table.py emits every pointer
 * and scalar from the corresponding PE entry. */
#include "recovered_initializers/cg_cvar_object_decls.inc"
const cvarTable_t cg_cvarTable[CG_CVAR_TABLE_COUNT] = { /* 0x300851f0; PE_DIRECT_RELOCATION_INITIALIZER */
#include "recovered_initializers/cg_cvar_table.inc"
};
/* Source: uo_cgame_mp_x86.dll 0x30085d70..0x30085d9b (.data); 11 initialized string pointers.
 * cg_numberShaderNames[11] — HUD bitmap-number shader names; see globals.h. The mechanical
 * export captured only element 0 (the "zero_32b" pointer at 0x300784b8); superseded here as
 * the full initialized table (pointer values verified via objdump of 0x30085d70).
 * PE_RELOCATION_VALUES_VERIFIED. */
const char *const cg_numberShaderNames[11] = {
    "gfx/2d/numbers/zero_32b",  "gfx/2d/numbers/one_32b",
    "gfx/2d/numbers/two_32b",   "gfx/2d/numbers/three_32b",
    "gfx/2d/numbers/four_32b",  "gfx/2d/numbers/five_32b",
    "gfx/2d/numbers/six_32b",   "gfx/2d/numbers/seven_32b",
    "gfx/2d/numbers/eight_32b", "gfx/2d/numbers/nine_32b",
    "gfx/2d/numbers/minus_32b"
};
/* Source: uo_cgame_mp_x86.dll 0x30085da0 (.data); refs=6 width=4; first=0x3002f005; owner=bullet_fire_extended.
 * cg_stanceHintChangeTime — cg.time (ms) at which the player's stance/snapshot flags
 * last changed; the animation windows key off (cg.time - this) against 0xbb8/0x7d0/
 * 0x3e8. Reset to -1 when no snapshot is present. Written/read by CG_DrawPlayerStance
 * (0x3002f005..0x3002f3e0). Old owner label "bullet_fire_extended" was a wrong
 * size-guess; superseded here.
 */
int32_t cg_stanceHintChangeTime = -1;
/* Source: uo_cgame_mp_x86.dll 0x30085da4 (.data); refs=8 width=1/4; first=0x3002f019; owner=bullet_fire_extended.
 * cg_stanceHintFlags — the latched stance flags word (predictedPlayerState snapshot
 * flags masked with 0x10003) used to pick the stance/fatigue icon and hint tables.
 * Bit 0x10000 = flash/sprint, 0x1 = prone, 0x2 = crouch. Written and bit-tested by
 * CG_DrawPlayerStance (0x3002f07a etc.). Old owner label "bullet_fire_extended" was a
 * wrong size-guess; superseded here.
 */
uint32_t cg_stanceHintFlags = 0xffffffffu;
/* Source: uo_cgame_mp_x86.dll 0x30085da8 (.data); refs=4 width=4; first=0x3002f074; owner=bullet_fire_extended.
 * cg_stanceHintExpireTime — cg.time (ms) until which the stance-change slide/hint
 * animation stays active; set to cg.time+1500 on a change, or -1 when suppressed
 * (0x304877c4 bit 0). Read/written by CG_DrawPlayerStance (0x3002f074..0x3002f0aa).
 */
int32_t cg_stanceHintExpireTime = -1;
/* Source: uo_cgame_mp_x86.dll 0x30085dac (.data). cg_statBarHoldSeed — the reset
 * value (1) for cg_statBarHoldTimer; read-only in CG_DrawStatBarWithDecay
 * (0x3002f9d0). The mechanical (owner=stopfollowing) label was a size-guess; the
 * only consumer is the 2D stat-bar draw, not the server StopFollowing routine. */
int32_t cg_statBarHoldSeed = 1;
/* Source: uo_cgame_mp_x86.dll 0x30085db0 (.data). cg_statBarHoldTimer — the ms
 * countdown that holds the stat bar's trailing indicator before it decays; seeded
 * from cg_statBarHoldSeed and decremented by cg.frametime, floored at 0. */
int32_t cg_statBarHoldTimer = 1;
/* Source: uo_cgame_mp_x86.dll 0x30085db4 (.data). cg_statBarLastClientNum — the
 * local clientNum the stat bar's smoothing state is valid for; -1 until first use,
 * and a change snaps the trailing bar to the live value. */
int32_t cg_statBarLastClientNum = -1;
/* Source: uo_cgame_mp_x86.dll 0x30085db8 (.data); refs=2 width=4; first=0x30037c29; owner=cg_entitypreevent.
 * Example: 30037c29   8b 1d b8 5d 08 30            MOV EBX,dword ptr [0x30085db8] | 30037c43   89 1d b8 5d 08 30            MOV dword ptr [0x30085db8],EBX
 */
/* Source: uo_cgame_mp_x86.dll 0x30085db8. */
int32_t cg_scoreboardLeadTeam = 2;
/*
 * Source: uo_cgame_mp_x86.dll 0x30085dc0 / 0x30085e30 (.data) — the shellshock
 * parameter definition tables (see globals.h). 27 entries each, proven by
 * CG_ShellShockLoad (0x3003b950): CG_COM_LOAD_CVARS_FROM_BUFFER count 0x1b over the name table and
 * the loop limit 0x6c (27*4) over the target table. The mechanical export had
 * captured only each table's first dword; the full arrays are restored here from
 * the .data image (name strings from .rdata, target pointers into cgame vmCvar
 * BSS). cg_shockParamNames[i] is the cg_shock_* cvar name; cg_shockParamTargets[i]
 * points at the matching scattered vmCvar_t object.
 * PE_RELOCATION_VALUES_VERIFIED: both 27-entry pointer tables match the PE.
 */
const char *const cg_shockParamNames[CG_SHOCK_PARAM_COUNT] = {
    "cg_shock_screenBlendTime",       /* 0x30078bc8 */
    "cg_shock_screenBlendFadeTime",   /* 0x30078ba8 */
    "cg_shock_viewKickPeriod",        /* 0x30078b90 */
    "cg_shock_viewKickRadius",        /* 0x30078b74 */
    "cg_shock_sound",                 /* 0x30078b60 */
    "cg_shock_soundFadeInTime",       /* 0x30078b44 */
    "cg_shock_soundFadeOutTime",      /* 0x30078b20 */
    "cg_shock_soundLoopFadeTime",     /* 0x30078b00 */
    "cg_shock_soundLoopEndDelay",     /* 0x30078ae0 */
    "cg_shock_soundRoomType",         /* 0x30078ac0 */
    "cg_shock_soundWetLevel",         /* 0x30078a9c */
    "cg_shock_soundModEndDelay",      /* 0x30078a7c */
    "cg_shock_volume_auto",           /* 0x30078a64 */
    "cg_shock_volume_menu",           /* 0x30078a48 */
    "cg_shock_volume_weapon",         /* 0x30078a30 */
    "cg_shock_volume_voice",          /* 0x30078a18 */
    "cg_shock_volume_item",           /* 0x300789fc */
    "cg_shock_volume_body",           /* 0x300789e4 */
    "cg_shock_volume_local",          /* 0x300789cc */
    "cg_shock_volume_music",          /* 0x300789b4 */
    "cg_shock_volume_announcer",      /* 0x30078998 */
    "cg_shock_volume_shellshock",     /* 0x3007897c */
    "cg_shock_mouse",                 /* 0x3007896c */
    "cg_shock_mouse_maxpitchspeed",   /* 0x3007894c */
    "cg_shock_mouse_maxyawspeed",     /* 0x3007892c */
    "cg_shock_mouse_sensitivityscale",/* 0x3007890c */
    "cg_shock_mouse_fadeTime",        /* 0x300788f4 */
};
/* The 27 target-table entries at 0x30085e30 are scattered vmCvar_t objects,
 * not one contiguous array. Each address below is the corresponding original
 * pointer-table value; the initializer audit proves each complete 0x110-byte
 * object is zero-filled in the PE image. */
/* Source: uo_cgame_mp_x86.dll 0x30456e60 (.data). */
vmCvar_t cg_shockScreenBlendTime;
/* Source: uo_cgame_mp_x86.dll 0x304557e0 (.data). */
vmCvar_t cg_shockScreenBlendFadeTime;
/* Source: uo_cgame_mp_x86.dll 0x30455a20 (.data). */
vmCvar_t cg_shockViewKickPeriod;
/* Source: uo_cgame_mp_x86.dll 0x30458a80 (.data). */
vmCvar_t cg_shockViewKickRadius;
/* Source: uo_cgame_mp_x86.dll 0x30451a00 (.data). */
vmCvar_t cg_shockSound;
/* Source: uo_cgame_mp_x86.dll 0x3044f720 (.data). */
vmCvar_t cg_shockSoundFadeInTime;
/* Source: uo_cgame_mp_x86.dll 0x3052fd60 (.data). */
vmCvar_t cg_shockSoundFadeOutTime;
/* Source: uo_cgame_mp_x86.dll 0x30457400 (.data). */
vmCvar_t cg_shockSoundLoopFadeTime;
/* Source: uo_cgame_mp_x86.dll 0x30456c20 (.data). */
vmCvar_t cg_shockSoundLoopEndDelay;
/* Source: uo_cgame_mp_x86.dll 0x3052f460 (.data). */
vmCvar_t cg_shockSoundRoomType;
/* Source: uo_cgame_mp_x86.dll 0x30450c80 (.data). */
vmCvar_t cg_shockSoundWetLevel;
/* Source: uo_cgame_mp_x86.dll 0x304528a0 (.data). */
vmCvar_t cg_shockSoundModEndDelay;
/* Source: uo_cgame_mp_x86.dll 0x3044f4e0 (.data). */
vmCvar_t cg_shockVolumeAuto;
/* Source: uo_cgame_mp_x86.dll 0x30457e20 (.data). */
vmCvar_t cg_shockVolumeMenu;
/* Source: uo_cgame_mp_x86.dll 0x30413220 (.data). */
vmCvar_t cg_shockVolumeWeapon;
/* Source: uo_cgame_mp_x86.dll 0x30421780 (.data). */
vmCvar_t cg_shockVolumeVoice;
/* Source: uo_cgame_mp_x86.dll 0x3052e920 (.data). */
vmCvar_t cg_shockVolumeItem;
/* Source: uo_cgame_mp_x86.dll 0x304560e0 (.data). */
vmCvar_t cg_shockVolumeBody;
/* Source: uo_cgame_mp_x86.dll 0x30455b40 (.data). */
vmCvar_t cg_shockVolumeLocal;
/* Source: uo_cgame_mp_x86.dll 0x30457520 (.data). */
vmCvar_t cg_shockVolumeMusic;
/* Source: uo_cgame_mp_x86.dll 0x304408e0 (.data). */
vmCvar_t cg_shockVolumeAnnouncer;
/* Source: uo_cgame_mp_x86.dll 0x30440580 (.data). */
vmCvar_t cg_shockVolumeShellshock;
/* Source: uo_cgame_mp_x86.dll 0x3052f580 (.data). */
vmCvar_t cg_shockMouse;
/* Source: uo_cgame_mp_x86.dll 0x30458060 (.data). */
vmCvar_t cg_shockMouseMaxPitchSpeed;
/* Source: uo_cgame_mp_x86.dll 0x30412fe0 (.data). */
vmCvar_t cg_shockMouseMaxYawSpeed;
/* Source: uo_cgame_mp_x86.dll 0x3052fe80 (.data). */
vmCvar_t cg_shockMouseSensitivityScale;
/* Source: uo_cgame_mp_x86.dll 0x30450800 (.data). */
vmCvar_t cg_shockMouseFadeTime;
/* Source: uo_cgame_mp_x86.dll 0x30085e30 (.data): 27 relocated pointers,
 * exactly the values listed above, in cg_shockParamNames order.
 * PE_RELOCATION_VALUES_VERIFIED. */
vmCvar_t *const cg_shockParamTargets[CG_SHOCK_PARAM_COUNT] = {
    &cg_shockScreenBlendTime, &cg_shockScreenBlendFadeTime,
    &cg_shockViewKickPeriod, &cg_shockViewKickRadius, &cg_shockSound,
    &cg_shockSoundFadeInTime, &cg_shockSoundFadeOutTime,
    &cg_shockSoundLoopFadeTime, &cg_shockSoundLoopEndDelay,
    &cg_shockSoundRoomType, &cg_shockSoundWetLevel, &cg_shockSoundModEndDelay,
    &cg_shockVolumeAuto, &cg_shockVolumeMenu, &cg_shockVolumeWeapon,
    &cg_shockVolumeVoice, &cg_shockVolumeItem, &cg_shockVolumeBody,
    &cg_shockVolumeLocal, &cg_shockVolumeMusic, &cg_shockVolumeAnnouncer,
    &cg_shockVolumeShellshock, &cg_shockMouse, &cg_shockMouseMaxPitchSpeed,
    &cg_shockMouseMaxYawSpeed, &cg_shockMouseSensitivityScale,
    &cg_shockMouseFadeTime,
};
/* Source: uo_cgame_mp_x86.dll 0x30085ea0 (.data); refs=3 width=imm; first=0x3003f7bb; owner=scr_vehicle_damagescale.
 * Example: 3003f7bb   68 a0 5e 08 30               PUSH 0x30085ea0 | 3003f846   68 a0 5e 08 30               PUSH 0x30085ea0 | 3003f8b8   68 a0 5e 08 30               PUSH 0x30085ea0
 */
uint32_t cg_vehicleDamageBoundsMinsBits[3] = {
    0xc0800000u, 0xc0800000u, 0xc0800000u
};
/* Source: uo_cgame_mp_x86.dll 0x30085eac (.data); refs=1 width=imm; first=0x3003f7df; owner=scr_vehicle_damagescale.
 * Example: 3003f7df   bb ac 5e 08 30               MOV EBX,0x30085eac
 */
uint32_t cg_vehicleDamageBoundsMaxsBits[3] = {
    0x40800000u, 0x40800000u, 0x40800000u
};
/* Source: uo_cgame_mp_x86.dll 0x30085eb8 (.data); refs=1 width=imm; first=0x3003fd06; owner=pm_weapon_finishweaponchange.
 * Example: 3003fd06   68 b8 5e 08 30               PUSH 0x30085eb8
 */
uint32_t cg_weaponChangeBoundsMinsBits[3] = {
    0xc1000000u, 0xc1000000u, 0xc1000000u
};
/* Source: uo_cgame_mp_x86.dll 0x30085ec4 (.data); refs=1 width=imm; first=0x3003fcc0; owner=pm_weapon_finishweaponchange.
 * Example: 3003fcc0   bb c4 5e 08 30               MOV EBX,0x30085ec4
 */
uint32_t cg_weaponChangeBoundsMaxsBits[3] = {
    0x41000000u, 0x41000000u, 0x41000000u
};
/* Source: uo_cgame_mp_x86.dll 0x30085ed0 (.data). */
float cg_weaponChangeAngleLimit = 90.0f;
/* Source: uo_cgame_mp_x86.dll 0x30085ed4 (.data); refs=1 width=imm; first=0x30041d52; owner=pm_ufomove.
 * Example: 30041d52   68 d4 5e 08 30               PUSH 0x30085ed4
 */
uint32_t cg_ufoMoveBoundsMinsBits[3] = {
    0xc1000000u, 0xc1000000u, 0xc1000000u
};
/* Source: uo_cgame_mp_x86.dll 0x30085ee0 (.data); refs=1 width=imm; first=0x30041d72; owner=pm_ufomove.
 * Example: 30041d72   bb e0 5e 08 30               MOV EBX,0x30085ee0
 */
uint32_t cg_ufoMoveBoundsMaxsBits[3] = {
    0x41000000u, 0x41000000u, 0x41000000u
};
/* Source: uo_cgame_mp_x86.dll 0x30085eec (.data); refs=6 width=4; first=0x300463f5; owner=item_listbox_paint.
 * Example: 300463f5   8b 15 ec 5e 08 30            MOV EDX,dword ptr [0x30085eec] | 3004642f   8b 0d ec 5e 08 30            MOV ECX,dword ptr [0x30085eec] | 30047e15   8b 04 8d ec 5e 08 30         MOV EAX,dword ptr [ECX*0x4 + 0x30085eec]
 * PE_RELOCATION_VALUES_VERIFIED: six tag-string pointers match the PE.
 */
const char *cg_muzzleTagNames[6] = {
    "tag_flash",           /* [0] 0x300772c0 */
    "tag_flash_11",        /* [1] 0x3007ac38 */
    "tag_flash_2",         /* [2] 0x3007ac2c */
    "tag_flash_22",        /* [3] 0x3007ac1c */
    "tag_altfire",         /* [4] 0x3007ac10 */
    "tag_secondary_flash", /* [5] 0x3007abfc */
};
/* Source: uo_cgame_mp_x86.dll 0x30085f04..0x30085f20 (.data). */
const float cg_muzzleEffectBoundsAndBias[7] = {
    -4.0f, -4.0f, -4.0f, 4.0f, 4.0f, 4.0f, 0.0f
};
/* 0x30085efc and 0x30085f00 are cg_muzzleTagNames[4] and [5], not standalone
 * globals; their direct dword loads select tag_altfire/tag_secondary_flash. */
/* Source: uo_cgame_mp_x86.dll 0x30134cdc; reset after CG_LoadHud_f reloads HUD menus. */
int32_t cg_loadHudState;
/* Source: uo_cgame_mp_x86.dll 0x3008acbc (.data); refs=1 width=4; first=0x3004fe13.
 * Example: 3004fe13   a1 bc ac 08 30               MOV EAX,[0x3008acbc]
 * Resolved on consume by String_Alloc (0x3004fe00): when the input string is empty
 * ([EBP]==0), String_Alloc returns this pointer directly. The stored value 0x30074a0c
 * points at a NUL byte in .rdata (an empty string ""), so this is the static
 * empty-string sentinel returned for String_Alloc(""). Typed as const char *; the
 * mechanical export captured only the pointer bits as a uint32_t. (Owner label
 * scriptentcmd_moveto was the exporter's wrong first-toucher / size-guess.)
 * PE_RELOCATION_VALUES_VERIFIED. */
const char *emptyStr = ""; /* 0x3008acbc -> "" at 0x30074a0c; PE_RELOCATION_VALUES_VERIFIED */
/* 0x3008bd61 is byte 1 of the containing CRT state word; the byte ORs update
 * that packed flag word. */
/* Animation-script parser scratch storage now has one common definition in
 * src/bg/bg_animation_script_data.c. */
/* Source: uo_cgame_mp_x86.dll 0x300a5108 (.data); refs=4 width=4; first=0x30001319; owner=g_testentityposition.
 * Example: 30001319   8b 0d 08 51 0a 30            MOV ECX,dword ptr [0x300a5108] | 30001395   a1 08 51 0a 30               MOV EAX,[0x300a5108] | 30001741   8b 15 08 51 0a 30            MOV EDX,dword ptr [0x300a5108]
 */
int32_t *bgRuntimeAnimationCount = NULL;
/* Source: uo_cgame_mp_x86.dll 0x300a7820 (.data); refs=8 width=4; first=0x300012a1; owner=g_testentityposition.
 * Example: 300012a1   a1 20 78 0a 30               MOV EAX,[0x300a7820] | 3000132b   8b 3d 20 78 0a 30            MOV EDI,dword ptr [0x300a7820] | 3000135f   8b 0d 20 78 0a 30            MOV ECX,dword ptr [0x300a7820]
 */
bg_runtime_animation_t *bgRuntimeAnimations = NULL;
/* Source: uo_cgame_mp_x86.dll 0x300a7828 (.data); refs=5 width=4; first=0x30010a20; owner=cg_shakecamera.
 * bg_ammoClipNames[] — clip-type dedup name table; see globals.h. Written by
 * BG_SetupClipIndexes at 0x30010bd4 and seeded [0]="none" by InitWeaponInfo.
 */
const char *bg_ammoClipNames[MAX_AMMO_TYPES];
/* Source: uo_cgame_mp_x86.dll 0x300a7a28 (.data); refs=5 width=4; first=0x3001063f.
 * bg_ammoTypeMax[] — per-ammo-type maximum reserve ammo, indexed by
 * weaponInfo_t::ammoIndex (+0x1e8) / playerState_t::ammo[] index (see globals.h).
 * Filled during ammo-type dedup (BG_SetupAmmoIndexes, 0x30010797:
 * bg_ammoTypeMax[ammoIndex] = weaponInfo->maxAmmo (+0x1f4)); read by
 * BG_GetAmmoTypeMax (thunk 0x30010fb0) and by BG_GetMaxPickupableAmmo
 * (0x300117e6). Mechanical owner=pm_checkjump was only the first-touching
 * function; the export truncated the array to its first element. Signed int32_t
 * (used in signed subtractions and a FIDIV at 0x300317b7). Sized to the ammo-type
 * index domain (playerState ammo[128]); zero-initialized .data, filled at
 * registration time.
 */
int32_t bg_ammoTypeMax[MAX_AMMO_TYPES];
/* Source: uo_cgame_mp_x86.dll 0x300a7c28 (.data); refs=3 width=4; first=0x30010870; owner=cg_vehicleownericon.
 * bg_sharedAmmoCapNames[] — shared-ammo-pool dedup name table; see globals.h. Written
 * and read only by BG_SetupSharedAmmoIndexes (0x300107e0).
 * Example: 30010870   8b 14 ad 28 7c 0a 30         MOV EDX,dword ptr [EBP*0x4 + 0x300a7c28] | 300108d6   8b 14 ad 28 7c 0a 30         MOV EDX,dword ptr [EBP*0x4 + 0x300a7c28] | 3001094a   89 04 ad 28 7c 0a 30         MOV dword ptr [EBP*0x4 + 0x300a7c28],EAX
 */
const char *bg_sharedAmmoCapNames[MAX_AMMO_TYPES];
/* Source: uo_cgame_mp_x86.dll 0x300a7e28 (.data); refs=6 width=4; first=0x30010a06; owner=cg_shakecamera.
 * Example: 30010a06   a1 28 7e 0a 30               MOV EAX,[0x300a7e28] | 30010b45   a1 28 7e 0a 30               MOV EAX,[0x300a7e28] | 30010bc6   3b 0d 28 7e 0a 30            CMP ECX,dword ptr [0x300a7e28]
 */
int32_t bg_numAmmoClips = 0;
/* Source: uo_cgame_mp_x86.dll 0x300a7e2c (.data); refs=3 width=4; first=0x30010856; owner=cg_vehicleownericon.
 * bg_numSharedAmmoCaps — count of distinct shared-ammo pools; see globals.h.
 * Example: 30010856   8b 1d 2c 7e 0a 30            MOV EBX,dword ptr [0x300a7e2c] | 3001093a   8b 1d 2c 7e 0a 30            MOV EBX,dword ptr [0x300a7e2c] | 30010965   89 1d 2c 7e 0a 30            MOV dword ptr [0x300a7e2c],EBX
 */
int32_t bg_numSharedAmmoCaps = 0;
/* Source: uo_cgame_mp_x86.dll 0x300a7e30 (.data); refs=6 width=4; first=0x300105b6; owner=pm_checkjump.
 * Example: 300105b6   a1 30 7e 0a 30               MOV EAX,[0x300a7e30] | 300106f5   a1 30 7e 0a 30               MOV EAX,[0x300a7e30] | 30010776   3b 0d 30 7e 0a 30            CMP ECX,dword ptr [0x300a7e30]
 */
int32_t bg_numAmmoTypes = 0;
/* Source: uo_cgame_mp_x86.dll 0x300a7e34 (.data); refs=5 width=4; first=0x3000fd95.
 * cg_emptyString: cached pointer to a heap-allocated "" (the CopyString empty-source
 * fast-path result). Runtime-initialized to a 1-byte "" buffer by the weapon
 * registration setup at 0x3000ff23; zero at rest. See globals.h for the datum's
 * consumers. (owner=bg_getanimstring was the mechanical first-touch guess.)
 */
const char *cg_emptyString = 0;
/* Source: uo_cgame_mp_x86.dll 0x300a7e38 (.data); refs=4 width=4; first=0x300108a5.
 * bg_sharedAmmoCapSizes[] — per-shared-ammo-group capacity, indexed by
 * weaponInfo_t::sharedAmmoCapIndex (+0x200; see globals.h). Filled during
 * shared-ammo dedup (BG_SetupSharedAmmoIndexes, 0x30010958:
 * bg_sharedAmmoCapSizes[sharedAmmoCapIndex] = weaponInfo->sharedAmmoCap (+0x204));
 * read by BG_GetSharedAmmoCapSize (thunk 0x30010fe0) and by
 * BG_GetMaxPickupableAmmo (0x3001170e), which uses it as the group ammo cap it
 * decrements each owned weapon's held ammo/clips from. Mechanical
 * owner=cg_vehicleownericon was only the first-touching function; the export
 * truncated the array to its first element. Signed int32_t; zero-initialized
 * .data, filled at registration time.
 */
int32_t bg_sharedAmmoCapSizes[MAX_AMMO_TYPES];
/* Source: uo_cgame_mp_x86.dll 0x300a8038 (.data); refs=5 width=4; first=0x300105d0; owner=pm_checkjump.
 * Example: 300105d0   8b 04 8d 38 80 0a 30         MOV EAX,dword ptr [ECX*0x4 + 0x300a8038] | 3001067d   8b 0c 95 38 80 0a 30         MOV ECX,dword ptr [EDX*0x4 + 0x300a8038] | 30010784   89 04 8d 38 80 0a 30         MOV dword ptr [ECX*0x4 + 0x300a8038],EAX
 * bg_ammoTypeNames[] — ammo-type dedup name table; see globals.h. Written by
 * BG_SetupAmmoIndexes at 0x30010784 and seeded [0]="none" by InitWeaponInfo.
 */
const char *bg_ammoTypeNames[MAX_AMMO_TYPES];
/* Source: uo_cgame_mp_x86.dll 0x300a8238 (.data); refs=7; first=0x30010a8f.
 * bg_ammoClipSizes[] — per-clip-index clip capacity table (see globals.h). Written
 * during weapon registration bg_ammoClipSizes[clipIndex] = weaponInfo->clipSize
 * (0x30010be7); read as bg_ammoClipSizes[clipIndex] by BG_GetAmmoClipSize and the
 * reload/ammo predicates. Mechanical owner=cg_shakecamera was only the first
 * function to touch the datum; the mechanical export truncated the array to its
 * first element. Zero-initialized .data (BSS-like); filled at registration time.
 */
int32_t bg_ammoClipSizes[MAX_AMMO_TYPES];
/* Source: uo_cgame_mp_x86.dll 0x300a8438 (.data); the 32-slot rolling per-frame
 * elapsed-ms ring maintained/consumed by CG_DrawFPS (0x30018090). The mechanical
 * export split the base (0x300a8438, the [idx*4+base] store) and its +4 interior
 * (0x300a843c, only ever taken as the &array[1] loop-cursor address) into two dword
 * symbols under the rejected size-guess owner=cg_drawweapreticle; unified here.
 * Example: 300180d6 MOV [EDI*4 + 0x300a8438],ECX | 300180ea MOV EAX,0x300a843c
 */
int32_t cg_statsFrameTimes[32] = {0};
/* Source: uo_cgame_mp_x86.dll 0x300a84b8 (.data); refs=2 width=4; first=0x3001a800.
 * Resolved: last engine-milliseconds tick at which CG_ScreenFade (0x3001a7c0)
 * advanced the cg_fovFade screen-fade animator. Read at 0x3001a800, written at
 * 0x3001a887; deltaMs = trap_Milliseconds() - cg_screenFadeLastMs gates the fade
 * step. The mechanical owner=vectosignedangles was a rejected size-guess mislabel.
 * Example: 3001a800   8b 15 b8 84 0a 30            MOV EDX,dword ptr [0x300a84b8] | 3001a887   89 0d b8 84 0a 30            MOV dword ptr [0x300a84b8],ECX
 */
int32_t cg_screenFadeLastMs = 0;
/* Source: uo_cgame_mp_x86.dll 0x300a84bc (.data); frame-sample counter for the
 * fps ring; owner=cg_drawweapreticle was a rejected size-guess name.
 * Example: 300180aa MOV EAX,[0x300a84bc] | 300180dd MOV [0x300a84bc],EAX
 */
int32_t cg_statsFrameCount = 0;
/* Source: uo_cgame_mp_x86.dll 0x300a84c0 (.data); previous-frame trap_Milliseconds
 * snapshot used by CG_DrawFPS; owner=cg_drawweapreticle rejected.
 * Example: 3001809d MOV EDX,[0x300a84c0] | 300180a5 MOV [0x300a84c0],EAX
 */
int32_t cg_statsPrevTimeMs = 0;
/* Source: uo_cgame_mp_x86.dll 0x300a84c4..0x300a84e4 (.data); the renderer
 * performance-counter block cg_rendererStats (renderer_frame_statistics_t) filled via
 * cgame_syscall(CG_R_TRACK_STATISTICS, &cg_rendererStats) and printed by CG_DrawFPS. The
 * mechanical export split this one struct into nine owner=cg_drawweapreticle dwords
 * (a rejected size-guess name); unified here into the typed struct.
 */
renderer_frame_statistics_t cg_rendererStats = {0};
/* Source: uo_cgame_mp_x86.dll 0x300a84e8..0x300a84f4 (.data/.bss); the 4-float
 * static return buffer of CG_FadeColor (0x3001d200). RGB (0x300a84e8/ec/f0) are
 * set to 1.0 and alpha (0x300a84f4) to the computed fade fraction; the function
 * returns &cg_fadeColor[0] (EAX=0x300a84e8). The mechanical export split this one
 * vec4_t into four owner=pm_addtouchent dwords (a size-match misname, rejected);
 * unified here. Example: 3001d24b MOV [0x300a84e8],0x3f800000 (=1.0f) |
 * 3001d255 MOV EAX,0x300a84e8 | 3001d25a FSTP float ptr [0x300a84f4]. */
vec4_t cg_fadeColor = { 0.0f, 0.0f, 0.0f, 0.0f };
/* Source: uo_cgame_mp_x86.dll 0x300a84f8..0x300a8503 (.data) — cg_flameSpriteSrcRight vec3.
 * Written by CG_FireFlameChunks (0x30027dbd/dc7/dd3) = cg_refdef.viewaxis[1]; read by
 * CG_AddFlameSpriteToScene (0x30026de7/ded/df2). Retyped from three uint32_t placeholders. */
vec3_t cg_flameSpriteSrcRight = { 0.0f, 0.0f, 0.0f };
/* Source: uo_cgame_mp_x86.dll 0x300a8508 (.data); refs=3 width=4; first=0x30027119; owner=cg_addflamespritetoscene.
 * Example: 30027119   8b 04 8d 08 85 0a 30         MOV EAX,dword ptr [ECX*0x4 + 0x300a8508] | 3002723d   8b 14 b5 08 85 0a 30         MOV EDX,dword ptr [ESI*0x4 + 0x300a8508] | 30027a24   89 04 9d 08 85 0a 30         MOV dword ptr [EBX*0x4 + 0x300a8508],EAX
 * Retyped to the flamethrower-fire material handle table cg_flameFireMaterials[43]
 * (0x300a8508..0x300a85b4, 43 dwords). Filled at runtime by CG_InitFlameChunks; the
 * mechanical export only carved the leading dword, the rest of the .data span is
 * zero-initialized and not separately exported. */
qhandle_t cg_flameFireMaterials[43];
/* Source: uo_cgame_mp_x86.dll 0x300a85b4 (.data); refs=5 width=4; first=0x30025592; owner=hudelem_destroyall.
 * Example: 30025592   89 2d b4 85 0a 30            MOV dword ptr [0x300a85b4],EBP | 30025600   a1 b4 85 0a 30               MOV EAX,[0x300a85b4] | 30025624   89 3d b4 85 0a 30            MOV dword ptr [0x300a85b4],EDI
 */
/* RESOLVED: head of the flame-chunk free list (see globals.h). */
/* Source: uo_cgame_mp_x86.dll 0x300a85b4. */
flameChunk_t *cg_freeFlameChunks = NULL;
/* Source: uo_cgame_mp_x86.dll 0x300a85b8..0x300a85e7 (.data) — cg_flameDamageTrace, the
 * module-static 48-byte flame-damage-source trace record. Supersedes the ten mechanical
 * dword aliases the exporter carved out of this one object
 * (0x300a85b8/bc/c0/c4/c8/cc/d0/d4/e0/e7), all mis-labeled
 * owner=cg_playerturretpositionandblend (the mis-sized first-touching function). Filled by
 * CG_FlamethrowerTrace and consumed by CG_MoveFlameChunk (0x30025da0). Zero-init. */
trace_t cg_flameDamageTrace;
/* Source: uo_cgame_mp_x86.dll 0x300a85e8..0x300a86f8 (.data) — r_fullscreen
 * vmCvar mirror. Its address is passed to cvar register/update traps; the read at
 * 0x300a85f4 is its .integer field at +0x0c. */
vmCvar_t cg_rFullscreenCvar;
/* Source: uo_cgame_mp_x86.dll 0x300a86f8 (.data); refs=2 width=4; first=0x30027a54; owner=bg_finditem.
 * Example: 30027a54   89 04 9d f8 86 0a 30         MOV dword ptr [EBX*0x4 + 0x300a86f8],EAX | 30028e5f   8b 0c 85 f8 86 0a 30         MOV ECX,dword ptr [EAX*0x4 + 0x300a86f8]
 * Retyped to the flame-smoke material handle table cg_flameSmokeMaterials[6]
 * (0x300a86f8..0x300a8710, 6 dwords). Filled by CG_InitFlameChunks. */
qhandle_t cg_flameSmokeMaterials[6];
/* Source: uo_cgame_mp_x86.dll 0x300a8710 (.data); refs=2 width=4; first=0x3002666f.
 * cg_flameDamageBestPosTime — the cg_flameTime stamp at which cg_flameDamageBestPos was
 * last recorded (see globals.h). RESOLVED from CG_FlameDamage (0x300265c0). The
 * mechanical owner=item_listbox_overlb was the first-touching function, not the identity.
 * Example: 3002666f CMP EBX,[0x300a8710] | 300266b0 MOV [0x300a8710],EBX */
int32_t cg_flameDamageBestPosTime = 0;
/* Source: uo_cgame_mp_x86.dll 0x300a8718..0x300ab718 (.data).
 * cg_flameSoundLoops[1024] — flame sound-loop envelope table, stride 12 bytes.
 * Supersedes the three mechanical dword aliases (0x300a8718/871c/8720) the exporter
 * carved out of slot[0]. Every access strides it by 12 (index*3 dwords): the
 * "g_radiusdamage" owner FLD/FADD/FSTPs envA at [ECX*4 + 0x300a8718]
 * (0x30027405/748d/7494); CG_UpdateFlamethrowerSounds strides envA/envB/frameOwner by
 * flameChunk_t.field_34 and walks all 1024 in its decay pass. See cgFlameSoundLoop_t
 * in client_recovered.h. */
struct cgFlameSoundLoop_s cg_flameSoundLoops[1024];
/* Source: uo_cgame_mp_x86.dll 0x300ab718 (.data); refs=46 width=4; first=0x30023d10.
 * RESOLVED: cg_flameTime — the flame subsystem's per-frame time base, computed as
 * round(2 * cg_time) by CG_UpdateFlameTime (0x30023af0) and inlined at the head of
 * CG_AddFlameChunks (0x300272b0) and CG_AddFlameToScene (0x300268e0), and written
 * as `(uint32_t)flameTime` by CG_UpdateFlamethrowerSounds (0x30029210). Read via FILD as
 * an int timestamp by the flame chunk clock consumers (and by the per-frame chunk
 * driver CG_UpdateFlameChunk, which shares this flame clock). The mechanical owner
 * label "cg_playadsanim" was the first-touching function, not the identity. */
uint32_t cg_flameTime = (uint32_t)0x00000000u;
/* Source: uo_cgame_mp_x86.dll 0x300ab71c (.data); refs=5 width=4; first=0x30026dcc; owner=cg_addflamespritetoscene.
 * Example: 30026dcc   bb 1c b7 0a 30               MOV EBX,0x300ab71c | 30026e1a   89 15 1c b7 0a 30            MOV dword ptr [0x300ab71c],EDX | 30026e41   d9 05 1c b7 0a 30            FLD float ptr [0x300ab71c]
 */
vec3_t cg_flameSpriteUp = {0.0f, 0.0f, 0.0f};
/* Source: uo_cgame_mp_x86.dll 0x300ab720 (.data); refs=4 width=4; first=0x30026e20; owner=cg_addflamespritetoscene.
 * Example: 30026e20   a3 20 b7 0a 30               MOV [0x300ab720],EAX | 30026e73   d9 05 20 b7 0a 30            FLD float ptr [0x300ab720] | 30026ef1   d8 0d 20 b7 0a 30            FMUL float ptr [0x300ab720]
 */
/* Source: uo_cgame_mp_x86.dll 0x300ab724 (.data); refs=4 width=4; first=0x30026e25; owner=cg_addflamespritetoscene.
 * Example: 30026e25   89 0d 24 b7 0a 30            MOV dword ptr [0x300ab724],ECX | 30026e7f   d9 05 24 b7 0a 30            FLD float ptr [0x300ab724] | 30026efd   d8 0d 24 b7 0a 30            FMUL float ptr [0x300ab724]
 */
/* Source: uo_cgame_mp_x86.dll 0x300ab728 (.data); refs=6 width=4; first=0x30025598; owner=hudelem_destroyall.
 * Example: 30025598   89 3d 28 b7 0a 30            MOV dword ptr [0x300ab728],EDI | 3002562f   8b 3d 28 b7 0a 30            MOV EDI,dword ptr [0x300ab728] | 30025640   a3 28 b7 0a 30               MOV [0x300ab728],EAX
 */
/* RESOLVED: head of the flame-chunk active list (see globals.h). */
/* Source: uo_cgame_mp_x86.dll 0x300ab728. */
flameChunk_t *cg_activeFlameChunks = NULL;
/* Source: uo_cgame_mp_x86.dll 0x300ab72c (.data); refs=4 width=4; first=0x30026dd1; owner=cg_addflamespritetoscene.
 * Example: 30026dd1   bf 2c b7 0a 30               MOV EDI,0x300ab72c | 30026df8   89 15 2c b7 0a 30            MOV dword ptr [0x300ab72c],EDX | 30026e91   d8 0d 2c b7 0a 30            FMUL float ptr [0x300ab72c]
 */
vec3_t cg_flameSpriteRight = {0.0f, 0.0f, 0.0f};
/* Source: uo_cgame_mp_x86.dll 0x300ab730 (.data); refs=3 width=4; first=0x30026e04; owner=cg_addflamespritetoscene.
 * Example: 30026e04   a3 30 b7 0a 30               MOV [0x300ab730],EAX | 30026ea7   d8 0d 30 b7 0a 30            FMUL float ptr [0x300ab730] | 30026f32   d8 0d 30 b7 0a 30            FMUL float ptr [0x300ab730]
 */
/* Source: uo_cgame_mp_x86.dll 0x300ab734 (.data); refs=3 width=4; first=0x30026e0e; owner=cg_addflamespritetoscene.
 * Example: 30026e0e   89 0d 34 b7 0a 30            MOV dword ptr [0x300ab734],ECX | 30026eb5   d8 0d 34 b7 0a 30            FMUL float ptr [0x300ab734] | 30026f4d   d8 0d 34 b7 0a 30            FMUL float ptr [0x300ab734]
 */
/* Source: uo_cgame_mp_x86.dll 0x300ab738 (.data); refs=1 width=4; first=0x30027260; owner=cg_addflamespritetoscene.
 * Example: 30027260   a3 38 b7 0a 30               MOV [0x300ab738],EAX
 */
vec3_t cg_flameLastSpritePos = {0.0f, 0.0f, 0.0f};
/* Source: uo_cgame_mp_x86.dll 0x300ab73c (.data); refs=1 width=4; first=0x3002726c; owner=cg_addflamespritetoscene.
 * Example: 3002726c   89 0d 3c b7 0a 30            MOV dword ptr [0x300ab73c],ECX
 */
/* Source: uo_cgame_mp_x86.dll 0x300ab740 (.data); refs=1 width=4; first=0x30027279; owner=cg_addflamespritetoscene.
 * Example: 30027279   89 15 40 b7 0a 30            MOV dword ptr [0x300ab740],EDX
 */
/* Source: uo_cgame_mp_x86.dll 0x300ab744..0x300ab74f (.data) — cg_flameSpriteSrcUp vec3.
 * Written by CG_FireFlameChunks (0x30027ddf/de4/dea) = cg_refdef.viewaxis[2]; read by
 * CG_AddFlameSpriteToScene (0x30026dfe/e09/e14). Retyped from three uint32_t placeholders. */
vec3_t cg_flameSpriteSrcUp = { 0.0f, 0.0f, 0.0f };
/* Source: uo_cgame_mp_x86.dll 0x300ab750 (.data); refs=9 width=4; first=0x300240a3; owner=cg_addflamechunks.
 * Example: 300240a3   81 c5 50 b7 0a 30            ADD EBP,0x300ab750 | 30025589   bf 50 b7 0a 30               MOV EDI,0x300ab750 | 30025e36   8b b0 50 b7 0a 30            MOV ESI,dword ptr [EAX + 0x300ab750]
 */
/* RESOLVED 0x300ab750: base of the per-owner flame-info region, FLAME_INFO_COUNT elements
 * of FLAME_INFO_SIZE bytes (see globals.h). CG_ClearFlameChunks zeroes it all. */
cgFlameInfo_t cg_flameInfo[FLAME_INFO_COUNT];
/* Source: uo_cgame_mp_x86.dll 0x300d9750 (.data); refs=7 width=4; first=0x3002559e; owner=hudelem_destroyall.
 * Example: 3002559e   89 3d 50 97 0d 30            MOV dword ptr [0x300d9750],EDI | 3002560e   8b 15 50 97 0d 30            MOV EDX,dword ptr [0x300d9750] | 300256c3   a3 50 97 0d 30               MOV [0x300d9750],EAX
 */
/* RESOLVED: head of the secondary flame-chunk list (see globals.h). */
/* Source: uo_cgame_mp_x86.dll 0x300d9750. */
flameChunk_t *cg_flameChunkList = NULL;
/* Source: uo_cgame_mp_x86.dll 0x300d9754 (.data); refs=2 width=4; first=0x30029232; owner=cg_updateflamethrowersounds.
 * Example: 30029232   8b 0d 54 97 0d 30            MOV ECX,dword ptr [0x300d9754] | 3002925f   a3 54 97 0d 30               MOV [0x300d9754],EAX
 * RESOLVED cg_flameSoundsPrevTime: cg.time of the previous CG_UpdateFlamethrowerSounds run.
 * Retyped to int32_t (signed time). Both references are that function's read/write.
 */
int32_t cg_flameSoundsPrevTime = 0;
/* Source: uo_cgame_mp_x86.dll 0x300d9758 (.data); refs=4 width=4; first=0x300265c0.
 * cg_flameDamageTakenThisFrame — once-per-frame local flame-damage flag (see globals.h).
 * RESOLVED from CG_FlameDamage (0x300265c0) and the per-frame flame updater
 * (cleared 0x300290ef, forwarded via trap 0x58 at 0x30029192). The mechanical
 * owner=item_listbox_overlb was the first toucher, not the identity.
 * Example: 300265c0 MOV EAX,[0x300d9758] | 300268c6 MOV [0x300d9758],1 | 300290ef MOV [0x300d9758],EBP */
int32_t cg_flameDamageTakenThisFrame = 0;
/* Source: uo_cgame_mp_x86.dll 0x300d975c (.data); refs=5 width=4; first=0x300255e6; owner=hudelem_destroyall.
 * Example: 300255e6   89 3d 5c 97 0d 30            MOV dword ptr [0x300d975c],EDI | 300256b9   8b 0d 5c 97 0d 30            MOV ECX,dword ptr [0x300d975c] | 300256cb   89 0d 5c 97 0d 30            MOV dword ptr [0x300d975c],ECX
 */
/* RESOLVED: count of active flame chunks (see globals.h). */
/* Source: uo_cgame_mp_x86.dll 0x300d975c. */
int32_t cg_numActiveFlameChunks = 0;
/* Source: uo_cgame_mp_x86.dll 0x300d9760..0x300d976b (.data); refs=2+1+1 width=4; first=0x30026677.
 * cg_flameDamageBestPos — world position of the flame source that damaged the local player
 * this flame-frame (see globals.h). RESOLVED from CG_FlameDamage (0x300265c0);
 * supersedes the three mechanical dword aliases (0x300d9760/64/68) into one vec3. The
 * mechanical owner=item_listbox_overlb was the first toucher, not the identity.
 * Example: 30026694 MOV [0x300d9760],EAX | 3002669c MOV [0x300d9764],ECX | 300266a5 MOV [0x300d9768],EDX */
vec3_t cg_flameDamageBestPos = { 0.0f, 0.0f, 0.0f };
/* Source: uo_cgame_mp_x86.dll 0x300d9770..0x300d9880 (.data) — r_overbrightbits
 * vmCvar mirror. 0x300d977c is its .integer at +0x0c. The comparison immediate
 * 0x300d97b0 in CG_UpdateFlamethrowerSounds is independently a one-past cursor
 * for cg_flameInfo[][+0x60], not a source object boundary. */
vmCvar_t cg_rOverbrightBitsCvar;
/* Source: uo_cgame_mp_x86.dll 0x300d9880 (.data); refs=3 width=4; first=0x30026d9b; owner=cg_addflamespritetoscene.
 * Example: 30026d9b   a1 80 98 0d 30               MOV EAX,[0x300d9880] | 30026da6   a3 80 98 0d 30               MOV [0x300d9880],EAX | 30027df0   89 2d 80 98 0d 30            MOV dword ptr [0x300d9880],EBP
 */
uint32_t cg_flameDamageBillboardCount = (uint32_t)0x00000000u;
/* Source: uo_cgame_mp_x86.dll 0x300d9888 (.data); the 1024-byte static result
 * buffer of CG_SafeTranslateString_Internal (0x3002d6e0) — 0x300d9c88 is the next .data symbol.
 * The mechanical export split it into five owner=script_method_scriptbuiltin_sethin
 * dword/byte symbols (…888/…88c/…890/…894/…898) that were merely the offsets 0..0x10
 * written by the inlined 17-byte prefix strcpy; collapsed here to the real array.
 * Example: 3002d741   a3 88 98 0d 30               MOV [0x300d9888],EAX | 3002d777   bf 88 98 0d 30               MOV EDI,0x300d9888 | 3002d798   bf 88 98 0d 30               MOV EDI,0x300d9888
 */
char cg_translatedString[MAX_STRING_CHARS];
/* Source: uo_cgame_mp_x86.dll 0x300d9c88..0x300da487 (.data).
 * CG_TranslateMessage selects one 1024-byte half with toggle<<10. */
char cg_translateMessageBuffers[2][MAX_STRING_CHARS];
/* Source: uo_cgame_mp_x86.dll 0x300da488 (.data); refs=60. A shared 1024-byte text
 * scratch buffer, superseding the mechanical single-byte capture (width=1). Sized
 * from CG_ConsoleCommand's trap_Argv(0, buf, 0x400) at 0x300178c1/0x300178c6, then
 * read as a C string by Q_stricmpn; many other sites (e.g. 0x3002b4d5) reuse it as
 * a formatting/argv workspace. owner=script_func_vectordot is the wrong first-toucher label.
 */
char g_textScratchBuffer[MAX_STRING_CHARS] = { 0 };
/* Source: uo_cgame_mp_x86.dll 0x300da888..0x300db888 (.data); 4096-byte text buffer.
 * cg_menuListText — the buffer CG_LoadMenus (0x3002d2d0) reads a "loadmenu { ... }"
 * menu-list file into (CG_FS_READ), NUL-terminates, Com_Compress()es, and parses in
 * place with Com_Parse. Size 4096 = the file-length guard (0x1000) and the stride to
 * the next distinct global at 0x300db888. The mechanical export captured only the
 * first dword (width=1/4) with the wrong owner label bg_setupweaponalts (a rejected
 * size-guess for CG_LoadMenus); superseded here by the real char[4096] buffer. */
char cg_menuListText[MAX_MENULIST_FILE] = { 0 };
/* Source: uo_cgame_mp_x86.dll 0x300db888..0x300dbc88 (.data); 1024-byte result buffer.
 * cg_translatedLocationString — the static buffer CG_GetTranslatedLocationString
 * (0x300310b0) builds an untranslated location name into. The mechanical export
 * split the inlined 17-byte prefix strcpy into five per-dword/byte aliases
 * (g_data_menus_removefromstack_300db888/88c/890/894/898); those are one buffer and
 * are superseded here by the proven array shape (owner=menus_removefromstack was a
 * size-match first-toucher label). */
char cg_translatedLocationString[MAX_STRING_CHARS] = { 0 };
/* Source: uo_cgame_mp_x86.dll 0x300dbc88 (.data); 256-entry trigger-entity list
 * built by CG_BuildSolidList and consumed by CG_TouchTriggerPrediction.
 * Example: 3003508a   89 04 9d 88 bc 0d 30         MOV dword ptr [EBX*0x4 + 0x300dbc88],EAX | 30035750   8b 3c b5 88 bc 0d 30         MOV EDI,dword ptr [ESI*0x4 + 0x300dbc88]
 */
centity_t *cg_triggerEntities[256] = { 0 };
/* Source: uo_cgame_mp_x86.dll 0x300dc088 (.data); refs=4 width=4; first=0x3003504e; owner=concussive_fx.
 * Valid-pointer count for cg_triggerEntities[].
 * Example: 3003504e   89 1d 88 c0 0d 30            MOV dword ptr [0x300dc088],EBX | 300350b7   89 1d 88 c0 0d 30            MOV dword ptr [0x300dc088],EBX | 3003573d   a1 88 c0 0d 30               MOV EAX,[0x300dc088]
 */
int32_t cg_numTriggerEntities = 0;
/* Source: uo_cgame_mp_x86.dll 0x300dc08c (.data); refs=6 width=4; first=0x30035048.
 * Valid-pointer count for cg_solidEntities[].
 * Example: 30035048   89 2d 8c c0 0d 30   MOV [0x300dc08c],EBP | 300350d3 a1 8c c0 0d 30 MOV EAX,[0x300dc08c] */
int32_t cg_numSolidEntities = 0;
/* Source: uo_cgame_mp_x86.dll 0x300dc090 (.data); refs=3 width=4; first=0x30035098.
 * Per-frame 256-entry solid-entity collision list.
 * Example: 30035098 89 04 ad 90 c0 0d 30 MOV [EBP*4 + 0x300dc090],EAX | 30035443 8b 34 bd 90 c0 0d 30 MOV ESI,[EDI*4 + 0x300dc090] */
centity_t *cg_solidEntities[256] = { 0 };
/* Source: uo_cgame_mp_x86.dll 0x300dc490 (.data); refs=11 width=4; first=0x30035865; owner=pmovesingle.
 * Example: 30035865   c7 05 90 c4 0d 30 c4 31 48 30 MOV dword ptr [0x300dc490],0x304831c4 | 30035898   8b 15 90 c4 0d 30            MOV EDX,dword ptr [0x300dc490] | 3003595f   a1 90 c4 0d 30               MOV EAX,[0x300dc490]
 */
pmove_t cg_pmove = {0};
/* Source: uo_cgame_mp_x86.dll 0x300dc494 (.data); refs=7 width=4; first=0x30035aa2; owner=pmovesingle.
 * Example: 30035aa2   68 94 c4 0d 30               PUSH 0x300dc494 | 30035ac6   68 94 c4 0d 30               PUSH 0x300dc494 | 30035ad3   a1 94 c4 0d 30               MOV EAX,[0x300dc494]
 */
/* Source: uo_cgame_mp_x86.dll 0x300dc498 (.data); refs=1 width=1; first=0x30035d89; owner=pmovesingle.
 * Example: 30035d89   c6 05 98 c4 0d 30 00         MOV byte ptr [0x300dc498],0x0
 */
/* Source: uo_cgame_mp_x86.dll 0x300dc499 (.data); refs=2 width=1; first=0x30035d5d; owner=pmovesingle.
 * Example: 30035d5d   8a 1d 99 c4 0d 30            MOV BL,byte ptr [0x300dc499] | 30035da5   88 1d 99 c4 0d 30            MOV byte ptr [0x300dc499],BL
 */
/* Source: uo_cgame_mp_x86.dll 0x300dc49c (.data); refs=1 width=4; first=0x30035d69; owner=pmovesingle.
 * Example: 30035d69   89 0d 9c c4 0d 30            MOV dword ptr [0x300dc49c],ECX
 */
/* Source: uo_cgame_mp_x86.dll 0x300dc4a0 (.data); refs=1 width=4; first=0x30035dab; owner=pmovesingle.
 * Example: 30035dab   89 15 a0 c4 0d 30            MOV dword ptr [0x300dc4a0],EDX
 */
/* Source: uo_cgame_mp_x86.dll 0x300dc4a4 (.data); refs=1 width=4; first=0x30035d75; owner=pmovesingle.
 * Example: 30035d75   89 0d a4 c4 0d 30            MOV dword ptr [0x300dc4a4],ECX
 */
/* Source: uo_cgame_mp_x86.dll 0x300dc4a8 (.data); refs=1 width=1; first=0x30035d90; owner=pmovesingle.
 * Example: 30035d90   c6 05 a8 c4 0d 30 00         MOV byte ptr [0x300dc4a8],0x0
 */
/* Source: uo_cgame_mp_x86.dll 0x300dc4a9 (.data); refs=1 width=1; first=0x30035d97; owner=pmovesingle.
 * Example: 30035d97   c6 05 a9 c4 0d 30 00         MOV byte ptr [0x300dc4a9],0x0
 */
/* Source: uo_cgame_mp_x86.dll 0x300dc4aa (.data); refs=1 width=1; first=0x30035d9e; owner=pmovesingle.
 * Example: 30035d9e   c6 05 aa c4 0d 30 00         MOV byte ptr [0x300dc4aa],0x0
 */
/* Source: uo_cgame_mp_x86.dll 0x300dc4ac (.data); refs=1 width=imm; first=0x30035aee; owner=pmovesingle.
 * Example: 30035aee   68 ac c4 0d 30               PUSH 0x300dc4ac
 */
/* Source: uo_cgame_mp_x86.dll 0x300dc4b4 (.data); refs=1 width=4; first=0x30035d57; owner=pmovesingle.
 * Example: 30035d57   8b 0d b4 c4 0d 30            MOV ECX,dword ptr [0x300dc4b4]
 */
/* Source: uo_cgame_mp_x86.dll 0x300dc4b8 (.data); refs=1 width=4; first=0x30035d63; owner=pmovesingle.
 * Example: 30035d63   8b 15 b8 c4 0d 30            MOV EDX,dword ptr [0x300dc4b8]
 */
/* Source: uo_cgame_mp_x86.dll 0x300dc4bc (.data); refs=1 width=4; first=0x30035d6f; owner=pmovesingle.
 * Example: 30035d6f   8b 0d bc c4 0d 30            MOV ECX,dword ptr [0x300dc4bc]
 */
/* Source: uo_cgame_mp_x86.dll 0x300dc4c4 (.data); refs=3 width=4; first=0x300358c3; owner=pmovesingle.
 * Example: 300358c3   89 0d c4 c4 0d 30            MOV dword ptr [0x300dc4c4],ECX | 300358cf   81 25 c4 c4 0d 30 ff ff fe fd AND dword ptr [0x300dc4c4],0xfdfeffff | 30035e1a   8b 15 c4 c4 0d 30            MOV EDX,dword ptr [0x300dc4c4]
 */
/* Source: uo_cgame_mp_x86.dll 0x300dc4cc (.data); refs=2 width=4; first=0x3003597e; owner=pmovesingle.
 * Example: 3003597e   c7 05 cc c4 0d 30 00 00 00 00 MOV dword ptr [0x300dc4cc],0x0 | 300359e3   89 15 cc c4 0d 30            MOV dword ptr [0x300dc4cc],EDX
 */
/* Source: uo_cgame_mp_x86.dll 0x300dc4d0 (.data); refs=2 width=4; first=0x30035974; owner=pmovesingle.
 * Example: 30035974   c7 05 d0 c4 0d 30 00 00 00 00 MOV dword ptr [0x300dc4d0],0x0 | 300359fb   89 15 d0 c4 0d 30            MOV dword ptr [0x300dc4d0],EDX
 */
/* Source: uo_cgame_mp_x86.dll 0x300dc4d4 (.data); refs=2 width=4; first=0x3003596a; owner=pmovesingle.
 * Example: 3003596a   c7 05 d4 c4 0d 30 00 00 00 00 MOV dword ptr [0x300dc4d4],0x0 | 30035a13   89 15 d4 c4 0d 30            MOV dword ptr [0x300dc4d4],EDX
 */
/* Source: uo_cgame_mp_x86.dll 0x300dc4d8 (.data); refs=2 width=4; first=0x3003599c; owner=pmovesingle.
 * Example: 3003599c   c7 05 d8 c4 0d 30 00 00 00 00 MOV dword ptr [0x300dc4d8],0x0 | 30035a35   c7 05 d8 c4 0d 30 00 00 70 42 MOV dword ptr [0x300dc4d8],0x42700000
 */
/* Source: uo_cgame_mp_x86.dll 0x300dc4dc (.data); refs=2 width=4; first=0x30035992; owner=pmovesingle.
 * Example: 30035992   c7 05 dc c4 0d 30 00 00 00 00 MOV dword ptr [0x300dc4dc],0x0 | 30035a2b   c7 05 dc c4 0d 30 00 00 8c 42 MOV dword ptr [0x300dc4dc],0x428c0000
 */
/* Source: uo_cgame_mp_x86.dll 0x300dc4e0 (.data); refs=1 width=4; first=0x30035988; owner=pmovesingle.
 * Example: 30035988   c7 05 e0 c4 0d 30 00 00 00 00 MOV dword ptr [0x300dc4e0],0x0
 */
/* Source: uo_cgame_mp_x86.dll 0x300dc568 (.data); refs=1 width=imm; first=0x30035792; owner=snapvectortowards.
 * Example: 30035792   68 68 c5 0d 30               PUSH 0x300dc568
 */
/* Source: uo_cgame_mp_x86.dll 0x300dc574 (.data); refs=1 width=imm; first=0x3003578d; owner=snapvectortowards.
 * Example: 3003578d   68 74 c5 0d 30               PUSH 0x300dc574
 */
/* Source: uo_cgame_mp_x86.dll 0x300dc588 (.data); refs=3 width=4; first=0x30035a8d; owner=pmovesingle.
 * Example: 30035a8d   a3 88 c5 0d 30               MOV [0x300dc588],EAX | 30035ab0   a1 88 c5 0d 30               MOV EAX,[0x300dc588] | 30035d2a   a1 88 c5 0d 30               MOV EAX,[0x300dc588]
 */
/* Source: uo_cgame_mp_x86.dll 0x300dc58c (.data); refs=1 width=4; first=0x30035a92; owner=pmovesingle.
 * Example: 30035a92   89 0d 8c c5 0d 30            MOV dword ptr [0x300dc58c],ECX
 */
/* Source: uo_cgame_mp_x86.dll 0x300dc594 (.data); refs=1 width=4; first=0x3003586f; owner=pmovesingle.
 * Example: 3003586f   a3 94 c5 0d 30               MOV [0x300dc594],EAX
 */
/* Source: uo_cgame_mp_x86.dll 0x300dc598 (.data); refs=1 width=4; first=0x30035874; owner=pmovesingle.
 * Example: 30035874   a3 98 c5 0d 30               MOV [0x300dc598],EAX
 */
/* Source: uo_cgame_mp_x86.dll 0x300dc59c (.data); refs=1 width=4; first=0x30035879; owner=pmovesingle.
 * Example: 30035879   a3 9c c5 0d 30               MOV [0x300dc59c],EAX
 */
/* Source: uo_cgame_mp_x86.dll 0x300dc5a0 (.data); refs=1 width=4; first=0x3003587e; owner=pmovesingle.
 * Example: 3003587e   c7 05 a0 c5 0d 30 20 54 03 30 MOV dword ptr [0x300dc5a0],0x30035420
 */
/* Source: uo_cgame_mp_x86.dll 0x300dc5a4 (.data); refs=1 width=4; first=0x30035888; owner=pmovesingle.
 * Example: 30035888   c7 05 a4 c5 0d 30 d0 57 03 30 MOV dword ptr [0x300dc5a4],0x300357d0
 */
/* Source: uo_cgame_mp_x86.dll 0x300dc5a8 (.data); refs=1 width=4; first=0x300358a0; owner=pmovesingle.
 * Example: 300358a0   a3 a8 c5 0d 30               MOV [0x300dc5a8],EAX
 */
/* Source: uo_cgame_mp_x86.dll 0x300dc5b0..0x300dc9b0 (.data). 1024-byte static
 * result buffer for CG_GetTranslatedVoiceChatString (0x3003a150). The function's
 * inline copies are unbounded; the next global proves the extent. The mechanical
 * export previously split this one buffer into five
 * dword/byte symbols (…b0/…b4/…b8/…bc/…c0, first=0x3003a1ad) which were really
 * stores into offsets 0/4/8/0xc/0x10 during the inlined strcpy of the constant
 * prefix; superseded here as the single buffer. */
char cg_translatedVoiceChatString[MAX_STRING_CHARS] = { 0 };
/* Source: uo_cgame_mp_x86.dll 0x300dc9b0 (.data); owner=CG_CalcVehicleViewPos (0x30040810;
 * the mechanical owner=pm_slidemove was that function's rejected size-guess name).
 * Example: 3004109d   89 15 b0 c9 0d 30            MOV dword ptr [0x300dc9b0],cg_time
 */
int32_t cg_vehicleViewSwayPrevTime = 0;
/* Source: uo_cgame_mp_x86.dll 0x300dc9b4/b8/bc (.data); owner=CG_CalcVehicleViewPos.
 * The smoothed vehicle-view aim origin; three consecutive floats (consolidated from the
 * mechanical per-dword pm_slidemove symbols).
 * Example: 30041097 FSTP [0x300dc9b4] | 30041190 FSTP [0x300dc9b8] | 300411a4 MOV EAX,[0x300dc9b4]
 */
vec3_t cg_vehicleViewSwayOrigin = { 0.0f, 0.0f, 0.0f };
/* Source: uo_cgame_mp_x86.dll 0x300dc9c0 (.data); previous view angles passed
 * on the stack to BG_CalculateWeaponPosition_Sway at 0x30041796.
 * Example: 30041796   68 c0 c9 0d 30               PUSH 0x300dc9c0
 */
vec3_t cg_turretViewSwayPreviousViewAngles = { 0.0f, 0.0f, 0.0f };
/* Source: uo_cgame_mp_x86.dll 0x300dc9cc (.data); refs=1 width=imm; first=0x3004179b; owner=g_moverpush.
 * Example: 3004179b   bf cc c9 0d 30               MOV EDI,0x300dc9cc
 */
vec2_t cg_turretViewSwayViewAngles = { 0.0f, 0.0f };
/* Source: uo_cgame_mp_x86.dll 0x300dc9d8 (.data); refs=4 width=4; first=0x300400f6; owner=bg_parseweaponinfospecificfieldtyp.
 * Example: 300400f6   39 35 d8 c9 0d 30            CMP dword ptr [0x300dc9d8],ESI | 30040120   89 35 d8 c9 0d 30            MOV dword ptr [0x300dc9d8],ESI | 30040200   a1 d8 c9 0d 30               MOV EAX,[0x300dc9d8]
 */
int32_t cg_fovLastVehiclePosition = 0; /* 0x300dc9d8; see globals.h. CG_CalcFov FOV-zoom state. */
/* Source: uo_cgame_mp_x86.dll 0x300dc9dc/e0/e4 (.data); owner=CG_CalcVehicleViewPos.
 * BG_CalculateWeaponPosition_Sway positional output (EAX at 0x30040977); read back as float
 * scalars in the thompson-sway blend arm. Consolidated from the per-dword pm_slidemove symbols.
 * Example: 30040977 MOV EAX,0x300dc9dc | 30040b2a FMUL [0x300dc9dc] | 30040be7 FLD [0x300dc9e4]
 */
vec3_t cg_vehicleViewSwayOffset = { 0.0f, 0.0f, 0.0f };
/* Source: uo_cgame_mp_x86.dll 0x300dc9e8/ec/f0 (.data); owner=CG_CalcVehicleViewPos.
 * Previous view angles passed on the stack to BG_CalculateWeaponPosition_Sway
 * (address at 0x3004096d).
 * Example: 3004096d   68 e8 c9 0d 30               PUSH 0x300dc9e8
 */
vec3_t cg_vehicleViewSwayPreviousViewAngles = { 0.0f, 0.0f, 0.0f };
/* Source: uo_cgame_mp_x86.dll 0x300dc9f4 (.data); refs=5 width=4; first=0x300400ea; owner=bg_parseweaponinfospecificfieldtyp.
 * Example: 300400ea   89 15 f4 c9 0d 30            MOV dword ptr [0x300dc9f4],EDX | 30040113   89 15 f4 c9 0d 30            MOV dword ptr [0x300dc9f4],EDX | 30040234   89 15 f4 c9 0d 30            MOV dword ptr [0x300dc9f4],EDX
 */
int32_t cg_fovTransitionTime = 0; /* 0x300dc9f4; see globals.h. CG_CalcFov FOV-zoom state (holds -1 when idle). */
/* Source: uo_cgame_mp_x86.dll 0x300dc9f8/fc (.data); owner=CG_CalcVehicleViewPos.
 * BG_CalculateWeaponPosition_Sway out_angles vec2 output buffer (address at 0x30040972).
 * Example: 30040972   bf f8 c9 0d 30               MOV EDI,0x300dc9f8
 */
vec2_t cg_vehicleViewSwayViewAngles = { 0.0f, 0.0f };
/* Source: uo_cgame_mp_x86.dll 0x300dca04 (.data); positional sway output passed
 * in EAX to BG_CalculateWeaponPosition_Sway at 0x300417a0.
 * Example: 300417a0   b8 04 ca 0d 30               MOV EAX,0x300dca04 | 300418ab   d8 0d 04 ca 0d 30            FMUL float ptr [0x300dca04] | 300418bd   d8 0d 04 ca 0d 30            FMUL float ptr [0x300dca04]
 */
vec3_t cg_turretViewSwayOffset = { 0.0f, 0.0f, 0.0f };
/* Source: uo_cgame_mp_x86.dll 0x300dca08 (.data); refs=3 width=4; first=0x300418e1; owner=g_moverpush.
 * Example: 300418e1   d8 0d 08 ca 0d 30            FMUL float ptr [0x300dca08] | 300418f3   d8 0d 08 ca 0d 30            FMUL float ptr [0x300dca08] | 30041905   d8 0d 08 ca 0d 30            FMUL float ptr [0x300dca08]
 */
/* Source: uo_cgame_mp_x86.dll 0x300dca0c (.data); refs=1 width=4; first=0x30041913; owner=g_moverpush.
 * Example: 30041913   d9 05 0c ca 0d 30            FLD float ptr [0x300dca0c]
 */
/* Source: uo_cgame_mp_x86.dll 0x300dca10 (.data); refs=3 width=4; first=0x300400ae; owner=bg_parseweaponinfospecificfieldtyp.
 * Example: 300400ae   8b 04 bd 10 ca 0d 30         MOV EAX,dword ptr [EDI*0x4 + 0x300dca10] | 300400e3   89 14 bd 10 ca 0d 30         MOV dword ptr [EDI*0x4 + 0x300dca10],EDX | 30040119   89 14 bd 10 ca 0d 30         MOV dword ptr [EDI*0x4 + 0x300dca10],EDX
 */
int32_t cg_fovAdsUpdateTime[2] = { 0, 0 }; /* 0x300dca10..0x300dca18; see globals.h. CG_CalcFov FOV-zoom state. */
/* Source: uo_cgame_mp_x86.dll 0x300dca18 (.data); refs=2 width=4; first=0x30045579; owner=cg_drawplayerstance.
 * Example: 30045579   39 2d 18 ca 0d 30            CMP dword ptr [0x300dca18],EBP | 3004558c   89 2d 18 ca 0d 30            MOV dword ptr [0x300dca18],EBP
 */
int32_t cg_fakeTrajectoryEntity = 0;
/* Source: uo_cgame_mp_x86.dll 0x300dca1c (.data); refs=18 width=4; first=0x30046e1d; owner=cg_registergraphics.
 * Example: 30046e1d   c7 05 1c ca 0d 30 00 00 00 00 MOV dword ptr [0x300dca1c],0x0 | 30046ef2   c7 05 1c ca 0d 30 00 00 80 3f MOV dword ptr [0x300dca1c],0x3f800000 | 30046efe   c7 05 1c ca 0d 30 00 00 80 bf MOV dword ptr [0x300dca1c],0xbf800000
 */
float cg_weaponSelectTransition = 0.0f;
/* Source: uo_cgame_mp_x86.dll 0x300dca20 (.data); refs=2 width=4; first=0x3004555b; owner=cg_drawplayerstance.
 * Example: 3004555b   8b 0d 20 ca 0d 30            MOV ECX,dword ptr [0x300dca20] | 30045587   a3 20 ca 0d 30               MOV [0x300dca20],EAX
 */
int32_t cg_fakeTrajectoryTime = 0;
/* Source: uo_cgame_mp_x86.dll 0x300f08e0 (.data); refs=2 width=4; first=0x3004fd15.
 * Cgame script-export table; Scr_FarHook (0x3004fd00) seeds its five typed
 * callbacks and returns &cg_scriptExports. Zero-initialized
 * in BSS (the RVA is past the file image, so no .data initializer bytes exist).
 * Example: 3004fd15   c7 05 e0 08 0f 30 f0 62 01 30 MOV dword ptr [0x300f08e0],0x300162f0 | 3004fd47   b8 e0 08 0f 30               MOV EAX,0x300f08e0
 */
cg_scriptExportTable_t cg_scriptExports;
/* Source: uo_cgame_mp_x86.dll 0x300f08f8 (.data); refs=1 width=imm; first=0x3004fd0d.
 * Base of the 102-entry (408-byte) script-import function-pointer table.
 * Scr_FarHook REP-MOVSD-copies 0x66 dwords into it from its argument.
 * BSS (zero-init; RVA past the file image). Scr_BeginLoadAnimTrees and
 * Scr_FindAnimTree call its typed fields directly.
 * Example: 3004fd0d   bf f8 08 0f 30               MOV EDI,0x300f08f8
 */
cg_scriptImportTable_t cg_scriptImports;
/* Source: uo_cgame_mp_x86.dll 0x300f0b90..0x300f1b90 (.data).
 * PC_SourceWarning formats its variadic message into this private 4096-byte
 * buffer before reporting the current parser filename and line. */
char pc_sourceWarningMessage[4096] = { 0 };
/* Source: uo_cgame_mp_x86.dll 0x300f1b90 (.data); refs=3 width=4; first=0x3004fe24; owner=scriptentcmd_moveto.
 * Example: 3004fe24   8b 1c 85 90 1b 0f 30         MOV EBX,dword ptr [EAX*0x4 + 0x300f1b90] | 3004fef2   89 04 8d 90 1b 0f 30         MOV dword ptr [ECX*0x4 + 0x300f1b90],EAX | 3004ffbb   bf 90 1b 0f 30               MOV EDI,0x300f1b90
 */
stringDef_t *strHandle[UI_STRING_HASH_SIZE] = { 0 }; /* 0x300f1b90 */
/* Source: uo_cgame_mp_x86.dll 0x300f3b90 (.data); refs=1 width=4; first=0x3004fd8d.
 * Example: 3004fd8d   8d 81 90 3b 0f 30            LEA EAX,[ECX + 0x300f3b90]
 * The 0x20000-byte (128 KiB) UI general allocation pool that UI_Alloc (0x3004fd50)
 * bump-allocates from: the returned block is &memoryPool[allocPoint]. Extent
 * 0x300f3b90..0x30113b90; immediately follows the strHandle[2048] table. The
 * mechanical export truncated it to one dword and mislabeled owner=script_setplayermodel. */
/* The original base 0x300f3b90 is 16-byte aligned and UI_Alloc rounds every
 * block to 16 bytes. Preserve that host-object alignment explicitly so native
 * pointer-bearing UI records are not formed at under-aligned char storage. */
_Alignas(16) unsigned char memoryPool[UI_MEMORY_POOL_CAPACITY] = { 0 };
/* Source: uo_cgame_mp_x86.dll 0x30113c10 (.data); refs=1 width=1; first=0x3004fe93.
 * Example: 3004fe93   8d b9 10 3c 11 30            LEA EDI,[ECX + 0x30113c10]
 * The ui_shared.c string-intern character pool. String_Alloc (0x3004fe00) strcpy's each
 * unique string into strPool[strPoolIndex] and advances strPoolIndex (0x30134d50) by
 * strlen+1, erroring past the 0x20000 limit. Extent 0x30113c10..0x30133c10 == 0x20000
 * bytes exactly (next .data symbol is allocPoint). Cleared to 0 by String_Init.
 * (Mechanical owner scriptentcmd_moveto / uint32_t width were export artifacts.) */
char strPool[UI_STRING_POOL_CAPACITY] = { 0 }; /* 0x30113c10 */
/* Source: uo_cgame_mp_x86.dll 0x30133c10 (.data); refs=3 width=4; first=0x3004fd50; owner=script_setplayermodel.
 * Example: 3004fd50   8b 0d 10 3c 13 30            MOV ECX,dword ptr [0x30133c10] | 3004fd95   89 0d 10 3c 13 30            MOV dword ptr [0x30133c10],ECX | 3004ffda   89 35 10 3c 13 30            MOV dword ptr [0x30133c10],ESI
 */
int32_t allocPoint = 0; /* 0x30133c10: UI_Alloc pool bytes used (limit 0x20000). */
/* Source: uo_cgame_mp_x86.dll 0x30133c14 (.data); refs=2 width=4; first=0x3004fd66; owner=script_setplayermodel.
 * Example: 3004fd66   c7 05 14 3c 13 30 01 00 00 00 MOV dword ptr [0x30133c14],0x1 | 3004ffe0   89 35 14 3c 13 30            MOV dword ptr [0x30133c14],ESI
 */
qboolean outOfMemory = qfalse; /* 0x30133c14: UI_Alloc overflow flag. */
/* Source: uo_cgame_mp_x86.dll 0x30133c28..0x30133c48 (.data).
 * ui_shared.c's single scroll capture state; captureData points here. */
scrollInfo_t ui_scrollInfo = {0};
/* Source: uo_cgame_mp_x86.dll 0x30133c48 (.data); refs=2 first=0x300500b2.
 * PC_SourceError's static parse-error message buffer (repaired: the mechanical
 * export captured only the first dword as a uint32_t and mis-owned it to
 * `itemparse_elementtype`). Really a 4096-byte char array — size proven by the gap
 * to the next .data symbol (0x30134c48 - 0x30133c48 == 0x1000). PC_SourceError
 * (0x30050090) formats into it (vsprintf, PUSH at 0x300500b2) then reads it
 * back as the "%s" message to Com_Printf (PUSH at 0x300500e4).
 */
char pc_sourceErrorMessage[4096] = { 0 };
/* Source: uo_cgame_mp_x86.dll 0x30134cc8 (.data); refs=35 width=4; first=0x300012b4; owner=g_testentityposition.
 * Example: 300012b4   8b 3d c8 4c 13 30            MOV EDI,dword ptr [0x30134cc8] | 300016a5   8b 35 c8 4c 13 30            MOV ESI,dword ptr [0x30134cc8] | 300016d0   a1 c8 4c 13 30               MOV EAX,[0x30134cc8]
 */
bg_static_animation_table_t *bgAnimStaticTable = NULL;
/* Source: uo_cgame_mp_x86.dll 0x30134ccc (.data); refs=2 width=4; first=0x30002489; owner=cg_drawcrosshair.
 * Example: 30002489   a1 cc 4c 13 30               MOV EAX,[0x30134ccc] | 3000251c   c7 05 cc 4c 13 30 01 00 00 00 MOV dword ptr [0x30134ccc],0x1
 */
/* bgAnimScriptLoaded — one-time guard for the animation-script file load; see
 * globals.h. Zero-initialized in .data. */
/* Source: uo_cgame_mp_x86.dll 0x30134ccc. */
/* The corresponding source object is bgAnimScriptLoaded in
 * src/bg/bg_animation_script_data.c. */
/* Source: uo_cgame_mp_x86.dll 0x30134cd0 (.data); refs=15 width=4; first=0x3000a2c2; owner=bg_giveplayerweapon.
 * Example: 3000a2c2   a1 d0 4c 13 30               MOV EAX,[0x30134cd0] | 3000a62b   8b 15 d0 4c 13 30            MOV EDX,dword ptr [0x30134cd0] | 3000a6f1   8b 0d d0 4c 13 30            MOV ECX,dword ptr [0x30134cd0]
 */
/* c_pmove: monotonic pmove-invocation counter; ++ once per PmoveSingle
 * (0x3000e06d). Read as the "%i:lift\n" debug id by the ground-trace path.
 * The corresponding source object is shared in bg_pmove_state.c. */
/* Source: uo_cgame_mp_x86.dll 0x30134cd4 (.data); first=0x30001524.
 * Number of registered weaponInfo_t entries. Written by the weaponInfo_t parse path
 * (0x3000fee0, 0x30010df0) as a signed count; read by BG_GetWeaponIndexForName and
 * others. (Mechanical owner label was cmd_veh_freevehicle, its first-touching fn.)
 * Example: 30001524   a1 d4 4c 13 30   MOV EAX,[0x30134cd4] | 3000ff73   a3 d4 4c 13 30   MOV [0x30134cd4],EAX
 */
int32_t bg_numWeapons = 0;
/* Source: uo_cgame_mp_x86.dll 0x30134cd8 (.data); first=0x30001534.
 * Base of the weaponInfo_t pointer array, allocated via engine syscall 0xcb in
 * FUN_30010df0 (alloc-failure string: "Could not allocate WeaponInfo array").
 * Each element is a weaponInfo_t* with its name at +0x4.
 * Example: 30001534   a1 d8 4c 13 30   MOV EAX,[0x30134cd8] | 30010e29   89 15 d8 4c 13 30   MOV [0x30134cd8],EDX
 */
weaponInfo_t **bg_weaponInfos = 0;
/* Source: uo_cgame_mp_x86.dll 0x30134ce0 (.data); refs=3 width=4; first=0x3001d3ce.
 * cg_hudSpinPrevTime — cg_time (ms) at which CG_UpdateHudSpinAngle last advanced the
 * spinning HUD element. All 3 refs live in that one function; owner label
 * pm_beginweaponchange was a size-guess and is rejected. Signed ms time.
 * Example: 3001d3ce   a1 e0 4c 13 30               MOV EAX,[0x30134ce0] | 3001d41f   89 0d e0 4c 13 30            MOV dword ptr [0x30134ce0],ECX | 3001d6ab   89 0d e0 4c 13 30            MOV dword ptr [0x30134ce0],ECX
 */
int32_t cg_hudSpinPrevTime = 0;
/* Source: uo_cgame_mp_x86.dll 0x30134ce4 (.data); refs=3 width=4; first=0x3001d6ea; owner=veh_checkpushclients.
 * Example: 3001d6ea   a1 e4 4c 13 30               MOV EAX,[0x30134ce4] | 3001d72d   89 0d e4 4c 13 30            MOV dword ptr [0x30134ce4],ECX | 3001d94b   89 0d e4 4c 13 30            MOV dword ptr [0x30134ce4],ECX
 */
uint32_t cg_compassSpinPrevTime = (uint32_t)0x00000000u;
/* Source: uo_cgame_mp_x86.dll 0x30134ce8 (.data); refs=4 width=4; first=0x30025571; owner=hudelem_destroyall.
 * Resolved from the machine code to a heap-buffer base POINTER (not a scalar):
 * the reader at 0x30025571 loads it into EBP and immediately uses it as the
 * REP STOS destination base to zero 0xa8000 dwords (MOV EDI,EBP; MOV ECX,0xa8000;
 * REP STOSD), i.e. it is the base address of a large client data buffer; the
 * writer at 0x300279f3 stores an allocator result into it; the shutdown/reset
 * path at 0x3002e3c0 passes it to cgame trap 0xc1 (release) and then nulls it.
 * Widened from the mechanical uint32_t to a typed pointer at its canonical home.
 * RESOLVED: this is the flame-chunk pool base. The 0x30025570 init
 * (CG_ClearFlameChunks) and 0x300279d0 allocator (CG_InitFlameChunks) have now
 * been reconstructed: the allocator requests FLAME_CHUNK_COUNT*FLAME_CHUNK_SIZE
 * == 0x2a0000 bytes via cgame trap 0xc0 and stores the block here; the init
 * clears it and threads 0x2000 flame-chunk nodes of 0x150 bytes into the free
 * list. Retyped from void* to flameChunk_t*.
 * Example: 30025571   8b 2d e8 4c 13 30            MOV EBP,dword ptr [0x30134ce8] | 300279f3   a3 e8 4c 13 30               MOV [0x30134ce8],EAX | 3002e3c0   8b 0d e8 4c 13 30            MOV ECX,dword ptr [0x30134ce8]
 */
flameChunk_t *cg_flameChunks = NULL;
/* Source: uo_cgame_mp_x86.dll 0x30134cec (.data); refs=1 width=4; first=0x300255ee; owner=hudelem_destroyall.
 * Example: 300255ee   c7 05 ec 4c 13 30 01 00 00 00 MOV dword ptr [0x30134cec],0x1
 */
/* RESOLVED: flame-chunk-system initialized flag, set to 1 by CG_ClearFlameChunks
 * (0x300255ee). See globals.h. */
/* Source: uo_cgame_mp_x86.dll 0x30134cec. */
qboolean cg_flameChunksInited = qfalse;
/* Source: uo_cgame_mp_x86.dll 0x30134cf0 (.data) — cg_lastFlameChunkProcessed (see
 * globals.h). refs=1; only writer is CG_MoveFlameChunk (0x300265af). */
flameChunk_t *cg_lastFlameChunkProcessed = NULL;
/* Source: uo_cgame_mp_x86.dll 0x30134cf4 (.data); refs=1 width=4; first=0x30027d42; owner=cg_fireflamechunks.
 * Example: 30027d42   a1 f4 4c 13 30               MOV EAX,[0x30134cf4]
 */
int32_t cg_fireFlameChunksCvarsRegistered = 0;
/* Source: uo_cgame_mp_x86.dll 0x30134cf8 (.data); refs=58 width=4; first=0x3001c13b.
 * cg_updateScreenActive — the synchronous-redraw reentry latch. CG_DrawActive
 * (0x3001c120) returns immediately if this is nonzero (JNZ 0x3001c47d), otherwise
 * sets it to 1 for the duration of its levelshot/progress-bar draw and DECs it back
 * on exit (0x3001c14f / 0x3001c324). The sibling loading-screen updater at
 * 0x3002a530 uses the identical set-1/DEC bracket, so it guards against a
 * trap_UpdateScreen (CG_UPDATE_SCREEN) callback re-entering the loading draw. Retyped role
 * name; the mechanical owner=g_getnonpvsfriendlyinfo label was a wrong size-match
 * (this address's first toucher is CG_DrawActive, a cgame client draw function). */
int32_t cg_updateScreenActive = 0;
/* Source: uo_cgame_mp_x86.dll 0x30134cfc (.data); refs=8 width=4; first=0x3002aa24.
 * cg_numLocalEntities: number of localEntity_t on the cg_activeLocalEntities list.
 * Zeroed by CG_InitLocalEntities, ++ by CG_AllocLocalEntity, -- by CG_FreeLocalEntity.
 * (Provisional name by role; the mechanical owner="anglenormalize180accurate" is wrong.)
 */
int32_t cg_numLocalEntities = 0;
/* Source: uo_cgame_mp_x86.dll 0x30134d00 (.data).
 * CG_TranslateMessage XOR-toggles this index before selecting a result buffer. */
int32_t cg_translateMessageBufferIndex = 0;
/* Source: uo_cgame_mp_x86.dll 0x30134d04 (.data). cg_statBarDisplayFrac — the
 * smoothed/trailing fill fraction of the HUD stat bar (FLD/FST/FSUBR float). Retyped
 * from the mechanical uint32_t to float; (owner=stopfollowing) was a wrong size-guess.
 * Consumed only by CG_DrawStatBarWithDecay (0x3002f9d0). Zero-initialized. */
float cg_statBarDisplayFrac = 0.0f;
/* Source: uo_cgame_mp_x86.dll 0x30134d08 (.data): set after the skybox fog
 * service is configured so later no-fog frames do not submit redundant clears. */
qboolean cg_skyboxFogConfigured = qfalse;
/* Source: uo_cgame_mp_x86.dll 0x30134d10 (.data); refs=3 width=4; first=0x30046e27; owner=cg_registergraphics.
 * Example: 30046e27   89 0d 10 4d 13 30            MOV dword ptr [0x30134d10],ECX | 30046ec8   39 05 10 4d 13 30            CMP dword ptr [0x30134d10],EAX | 30046ed0   a3 10 4d 13 30               MOV [0x30134d10],EAX
 */
int32_t cg_weaponSelectPreviousSlot = 0;
/* Source: uo_cgame_mp_x86.dll 0x30134d14 (.data); refs=4 width=4; first=0x30046e2d; owner=cg_registergraphics.
 * Example: 30046e2d   89 35 14 4d 13 30            MOV dword ptr [0x30134d14],ESI | 30046ed5   89 35 14 4d 13 30            MOV dword ptr [0x30134d14],ESI | 30046ee0   a1 14 4d 13 30               MOV EAX,[0x30134d14]
 */
int32_t cg_weaponSelectPreviousWeapon = 0;
/* Source: uo_cgame_mp_x86.dll 0x30134d20 (.data); refs=1 width=4; first=0x3005adc0; owner=sp_func_door_rotating (wrong: size-only guess).
 * ui_shared.c `captureFunc` — per-frame capture callback (head of the capture
 * triad with captureData/captureItem). Set to a handler code address (0x30054420 /
 * 0x30054490) at 0x3005480b/0x30054826 and cleared on release; read only by
 * Menu_PaintAll (0x3005adc0), which calls it with captureData. Storage supersedes
 * the mechanical uint32_t.
 * Example: 3005adc0   a1 20 4d 13 30               MOV EAX,[0x30134d20] | 3005480b   c7 05 20 4d 13 30 20 44 05 30 MOV [0x30134d20],0x30054420
 */
ui_captureFunc_t captureFunc = 0; /* 0x30134d20 */
/* Source: uo_cgame_mp_x86.dll 0x30134d24 (.data); refs=1 width=4; first=0x3005adcc; owner=sp_func_door_rotating (wrong: size-only guess).
 * ui_shared.c `captureData` — opaque argument passed to captureFunc. Set to a data
 * pointer (0x30133c28) alongside captureFunc; read only by Menu_PaintAll.
 * Example: 3005adcc   8b 0d 24 4d 13 30            MOV ECX,dword ptr [0x30134d24] | 30054815   c7 05 24 4d 13 30 28 3c 13 30 MOV [0x30134d24],0x30133c28
 */
void *captureData = 0; /* 0x30134d24 */
/* Source: uo_cgame_mp_x86.dll 0x30134d28 (.data); refs=2 width=4; first=0x30052cd0; owner=menu_orbititembyname (wrong: size-only guess).
 * ui_shared.c `captureItem` — the itemDef_t currently captured for a mouse drag.
 * Written with an item pointer (ESI) at 0x300547ea and 0x30054851 alongside
 * captureFunc (0x30134d20) and captureData (0x30134d24); cleared to 0 with them
 * at 0x300549af on release. Compared against the item in
 * Item_ListBox_ThumbDrawPosition (0x30052cd0). Storage supersedes the mechanical
 * uint32_t; a NULL item pointer is the "nothing captured" state.
 * Example: 30052cd0   39 35 28 4d 13 30            CMP dword ptr [0x30134d28],ESI | 300547ea   89 35 28 4d 13 30            MOV [0x30134d28],ESI
 */
struct itemDef_s *captureItem = 0; /* 0x30134d28 */
/* Source: uo_cgame_mp_x86.dll 0x30134d2c (.data); refs=212 width=4; first=0x3002dc85; owner=pm_updateaimdownsightlerp.
 * Example: 3002dc85   c7 05 2c 4d 13 30 60 1f 42 30 MOV dword ptr [0x30134d2c],0x30421f60 | 3004fff0   a1 2c 4d 13 30               MOV EAX,[0x30134d2c] | 30050600   8b 0d 2c 4d 13 30            MOV ECX,dword ptr [0x30134d2c]
 */
displayContextDef_t *DC = 0; /* 0x30134d2c: ui_shared.c DC (static instance 0x30421f60, set at runtime). */
/* 0x30421f60 .data: the static displayContextDef_t instance that DC points at,
 * filled in and published by CG_UIDisplayContextInit (0x3002da90). */
displayContextDef_t g_uiDCInstance;
/* Source: uo_cgame_mp_x86.dll 0x30134d30 (.data); refs=5 width=4; first=0x30057078; owner=bg_calculateweaponposition_boboffs (wrong: first-touching function).
 * ui_shared.c `g_waitingForKey` — nonzero while the UI awaits the next key/mouse
 * press for a capture (bind/drag). Set to 1 at 0x300570a6, cleared at 0x300571fd,
 * read as an input-suppression guard by Menu_HandleMouseMove (0x30058a10).
 * Provisional role-based name; exact original symbol not fully proven.
 * Example: 30057078   a1 30 4d 13 30               MOV EAX,[0x30134d30] | 300570a6   a3 30 4d 13 30               MOV [0x30134d30],EAX
 */
int32_t g_waitingForKey = 0; /* 0x30134d30 */
/* Source: uo_cgame_mp_x86.dll 0x30134d34 (.data); refs=2 width=4; first=0x300562ad; owner=pm_friction (wrong: first-touching function).
 * ui_shared.c `g_editingField` — nonzero while a text edit field holds input
 * focus. Read as an input-suppression guard at 0x300562ad and by
 * Menu_HandleMouseMove (0x30058a10, 0x30058a4f). Provisional role-based name;
 * exact original symbol not fully proven.
 * Example: 300562ad   a1 34 4d 13 30               MOV EAX,[0x30134d34] | 30058a4f   39 05 34 4d 13 30            CMP dword ptr [0x30134d34],EAX
 */
int32_t g_editingField = 0; /* 0x30134d34 */
/* Source: uo_cgame_mp_x86.dll 0x30134d38 (.data); ui_shared.c g_bindItem — the
 * ITEM_TYPE_BIND item currently capturing the next key press (set with
 * g_waitingForKey by Item_Bind_HandleKey, 0x300570a6). refs=3 width=4;
 * first=0x30056e7b.
 * Example: 30056e7b   39 3d 38 4d 13 30            CMP dword ptr [0x30134d38],EDI | 3005709f   89 35 38 4d 13 30            MOV dword ptr [0x30134d38],ESI | 300570bd   a1 38 4d 13 30               MOV EAX,[0x30134d38]
 */
struct itemDef_s *g_bindItem = 0; /* 0x30134d38 */
/* Source: uo_cgame_mp_x86.dll 0x30134d3c (.data); refs=4 width=4; first=0x300540fe; owner=com_parseext.
 * Example: 300540fe   a3 3c 4d 13 30               MOV [0x30134d3c],EAX | 3005413f   a3 3c 4d 13 30               MOV [0x30134d3c],EAX | 3005417b   a3 3c 4d 13 30               MOV [0x30134d3c],EAX
 */
struct itemDef_s *g_editItem = 0;
/* Source: uo_cgame_mp_x86.dll 0x30134d40 (.data); first=0x3002d3b5.
 * Number of registered menus in the global Menus table (Menus). Signed
 * (iterated with a signed bound in Menus_FindByName). Mechanical owner label was
 * bg_setupweaponalts, its first-touching function.
 * Example: 3002d3b5   89 1d 40 4d 13 30   MOV [0x30134d40],EBX | 3002dc8f   ...   MOV [0x30134d40],0x0
 */
int32_t menuCount = 0;
/* Source: uo_cgame_mp_x86.dll 0x30134d44 (.data); refs=15 width=4; first=0x3004ffd4.
 * ui_shared.c `openMenuCount` — number of menus on the global open-menu stack
 * menuStack (0x30169880). Resolved by Menus_AddToStack (0x30051830). Reset to
 * 0 by String_Init (0x3004ffb0). (Mechanical owner label `longswap` was wrong.)
 * Example: 3004ffd4   89 35 44 4d 13 30            MOV dword ptr [0x30134d44],ESI | 30051830   8b 15 44 4d 13 30            MOV EDX,dword ptr [0x30134d44] | 30051852   89 15 44 4d 13 30            MOV dword ptr [0x30134d44],EDX
 */
int32_t openMenuCount = 0;
/* Source: uo_cgame_mp_x86.dll 0x30134d48 (.data); refs=6 width=4; first=0x30050ae3; owner=cg_drawactiveframe.
 * ui_shared.c `debugMode` — boolean toggle gating the debug rectangle/outline
 * drawing. Read by Menu_Paint (0x30058cc5), Window_Paint (0x30050ae3) and the item
 * painters (0x30058592); toggled by the debug-cvar handler at 0x300552f4
 * (`EAX = debugMode ^ 1`). Provisional role-based name; the mechanical owner label
 * cg_drawactiveframe was its first-touching function and is wrong.
 * Example: 30050ae3   a1 48 4d 13 30               MOV EAX,[0x30134d48] | 30058592   a1 48 4d 13 30               MOV EAX,[0x30134d48] | 30058cc5   a1 48 4d 13 30               MOV EAX,[0x30134d48]
 */
int32_t debugMode = 0;
/* Source: uo_cgame_mp_x86.dll 0x30134d4c (.data).
 * Item_ListBox_HandleKey compares DC->realTime against this deadline, runs
 * listBoxDef_t.doubleClick on a repeated selection, then stores realTime + 300. */
int32_t lastListBoxClickTime = 0;
/* Source: uo_cgame_mp_x86.dll 0x30134d50 (.data); refs=3 width=4; first=0x3004fe7f; owner=scriptentcmd_moveto.
 * Example: 3004fe7f   8b 0d 50 4d 13 30            MOV ECX,dword ptr [0x30134d50] | 3004feae   89 35 50 4d 13 30            MOV dword ptr [0x30134d50],ESI | 3004ffc8   89 35 50 4d 13 30            MOV dword ptr [0x30134d50],ESI
 */
int32_t strPoolIndex = 0; /* 0x30134d50: UI string-pool write cursor. */
/* Source: uo_cgame_mp_x86.dll 0x30134d54 (.data); refs=1 width=4; first=0x3004ffc2; owner=longswap.
 * Example: 3004ffc2   89 35 54 4d 13 30            MOV dword ptr [0x30134d54],ESI
 */
int32_t strHandleCount = 0; /* 0x30134d54: UI string-subsystem counter reset by String_Init; exact role unresolved. */
/* Source: uo_cgame_mp_x86.dll 0x30134d58 (.data).  Menu_HandleKey's recursion
 * guard for the Menus_HandleOOBClick path. */
int32_t inHandleKey = 0;
/* Source: uo_cgame_mp_x86.dll 0x30134fd4..0x30135fec (.data).
 * MSVC CRT small-block-heap state. The only direct references are in the linked
 * CRT heap bank: 0x30135fe4 is the HeapCreate handle, 0x30135fe8 the heap mode,
 * and 0x30134fd4/0x30135fe0 are maintained by its small-block initializer.
 * This is compiler-runtime storage, not recovered cgame source. */
uint8_t msvc_crt_smallBlockHeapState[4120];
/* 0x30136000 (.data): the MSVC CRT lowio scalable I/O-info table __pioinfo — an
 * array of pointers to blocks of 32 `ioinfo` structs (stride 0x24, each with a
 * CRITICAL_SECTION lock at +0xc). Indexed as __pioinfo[fd>>5] then entry (fd&0x1f)*0x24.
 * Operated on by _lock_fhandle/_unlock_fhandle (0x30068193/0x30068274, both
 * STATIC_LINKAGE msvc_crt) via Enter/LeaveCriticalSection. The exporter's
 * "scriptentcmd_notsolid" was a first-touch auto-name, not the real identity.
 * Kept uint32_t (a table base pointer stored as a 32-bit dword in this 32-bit DLL).
 * Source refs=23; first=0x3005cac1 (MOV ECX,[ECX*4 + 0x30136000]); CRT init writes it at 0x30060292. */
uint32_t crt_pioinfo_table = (uint32_t)0x00000000u;
/* Source: uo_cgame_mp_x86.dll 0x30136940 (.data); first=0x30051929.
 * The global Menus table (array of menuDef_t, 0x810-byte records, name at +0x20).
 * In the original this is fixed .data storage indexed base + i*0x810;
 * menuCount holds the live count. Menu_New compares the count against 100 at
 * 0x3005ad54 (`CMP EAX,0x64`), proving the table capacity. Mechanical owner
 * label was itemparse_hidecvar.
 * Example: 30051929   05 40 69 13 30   ADD EAX,0x30136940 | 3005ad4e   ...   ADD ESI,0x30136940
 */
menuDef_t Menus[MAX_MENUS];
/* 0x3016987c is not a distinct datum: it is &menuStack[-1], the base-4
 * addressing form used by Menus_Close (0x30051970) `MOV EDX,[EDX*4+0x3016987c]`
 * to read menuStack[edx-1]. The former mechanical split symbol
 * g_data_cg_getmg42anims_3016987c is superseded by menuStack (below) and
 * removed; its sole consumer now indexes the array directly. */
/* Source: uo_cgame_mp_x86.dll 0x30169880 (.data): ui_shared.c `menuStack[16]`
 * — the global open-menu stack (16 menuDef_t*, 0x40 bytes 0x30169880..0x301698c0).
 * Resolved by Menus_AddToStack (0x30051830). The mechanical exporter split this array
 * into per-element scalars 0x30169880 ([0]) and 0x30169884 ([1]); this single array
 * supersedes both (owner label `vectoranglemultiply` was a size-only false name).
 * Example: 30051840   39 1c 85 80 98 16 30         CMP dword ptr [EAX*0x4 + 0x30169880],EBX | 3005185b   8d 3c 85 80 98 16 30         LEA EDI,[EAX*0x4 + 0x30169880] | 30051888   89 1c 85 80 98 16 30         MOV dword ptr [EAX*0x4 + 0x30169880],EBX
 */
menuDef_t *menuStack[MAX_OPEN_MENUS] = { 0 };
/* Source: uo_cgame_mp_x86.dll 0x301698c0 (.data); refs=3 width=4; first=0x30046228; owner=item_listbox_paint.
 * Example: 30046228   89 0d c0 98 16 30            MOV dword ptr [0x301698c0],ECX | 30047c99   68 c0 98 16 30               PUSH 0x301698c0 | 30047cfb   68 c0 98 16 30               PUSH 0x301698c0
 */
vec3_t cg_brassEffectOrigin = { 0.0f, 0.0f, 0.0f };
/* Source: uo_cgame_mp_x86.dll 0x301698c4 (.data); refs=1 width=4; first=0x3004622e; owner=item_listbox_paint.
 * Example: 3004622e   89 15 c4 98 16 30            MOV dword ptr [0x301698c4],EDX
 */
/* Source: uo_cgame_mp_x86.dll 0x301698c8 (.data); refs=1 width=4; first=0x30046234; owner=item_listbox_paint.
 * Example: 30046234   a3 c8 98 16 30               MOV [0x301698c8],EAX
 */
/* Source: uo_cgame_mp_x86.dll 0x301698cc..0x3016a9e0 (.data).
 * Loader-zero storage emitted between the last ui_shared static and the voice-chat
 * tables. No PE relocation, initialized pointer, or instruction references any byte
 * in this interval. Preserve it as an explicitly inert UI aggregate tail; its
 * original field subdivision has no observable behavior in this build. */
uint8_t ui_unreferencedAggregateStorage[4372];
/* Source: uo_cgame_mp_x86.dll 0x3016a9e0..0x303b3420 (.data) — the voice-chat table
 * region. Eight 0x49148-byte blocks (block[0]=axis @0x3016a9e0, block[1]=allies
 * @0x301b3b28), loaded at runtime from mp/axis_chat.voice / mp/allies_chat.voice by
 * the loader at 0x300396f0. The old mechanical owners (0x3016a9e0/0x301b3b28
 * = playercmd_finishplayerdamage; 0x303b3420 = the end sentinel, mislabeled
 * bg_calculateweaponposition_damagek) were wrong and are repaired here.
 * Zero-initialized in .data; contents are filled in at load time.
 * &cg_voiceChatTables[8] == 0x303b3420, the walk sentinel used at 0x30039ed2. */
cgVoiceChatTable_t cg_voiceChatTables[CG_VOICE_CHAT_TABLE_COUNT];
/* Source: uo_cgame_mp_x86.dll 0x303b3420..0x303b5d20 (.data).
 * The 0x148-byte descriptor copied by CG_VoiceChat is element zero of the original
 * 32-entry buffered-voice-chat array.  The formerly unclaimed tail is exactly
 * 31 * 0x148 bytes, and the next object begins at 0x303b5d20. */
cgVoiceChatMsg_t cg_voiceChatBuffer[CG_VOICE_CHAT_BUFFER_COUNT];
/* Mark-poly (decal) pool. The mechanical owner=quatinverse labels on these five
 * addresses were wrong (the first-touching function is the pool initializer
 * CG_InitMarkPolys, 0x3002e400, not "QuatInverse"). Repaired in place to the real
 * mark-poly free-list head, pool array, and active-list sentinel proven by
 * CG_InitMarkPolys and CG_AllocMark (0x3002e490). Zero-initialized in C; the
 * runtime pointers/links are set up by CG_InitMarkPolys.
 *
 * cg_freeMarkPolys @ 0x303b5d20: head of the markPoly_t free list (->nextMark). */
struct markPoly_s *cg_freeMarkPolys = 0;
/* cg_markPolys @ 0x303b5d40: the mark-poly pool array (MAX_MARK_POLYS nodes).
 * 0x303b5eb4 (== &cg_markPolys[1], the exporter's separate g_data_..._303b5eb4)
 * is only the init free-list threading cursor and is subsumed by this array. */
struct markPoly_s cg_markPolys[MAX_MARK_POLYS];
/* cg_activeMarkPolys @ 0x30412d40: sentinel of the circular doubly-linked active
 * list. prevMark at +0x0 (0x30412d40) and nextMark at +0x4 (0x30412d44) — the
 * two mechanical dwords the exporter split out — are its head/tail links. */
struct markPoly_s cg_activeMarkPolys;
/* Source: uo_cgame_mp_x86.dll 0x30412ecc (.data); refs=2 width=4; first=0x3002e536.
 * cg_marks_vmCvar.integer: cached .integer of the cg_addMarks cvar; the marks-enable
 * gate read by CG_ImpactMark (0x3002e536) and CG_AddMarks (0x3002e8c0).
 * Example: 3002e536   a1 cc 2e 41 30               MOV EAX,[0x30412ecc] | 3002e8c0   a1 cc 2e 41 30               MOV EAX,[0x30412ecc]
 */

/* Source: uo_cgame_mp_x86.dll 0x30413100 (.data); refs=1 width=imm; first=0x30022c07; owner=cmd_callvote_f.
 * Example: 30022c07   68 00 31 41 30               PUSH 0x30413100
 */

/* Source: uo_cgame_mp_x86.dll 0x30413460 (.data). cg_cvarTable[179] identifies
 * this vmCvar_t as cl_serverloadwaiting; consumers test .integer at +0x0c. */
vmCvar_t cl_serverloadwaiting;
/* Source: uo_cgame_mp_x86.dll 0x30413580 (.data); refs=32 width=4; first=0x30005a76.
 * cg_weaponInfos[128]: the cgame per-weapon record table (cgWeaponInfo_t, stride
 * 0x1c4, name cached at +0x64). Element count 128 is proven by the byte extent to
 * the next table base 0x30421788 ((0x30421788-0x30413580)/0x1c4 = 128), matching
 * the 128-weapon MAX_WEAPONS bitset noted in client_recovered.h. The mechanical
 * export split this single array into many interior symbols (0x30413630, ...) and
 * mislabeled the base owner=g_freevehicle (a first-touch label), rejected.
 * Example: 30005a76   81 c7 80 35 41 30   ADD EDI,0x30413580
 */
cgWeaponInfo_t cg_weaponInfos[MAX_WEAPONS];
/* Source: uo_cgame_mp_x86.dll 0x304219c8 (.data); refs=20 width=4; first=0x300168d6; owner=cg_asset_parse.
 * Example: 300168d6   8b 15 c8 19 42 30            MOV EDX,dword ptr [0x304219c8] | 30016991   8b 0d c8 19 42 30            MOV ECX,dword ptr [0x304219c8] | 300169c4   d8 1d c8 19 42 30            FCOMP float ptr [0x304219c8]
 */

/* Source: uo_cgame_mp_x86.dll 0x30421ae8 (.data); refs=4 width=4; first=0x30045f86; owner=item_listbox_paint.
 * Example: 30045f86   d8 0d e8 1a 42 30            FMUL float ptr [0x30421ae8] | 30046018   d8 0d e8 1a 42 30            FMUL float ptr [0x30421ae8] | 30046064   d8 0d e8 1a 42 30            FMUL float ptr [0x30421ae8]
 */

/* Source: uo_cgame_mp_x86.dll 0x30421c0c (.data); refs=1 width=4; first=0x3003a50e.
 * cg_noTaunt_vmCvar.integer: voice-chat category filter flag, read only by
 * CG_ParseVoiceChat (0x3003a410, MOV EAX,[0x30421c0c] at 0x3003a50e). When nonzero,
 * the "insult"/"taunt"/"praise"/"gauntlet" voice-chat categories are suppressed
 * (their display is skipped). Never written in this DLL — set from elsewhere (a cvar
 * mirror). Retired owner-tag name g_data_bg_canitembegrabbed_30421c0c (the
 * BG_CanItemBeGrabbed owner was a rejected size-match on 0x3003a410).
 * Example: 3003a50e   a1 0c 1c 42 30               MOV EAX,[0x30421c0c]
 */

/* Source: uo_cgame_mp_x86.dll 0x30421d2c (.data); refs=2 width=4; first=0x300327cc; owner=com_parseonline.
 * Example: 300327cc   2b 05 2c 1d 42 30            SUB EAX,dword ptr [0x30421d2c] | 30037e6e   03 05 2c 1d 42 30            ADD EAX,dword ptr [0x30421d2c]
 */

/* Source: uo_cgame_mp_x86.dll 0x30421e4c (.data); refs=1 width=4; first=0x30048536; owner=veh_updateclient.
 * Example: 30048536   a1 4c 1e 42 30               MOV EAX,[0x30421e4c]
 */

/* 0x30422054..0x3042205b are g_uiDCInstance.cursorx/cursory. CGVM_MOUSE_EVENT
 * copies cgs_cursorX/cgs_cursorY into those fields. */
/* 0x30422064..0x3044028b are owned by g_uiDCInstance fields; no separate storage. */
/* Source: uo_cgame_mp_x86.dll 0x3044034c (.data); refs=21 width=4; first=0x30022686; owner=vectordistance2d.
 * Example: 30022686   8b 15 4c 03 44 30            MOV EDX,dword ptr [0x3044034c] | 3002ec6b   a1 4c 03 44 30               MOV EAX,[0x3044034c] | 3002edbb   a1 4c 03 44 30               MOV EAX,[0x3044034c]
 */
/* 0x3044046c: cg_stats.integer; cvar-table entry 0x30085760. */

/* Source: uo_cgame_mp_x86.dll 0x304407c8 (.data); refs=2 width=4; first=0x3002ab0c; owner=pm_switchifempty.
 * Example: 3002ab0c   a1 c8 07 44 30               MOV EAX,[0x304407c8] | 30048a27   a1 c8 07 44 30               MOV EAX,[0x304407c8]
 * Repaired uint32_t->float: read as `float ptr` by CG_AddMovingTracer (moving-tracer
 * width, mode A). Zero-initialized in .bss (set at init by an unseen writer). */

/* Source: uo_cgame_mp_x86.dll 0x30440a00..0x30447a04 (.data).
 * The DLL passes this base to trap 79. The symbolized Mac engine CL_GetGameState
 * copies 0xe00 eight-byte pairs plus one final dword: exactly 0x7004 bytes.
 * Windows consumers place stringData at +0x2000, proving the complete layout in
 * gameState_t and the exact +0x7000 dataCount field. */
gameState_t cg_gameState = { { 0 }, { 0 }, 0 };
/* Source: uo_cgame_mp_x86.dll 0x30447a04 (.data): cgs.glconfig, filled by
 * CG_Init's trap 0x4e. vidWidth/vidHeight are at original +0x84/+0x88. */
glconfig_t cgs_glconfig = { 0 };
/* Source: uo_cgame_mp_x86.dll 0x30447aa4 (.data); refs=96; owner=pm_cmdscale label is wrong.
 * cgs.screenXScale (float). Written in the cgs-init path at 0x3002e016 as
 * (float)glconfig.vidWidth * (1.0f/640.0f) [FILD [0x30447a88]; FMUL
 * [0x3007c3f4]=0.0015625]; consumed as a float in every 2D-draw wrapper
 * (e.g. FMUL [0x30447aa4] at 0x30017d3b, and CG_DrawPic at 0x3001cae0, and
 * CG_DrawTurretTagQuad at 0x3001ccf0). Zero-initialized .data (BSS tail).
 * The 1/640 factor is proven from the writer's .rdata constant 0x3007c3f4.
 */
float cgs_screenXScale = 0.0f;
/* Source: uo_cgame_mp_x86.dll 0x30447aa8 (.data); refs=96; owner=pm_cmdscale label is wrong.
 * cgs.screenYScale (float). Written in the cgs-init path at 0x3002e028 as
 * (float)glconfig.vidHeight * (1.0f/480.0f) [FILD [0x30447a8c]; FMUL
 * [0x3007c3f0]=0.0020833334]; consumed as a float in every 2D-draw wrapper
 * (e.g. FMUL [0x30447aa8] at 0x30017d0f, and CG_DrawPic at 0x3001caa4, and
 * CG_DrawTurretTagQuad at 0x3001ccf0). Zero-initialized .data (BSS tail).
 * The 1/480 factor is proven from the writer's .rdata constant 0x3007c3f0.
 */
float cgs_screenYScale = 0.0f;
/* Source: uo_cgame_mp_x86.dll 0x30447ab0 (.data); refs=5 width=4; first=0x30018021.
 * cgs.serverCommandSequence — see globals.h. Read as the "cmd:%i" value by
 * CG_DrawSnapshot (0x30018021); written in the cgame init/config path (0x3002dfbb).
 * Example: 30018021   a1 b0 7a 44 30               MOV EAX,[0x30447ab0] | 3002dfbb   a3 b0 7a 44 30               MOV [0x30447ab0],EAX | 3003b476   a1 b0 7a 44 30               MOV EAX,[0x30447ab0]
 */
int32_t cgs_serverCommandSequence = 0;
/* Source: uo_cgame_mp_x86.dll 0x30447ab4 (.data): cg.processedSnapshotNum — the
 * snapshot number cgame has consumed up through (see globals.h). Signed. Retyped from
 * the mechanical uint32_t (owner=g_runframe was the first-touching function).
 * Example: 3002dfb5   89 15 b4 7a 44 30            MOV dword ptr [0x30447ab4],EDX | 3003d220   a1 b4 7a 44 30               MOV EAX,[0x30447ab4] | 3003d247   a1 b4 7a 44 30               MOV EAX,[0x30447ab4]
 */
int32_t cg_processedSnapshotNum = 0;
/* Source: uo_cgame_mp_x86.dll 0x30447ab8 (.data); refs=5 width=4; first=0x30018bd0.
 * cgs_localServer — the cgame "config values already registered"
 * once-guard. Set nonzero in the cgame config/init path (0x3002b20d, from a trap
 * return value used as a sentinel handle), then read as `if (flag != 0) return;`
 * by every config-value setter to make the registration idempotent:
 * CG_SetConfigValues (0x300383cf, 0x30038437), the sibling setter at 0x300383cf,
 * and 0x30018bd0. Role name; exact CoD symbol unproven. The mechanical
 * owner=cmd_veh_fireturret label is a wrong first-touch guess and is rejected.
 * Example: 30018bd0   a1 b8 7a 44 30               MOV EAX,[0x30447ab8] | 3002b20d   a3 b8 7a 44 30               MOV [0x30447ab8],EAX | 300383c7   a1 b8 7a 44 30               MOV EAX,[0x30447ab8]
 */
int32_t cgs_localServer = 0;
/* cgs serverinfo mirror cluster (see globals.h for full evidence). Filled by
 * CG_ParseServerinfo (0x30038380) from config string 0 via Info_ValueForKey,
 * then read by many cgame consumers. Contiguous: gametype[32], hostname[256],
 * maxclients (int32), mapname[64]. The mechanical owner=<first-touch> labels
 * are wrong first-touch guesses and are rejected. */
/* Source: uo_cgame_mp_x86.dll 0x30447abc (.data). cgs.gametype (g_gametype
 * value, Q_strncpyz size 0x20). Written by CG_ParseServerinfo 0x300383bd; read
 * by 0x3002d5ef/0x30031c40/0x30036361. */
char cgs_gametype[32] = { 0 };
/* Source: uo_cgame_mp_x86.dll 0x30447adc (.data). cgs.hostname (sv_hostname
 * value, Q_strncpyz size 0x100). Written by CG_ParseServerinfo 0x300383a1; read
 * by 0x30036757/0x300367cd/0x3003682e. */
char cgs_hostname[256] = { 0 };
/* Source: uo_cgame_mp_x86.dll 0x30447bdc (.data). cgs.maxclients
 * (atoi(sv_maxclients)). Written by CG_ParseServerinfo 0x300383fd; read as int
 * by 0x30026344 (CMP) and 0x300327f1 (MOV EBP). */
int32_t cgs_maxclients = 0;
/* Source: uo_cgame_mp_x86.dll 0x30447be0 (.data). cgs.mapname ("maps/mp/%s.bsp"
 * from serverinfo mapname, Com_sprintf size 0x40). Written by CG_ParseServerinfo
 * 0x30038412; read by 0x3002e0f7..0x3002e1f8 and 0x30036420. */
char cgs_mapname[MAX_QPATH] = { 0 };
/* Source: uo_cgame_mp_x86.dll 0x30447c20..0x30447ca0 (.data).
 * cgs red/blue team-name buffers.  They occupy the two 64-byte fields between
 * mapname and the vote state; this build never directly references either field. */
char cgs_teamNames[2][64] = { { 0 } };
/* cgame vote HUD display cluster (see globals.h for full evidence). Rebuilt in
 * batch by CG_BuildVoteHudStrings (0x3002ddf0), per-field by the config-string
 * dispatcher (0x30038e70 cases 0x10/0x11/0x12/0x13), drawn by 0x3001b7d0. */
/* Source: uo_cgame_mp_x86.dll 0x30447ca0 (.data); refs=10 width=4; first=0x3001b814. */
int32_t cg_voteTime = 0;
/* Source: uo_cgame_mp_x86.dll 0x30447ca4 (.data); refs=5; first=0x3001bae1.
 * CG_PlayFxOnTag's indexed instruction uses this numerical displacement only
 * because MSVC folded the effect-id ASCII bias into the cg_effectDefs address;
 * it does not access the vote object. */
int32_t cg_voteYes = 0;
/* Source: uo_cgame_mp_x86.dll 0x30447ca8 (.data); refs=4; first=0x3001badb. */
int32_t cg_voteNo = 0;
/* Source: uo_cgame_mp_x86.dll 0x30447cac (.data); refs=5 width=4; first=0x3001ba0e.
 * cgs.voteModified flag (set by dispatcher 0x30039003, tested/cleared by drawer). */
qboolean cg_voteModified = qfalse;
/* Source: uo_cgame_mp_x86.dll 0x30447cb0..0x30447daf (.data); refs=5;
 * cgs.voteString[256]. The two byte stores at +255 explicitly terminate the
 * preceding strncpy(...,255); there is no consumer proving a separate flag. */
char cg_voteString[256] = { 0 };
/* The standard two-team vote block: four parallel two-int arrays followed by
 * two 256-byte vote strings.  No instruction in this build references the block,
 * but its field sizes exactly fill the interval before cg_timeoutEndTime. */
/* Source: uo_cgame_mp_x86.dll 0x30447db0..0x30447db8 (.data). */
int32_t cg_teamVoteTime[2] = { 0 };
/* Source: uo_cgame_mp_x86.dll 0x30447db8..0x30447dc0 (.data). */
int32_t cg_teamVoteYes[2] = { 0 };
/* Source: uo_cgame_mp_x86.dll 0x30447dc0..0x30447dc8 (.data). */
int32_t cg_teamVoteNo[2] = { 0 };
/* Source: uo_cgame_mp_x86.dll 0x30447dc8..0x30447dd0 (.data). */
qboolean cg_teamVoteModified[2] = { qfalse };
/* Source: uo_cgame_mp_x86.dll 0x30447dd0..0x30447fd0 (.data). */
char cg_teamVoteString[2][256] = { { 0 } };
/* Source: uo_cgame_mp_x86.dll 0x30447fd0 (.data); refs=11 width=4; first=0x3001bbd3; owner=pm_reloadclip.
 * Example: 3001bbd3   a1 d0 7f 44 30               MOV EAX,[0x30447fd0] | 3001bc08   8b 0d d0 7f 44 30            MOV ECX,dword ptr [0x30447fd0] | 3002dea3   c7 05 d0 7f 44 30 00 00 00 00 MOV dword ptr [0x30447fd0],0x0
 */
int32_t cg_timeoutEndTime = 0;
/* Source: uo_cgame_mp_x86.dll 0x30447fd4 (.data); refs=4 width=4; first=0x3001bc28; owner=pm_reloadclip.
 * Example: 3001bc28   8b 0d d4 7f 44 30            MOV ECX,dword ptr [0x30447fd4] | 3002de97   c7 05 d4 7f 44 30 00 00 00 00 MOV dword ptr [0x30447fd4],0x0 | 3002decd   c7 05 d4 7f 44 30 01 00 00 00 MOV dword ptr [0x30447fd4],0x1
 */
int32_t cg_timeoutActive = 0;
/* Source: uo_cgame_mp_x86.dll 0x30447fd8 (.data); refs=3 width=imm; first=0x3001bc31; owner=pm_reloadclip.
 * Example: 3001bc31   68 d8 7f 44 30               PUSH 0x30447fd8 | 3002df0d   68 d8 7f 44 30               PUSH 0x30447fd8 | 30039137   68 d8 7f 44 30               PUSH 0x30447fd8
 * The stores at 0x3002df1a/0x30039144 write the final terminator byte of the
 * same 256-byte string, not a separately consumed dirty flag. */
char cg_timeoutString[256] = { 0 };
/* Source: uo_cgame_mp_x86.dll 0x304480d8 (.data): first signed HUD-stat sibling,
 * initialized from config string 14 by Q_atoi. */
int32_t cg_hudStat14Value = 0;
/* Source: uo_cgame_mp_x86.dll 0x304480dc (.data); refs=4 width=4; first=0x30031521.
 * cg_hudStat5Value — middle sibling of the parallel HUD-stat int array
 * d8/dc/e0 (see globals.h). Signed, compared against the sentinel -9999 by its
 * display emitter CG_DrawRedScore (0x30031521); mechanical
 * owner=menuparse_itemdef label rejected. Zero-initialized .data.
 * Example: 30031521   a1 dc 80 44 30               MOV EAX,[0x304480dc] | 30031b60   a1 dc 80 44 30               MOV EAX,[0x304480dc] | 3003885d   a3 dc 80 44 30               MOV [0x304480dc],EAX
 */
int32_t cg_hudStat5Value = 0;
/* Source: uo_cgame_mp_x86.dll 0x304480e0 (.data); refs=4 width=4; first=0x300315f1.
 * cg_hudStat6Value — atoi-parsed system-info HUD integer stat (see globals.h).
 * Zero-initialized; the mechanical owner=script_orbit label is rejected (HUD state).
 * Example: 300315f1 MOV EAX,[0x304480e0] | 30031bd0 MOV EAX,[0x304480e0] | 3003887b MOV [0x304480e0],EAX
 */
int32_t cg_hudStat6Value = 0;
/* Source: uo_cgame_mp_x86.dll 0x304480e4..0x304484e3 (.data).
 * cg_gameModels[256] — registered handles for the CS_MODELS block. The graphics
 * registration loop at 0x3002c6fd starts with config string 0x196 and destination
 * 0x304480e8, proving that it fills elements 1..255 and leaves element 0 empty.
 * vmMain command 10 and four entity render paths index the same base directly as
 * [index*4 + 0x304480e4]. The previous split scalar at 0x304480e4 plus an array at
 * 0x304480e8 was therefore one element late. */
qhandle_t cg_gameModels[CS_MODELS_COUNT] = { 0 };
/* Source: uo_cgame_mp_x86.dll 0x304484e4..0x30448624 (.data); the cgame effect-handle
 * table cg_effectDefs[80] (see globals.h). Consumers index cg_effectDefs[id] for
 * 0 < id < 80 (0x300164f1, 0x30021b17, 0x3002272f); the registration loop at
 * 0x3002c760 writes engine effect handles into it starting at &cg_effectDefs[1]
 * (0x304484e8). Zero-initialized storage; filled at runtime by the registration
 * pass. Supersedes the mechanical g_data_bg_indexforstring_304484e4 and the
 * spuriously split element-[1] symbol g_data_pm_updateviewangles_304484e8.
 */
uint32_t cg_effectDefs[80];
/* Source: uo_cgame_mp_x86.dll 0x30448624 (.data); 124-byte shellshock_t.
 * cg_consoleShellShock: the manual "cg_shellshock" console-command parameter
 * block (declared in client_recovered.h). Consumers: CG_ShellShock_f
 * (0x3001755f fills it via CG_SetShellShockParams), scene reader 0x30042160
 * (0x30042508 uses it as the active manual shellshock), and 0x3003921c.
 * The mechanical owner=concatargs label was a wrong size-guess name for
 * 0x300174b0. Zero-initialized .data. */
shellshock_t cg_consoleShellShock;
/* Source: uo_cgame_mp_x86.dll 0x304486a0 (.data); refs=1 width=imm; first=0x3002c7af; owner=pm_updateviewangles.
 * Example: 3002c7af   be a0 86 44 30               MOV ESI,0x304486a0
 */
shellshock_t cg_shellShocks[15];
/* Source: uo_cgame_mp_x86.dll 0x30448de4 (.data); refs=2 width=4; first=0x3002c641; owner=pm_updateviewangles.
 * Example: 3002c641   a3 e4 8d 44 30               MOV [0x30448de4],EAX | 3002c6b0   a1 e4 8d 44 30               MOV EAX,[0x30448de4]
 */
int32_t cg_numInlineModels = 0;
/* Source: uo_cgame_mp_x86.dll 0x30448de8..0x304495e7 (.data); base of an
 * MAX_SUBMODELS-row int32 array indexed
 * by currentState.modelindex. Renamed from the mechanical g_data_matrixmultiply43_30448de8
 * (owner label = the rejected size-guess name of CG_Mover at 0x3001f120; the export
 * captured only the first dword). This is cg_inlineModelHandles[], the registered
 * handle table for "*N" inline/brush models (see globals.h). Writers/readers:
 * 3001f22e MOV ECX,[EAX*4+0x30448de8] (CG_Mover read) | 3001f388 MOV EAX,[EDX*4+0x30448de8]
 * | 3002c67b MOV [EBP*4+0x30448de8],EAX (registration store).
 * The next proven datum begins at 0x304495e8, so the complete 0x800-byte
 * interval is one 512-entry table. */
int32_t cg_inlineModelHandles[MAX_SUBMODELS];
/* Source: uo_cgame_mp_x86.dll 0x304495e8..0x3044ade8 (.data).
 * cgs.inlineModelMidpoints[MAX_SUBMODELS]. CG_RegisterGraphics starts its loop at
 * model 1, hence the immediate 0x304495f4 == &array[1], advances by 0xc per model,
 * and is bounded by cg_numInlineModels.  CG_EntityEffects indexes the same base
 * at 0x304495e8 with modelindex * 0xc.  The exact interval is 512 vec3_t entries. */
vec3_t cg_inlineModelMidpoints[MAX_SUBMODELS];
/* Source: uo_cgame_mp_x86.dll 0x3044ade8 (.data); refs=3; owner=reached_binarymover.
 * cgs.teamChatMsgs — team-chat scroll ring, TEAMCHAT_HEIGHT lines of
 * TEAMCHAT_LINE_BYTES (271 = stride 0x10f). Written by CG_AddToTeamChat
 * (0x30039390); read by the team-info drawer (0x30018770). Mechanically captured
 * as one uint32_t; the 0x10f stride over an 8-entry mod ring proves the array. */
char teamChatMsgs[TEAMCHAT_HEIGHT][TEAMCHAT_LINE_BYTES] = { { 0 } };
/* Source: uo_cgame_mp_x86.dll 0x3044b660 (.data); refs=4 width=4; owner=reached_binarymover.
 * cgs.teamChatMsgTimes[TEAMCHAT_HEIGHT] — cg.time stamp per ring line, indexed
 * [teamChatPos % chatHeight]. */
int teamChatMsgTimes[TEAMCHAT_HEIGHT] = { 0 };
/* Source: uo_cgame_mp_x86.dll 0x3044b680 (.data); refs=11 width=4; owner=reached_binarymover.
 * cgs.teamChatPos — monotonic ring write cursor (indexed modulo chatHeight). */
int teamChatPos = 0;
/* Source: uo_cgame_mp_x86.dll 0x3044b684 (.data); refs=6 width=4; owner=reached_binarymover.
 * cgs.teamChatLastPos — oldest visible ring line; trails teamChatPos by <=chatHeight. */
int teamChatLastPos = 0;
/* The cgs cursor coordinates are copied into g_uiDCInstance.cursorx/cursory by
 * vmMain's CGVM_MOUSE_EVENT command. Their placement immediately after the
 * cgs.teamChat* fields identifies them as the trailing cursor state in cgs. */
/* Source: uo_cgame_mp_x86.dll 0x3044b688 (.data). */
int32_t cgs_cursorX = 0;
/* Source: uo_cgame_mp_x86.dll 0x3044b68c (.data). */
int32_t cgs_cursorY = 0;
/* Source: uo_cgame_mp_x86.dll 0x3044b69c..0x3044b6a8 (.data); the FOV-zoom fade
 * animator (see cgFovFade_t in globals.h). The mechanical export split it into
 * four uint32 g_data_vectosignedangles_* scalars (wrong first-touch owner);
 * superseded here as one 16-byte struct. Writers: CG_StartFovFade (0x3001ab50),
 * CG_CalcFov (0x3003ffc0). Reader/evaluator: 0x3001a7c0. Zero-initialized. */
cgFovFade_t cg_fovFade = { 0.0f, 0.0f, 0, 0 };
/* Source: uo_cgame_mp_x86.dll 0x3044b6ac (.data); refs=10 width=4; first=0x30018cfa.
 * (owner=cmd_veh_fireturret label is wrong.)
 * cgs.media hudSoftLine shader handle. Registered at 0x3002dfcc as
 *   MOV [0x3044b6ac], EAX  immediately after the shader lookup for the string
 *   "hudSoftLine" (g_string_hudsoftline) returns its handle in EAX. Consumed
 * as the qhandle_t argument to trap_R_DrawStretchPic by the HUD fill/soft-line draw
 * family: CG_FillRect (0x3001c4e0) and the line drawers at 0x3001c8e0/0x3001c980/
 * 0x3001ca20. Zero-initialized .data (BSS tail); the engine fills it at asset
 * registration. */
qhandle_t cgs_media_whiteShader = 0;       /* 0x3044b6ac */
qhandle_t cgs_media_hudSoftLineShader = 0; /* 0x3044b6b0 */
qhandle_t cgs_media_hudSoftLineHShader = 0;/* 0x3044b6b4 */
/* Source: uo_cgame_mp_x86.dll 0x3044b6b8 (.data); refs=1 width=4; first=0x3002c4d6; owner=pm_updateviewangles.
 * Example: 3002c4d6   a3 b8 b6 44 30               MOV [0x3044b6b8],EAX
 */
qhandle_t cgs_media_hudAxisIcon = 0;
/* Source: uo_cgame_mp_x86.dll 0x3044b6bc (.data); refs=1 width=4; first=0x3002c4c5; owner=pm_updateviewangles.
 * Example: 3002c4c5   a3 bc b6 44 30               MOV [0x3044b6bc],EAX
 */
qhandle_t cgs_media_hudAlliedIcon = 0;
/* Source: uo_cgame_mp_x86.dll 0x3044b6c0 (.data); refs=3 width=4; first=0x30017e23.
 * cgs.media hudColorBar shader handle (owner=cg_initvote label is wrong). Registered
 * at 0x3002c4b4 with the qhandle_t returned by the "hudColorBar" shader lookup
 * (string 0x30078048). Consumed by CG_DrawTeamBackground (0x30017e23) and 0x3001890c.
 * Example: 30017e23 MOV EAX,[0x3044b6c0] | 3001890c MOV ECX,[0x3044b6c0] | 3002c4b4 MOV [0x3044b6c0],EAX
 */
qhandle_t cgs_media_hudColorBar = 0;
/* Source: uo_cgame_mp_x86.dll 0x3044b6c8 (.data); refs=2 width=4; first=0x3002c567.
 * cg_railCoreShader — qhandle_t of the "railCore" shader. Written once by the
 * cgame media registration (0x3002c567: EAX = trap_R_RegisterShader("railCore", 2)),
 * read by the rail-trail segment builder (0x30043107 -> refEntity.spriteShaderHandle).
 * Name resolved from the registered shader string; supersedes owner=pm_updateviewangles.
 */
qhandle_t cg_railCoreShader = 0;
/* Source: uo_cgame_mp_x86.dll 0x3044b6cc (.data); refs=3 width=4; first=0x3002c54d; owner=pm_updateviewangles.
 * Example: 3002c54d   a3 cc b6 44 30               MOV [0x3044b6cc],EAX | 3003a02f   8b 0d cc b6 44 30            MOV ECX,dword ptr [0x3044b6cc] | 3003a0bf   a1 cc b6 44 30               MOV EAX,[0x3044b6cc]
 */
/* cgs_voiceChatIcon: "headiconVoiceChat" shader handle (owner label above is the
 * mechanical first-touch and is incorrect). See globals.h. */
/* Source: uo_cgame_mp_x86.dll 0x3044b6cc. */
uint32_t cgs_voiceChatIcon = (uint32_t)0x00000000u;
/* cgs_talkBalloonIcon: "headiconTalkBalloon" shader handle (owner label above is
 * the mechanical first-touch and is incorrect). See globals.h.
 * Source: uo_cgame_mp_x86.dll 0x3044b6d0 (.data); refs=2 width=4; first=0x3002c55d.
 * Example: 3002c55d   a3 d0 b6 44 30               MOV [0x3044b6d0],EAX | 30032c0b   a1 d0 b6 44 30               MOV EAX,[0x3044b6d0]
 */
qhandle_t cgs_talkBalloonIcon = (qhandle_t)0;
/* cgs_disconnectedIcon: "headiconDisconnected" shader handle (owner label above is
 * the mechanical first-touch and is incorrect). See globals.h.
 * Source: uo_cgame_mp_x86.dll 0x3044b6d4 (.data); refs=2 width=4; first=0x3002bb25.
 * Example: 3002bb25   a3 d4 b6 44 30               MOV [0x3044b6d4],EAX | 30032ba8   a1 d4 b6 44 30               MOV EAX,[0x3044b6d4]
 */
qhandle_t cgs_disconnectedIcon = (qhandle_t)0;
/* cgs_youInKillCamIcon: "headiconYouInKillCam" shader handle (owner label above is
 * the mechanical first-touch and is incorrect). See globals.h.
 * Source: uo_cgame_mp_x86.dll 0x3044b6d8 (.data); refs=2 width=4; first=0x3002bb36.
 * Example: 3002bb36   a3 d8 b6 44 30               MOV [0x3044b6d8],EAX | 30032b7f   8b 15 d8 b6 44 30            MOV EDX,dword ptr [0x3044b6d8]
 */
qhandle_t cgs_youInKillCamIcon = (qhandle_t)0;
/* Source: uo_cgame_mp_x86.dll 0x3044b6dc (.data); refs=1 width=4; first=0x3002bbee; owner=pm_updateviewangles.
 * Example: 3002bbee   a3 dc b6 44 30               MOV [0x3044b6dc],EAX
 */
qhandle_t cgs_media_selectShader = 0;
/* Source: uo_cgame_mp_x86.dll 0x3044b6e4 (.data); refs=3 width=4; first=0x3002bbdd; owner=pm_updateviewangles.
 * Example: 3002bbdd   a3 e4 b6 44 30               MOV [0x3044b6e4],EAX | 30048706   8b 0d e4 b6 44 30            MOV ECX,dword ptr [0x3044b6e4] | 30048870   a1 e4 b6 44 30               MOV EAX,[0x3044b6e4]
 */
qhandle_t cgs_media_tracerShader = 0;
/* Source: uo_cgame_mp_x86.dll 0x3044b6e8 (.data); refs=1 width=4; first=0x3002bb03; owner=pm_updateviewangles.
 * Example: 3002bb03   a3 e8 b6 44 30               MOV [0x3044b6e8],EAX
 */
qhandle_t cgs_media_lagometerShader = 0;
/* Source: uo_cgame_mp_x86.dll 0x3044b6ec (.data); refs=2 width=4; first=0x30018be9; owner=cmd_veh_fireturret.
 * Example: 30018be9   a1 ec b6 44 30               MOV EAX,[0x3044b6ec] | 3002bb14   a3 ec b6 44 30               MOV [0x3044b6ec],EAX
 */
qhandle_t cgs_lagometerShader = 0;
/* Source: uo_cgame_mp_x86.dll 0x3044b6f0 (.data); refs=5 width=4; first=0x3001d1a8; owner=initweaponinfo.
 * Example: 3001d1a8   a1 f0 b6 44 30               MOV EAX,[0x3044b6f0] | 3001d1ba   a1 f0 b6 44 30               MOV EAX,[0x3044b6f0] | 3001d1cf   a1 f0 b6 44 30               MOV EAX,[0x3044b6f0]
 */
qhandle_t cgs_media_backTileShader = 0; /* 0x3044b6f0; resolved from CG_TileClear (0x3001d160): registered via CG_RegisterShader (0x3003db80) at 0x3002c481, read 5x by CG_TileClear as the background-tile shader handle. */
/* Source: uo_cgame_mp_x86.dll 0x3044b6f4 (.data); refs=2 width=4; first=0x3002c492; owner=pm_updateviewangles.
 * Example: 3002c492   a3 f4 b6 44 30               MOV [0x3044b6f4],EAX | 30046cc9   8b 15 f4 b6 44 30            MOV EDX,dword ptr [0x3044b6f4]
 */
qhandle_t cgs_media_hudNoWeaponIcon = 0;
/* Source: uo_cgame_mp_x86.dll 0x3044b6f8..0x3044b720 (.data).
 * CG_DrawCursorhint indexes this same shared cursor-hint range; the eight
 * registered shader stores prove elements 2..9 and elements 0..1 remain zero. */
qhandle_t cgs_media_usableHintShaders[CURSOR_HINT_BUILTIN_ICON_COUNT] = { 0 };
/* Source: uo_cgame_mp_x86.dll 0x3044b720 (.data); refs=3 width=4; first=0x3002bc8a; owner=pm_updateviewangles.
 * Example: 3002bc8a   a3 20 b7 44 30               MOV [0x3044b720],EAX | 300446d7   89 04 9d 20 b7 44 30         MOV dword ptr [EBX*0x4 + 0x3044b720],EAX | 300446e6   89 0c 9d 20 b7 44 30         MOV dword ptr [EBX*0x4 + 0x3044b720],ECX
 */
qhandle_t cg_weaponHudIcons[MAX_WEAPONS] = { 0 };
/* Source: uo_cgame_mp_x86.dll 0x3044b920 (.data); refs=2 width=4; first=0x3004473b; owner=bg_checkpronevalid.
 * Example: 3004473b   89 04 9d 20 b9 44 30         MOV dword ptr [EBX*0x4 + 0x3044b920],EAX | 3004474a   89 14 9d 20 b9 44 30         MOV dword ptr [EBX*0x4 + 0x3044b920],EDX
 */
qhandle_t cg_weaponAmmoIcons[MAX_WEAPONS] = { 0 };
/* Source: uo_cgame_mp_x86.dll 0x3044bb24..0x3044bb47 (.data); nine consecutive
 * 4-byte qhandle_t slots. Consolidated from the mechanical
 * g_data_pm_updateviewangles_3044bb{24,28,2c,30,34,38,3c,40,44} (that owner label
 * was a wrong size-guess). Written once by the HUD stance/fatigue asset setup at
 * 0x3002bc9b..0x3002bd26, each entry filled by trap_R_RegisterShader(name, 5):
 *   [0]=hudStanceStand   [1]=hudStanceCrouch [2]=hudStanceProne [3]=hudStanceSprint
 *   [4]=hudStanceFlash   [5]=hudFatigueStand [6]=hudFatigueCrouch
 *   [7]=hudFatigueProne  [8]=hudFatigueSprint
 * Read as indexed handles by CG_DrawPlayerStance (0x3002f63b..0x3002f8db). */
qhandle_t cg_stanceHudShaders[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
/* Source: uo_cgame_mp_x86.dll 0x3044bb48 (.data); refs=1 width=4; first=0x3002bd2b; owner=pm_updateviewangles.
 * Example: 3002bd2b   c7 05 48 bb 44 30 00 00 00 00 MOV dword ptr [0x3044bb48],0x0
 */
qhandle_t cg_hudObjectiveReserved = 0;
/* Source: uo_cgame_mp_x86.dll 0x3044bb4c..0x3044bb57 (.data); three consecutive
 * 4-byte qhandle_t slots (0x3044bb4c/0x50/0x54). Consolidated from the mechanical
 * g_data_pm_updateviewangles_3044bb4c/50/54 placeholders (owner label was a
 * size-guess and wrong). Written by objective-asset setup at 0x3002bd41/bd52/bd63 via
 * trap_R_RegisterShader(name,5); read as an array by CG_GetObjectiveShaderForDir
 * (0x3002fe50 MOV EAX,[ESI*4 + 0x3044bb4c]).
 *   3002bd41 MOV [0x3044bb4c],EAX  (= RegisterShader("hudObjective"))
 *   3002bd52 MOV [0x3044bb50],EAX  (= RegisterShader("hudObjectiveUp"))
 *   3002bd63 MOV [0x3044bb54],EAX  (= RegisterShader("hudObjectiveDown"))
 */
qhandle_t cg_objectiveShaders[3] = { 0, 0, 0 };
/* Source: uo_cgame_mp_x86.dll 0x3044bb58 (.data); refs=2 width=4; first=0x30016b9d; owner=cg_asset_parse.
 * Example: 30016b9d   8b 14 b5 58 bb 44 30         MOV EDX,dword ptr [ESI*0x4 + 0x3044bb58] | 3002bd77   a3 58 bb 44 30               MOV [0x3044bb58],EAX
 */
qhandle_t cg_compassFriendlyShaders[2] = { 0, 0 };
/* Source: uo_cgame_mp_x86.dll 0x3044bb5c (.data); refs=2 width=4; first=0x30016b80; owner=cg_asset_parse.
 * Example: 30016b80   8b 15 5c bb 44 30            MOV EDX,dword ptr [0x3044bb5c] | 3002bd88   a3 5c bb 44 30               MOV [0x3044bb5c],EAX
 */
/* Source: uo_cgame_mp_x86.dll 0x3044bb60 (.data); refs=2 width=4; first=0x30017193; owner=cg_drawcompasstanks.
 * Example: 30017193   8b 0d 60 bb 44 30            MOV ECX,dword ptr [0x3044bb60] | 3002bd92   a3 60 bb 44 30               MOV [0x3044bb60],EAX
 */
qhandle_t cg_compassTankShaders[3] = { 0, 0, 0 };
/* Source: uo_cgame_mp_x86.dll 0x3044bb64 (.data); refs=2 width=4; first=0x300171c7; owner=cg_drawcompasstanks.
 * Example: 300171c7   a1 64 bb 44 30               MOV EAX,[0x3044bb64] | 3002bdaa   a3 64 bb 44 30               MOV [0x3044bb64],EAX
 */
/* Source: uo_cgame_mp_x86.dll 0x3044bb68 (.data); refs=2 width=4; first=0x300171ad; owner=cg_drawcompasstanks.
 * Example: 300171ad   8b 15 68 bb 44 30            MOV EDX,dword ptr [0x3044bb68] | 3002bdbb   a3 68 bb 44 30               MOV [0x3044bb68],EAX
 */
/* Source: uo_cgame_mp_x86.dll 0x3044bb6c (.data); refs=2 width=4; first=0x3001aac9; owner=cg_ejectweaponbrass.
 * Example: 3001aac9   8b 0d 6c bb 44 30            MOV ECX,dword ptr [0x3044bb6c] | 3002bdc5   a3 6c bb 44 30               MOV [0x3044bb6c],EAX
 */
qhandle_t cg_hitDirectionShader = 0; /* 0x3044bb6c; "hudHitDirection" 2D shader handle. See globals.h. */
/* Source: uo_cgame_mp_x86.dll 0x3044bb70 (.data); first=0x30017cfb.
 * cg_numberShaders[11] — HUD bitmap-number shader handles ('0'..'9', then '-');
 * registered by the loop at 0x3002bad1 and read by the digit drawer 0x30031300.
 * See globals.h. The mechanical export captured only element 0; superseded as the
 * full qhandle_t[11] array. owner=pm_cmdscale (a first-toucher) rejected.
 * Example: 3002badf   89 86 70 bb 44 30            MOV dword ptr [ESI + 0x3044bb70],EAX | 3003146f   8b 0c b5 70 bb 44 30         MOV ECX,dword ptr [ESI*0x4 + 0x3044bb70]
 */
qhandle_t cg_numberShaders[11] = {0};
/* Source: uo_cgame_mp_x86.dll 0x3044bb9c (.data); refs=2 width=4; first=0x3002c5e4; owner=pm_updateviewangles.
 * Example: 3002c5e4   a3 9c bb 44 30               MOV [0x3044bb9c],EAX | 30032d55   8b 15 9c bb 44 30            MOV EDX,dword ptr [0x3044bb9c]
 */
qhandle_t cgs_media_markShadowShader = 0;
/* Source: uo_cgame_mp_x86.dll 0x3044bba0 (.data); refs=1 width=4; first=0x3002c4a3; owner=pm_updateviewangles.
 * Example: 3002c4a3   a3 a0 bb 44 30               MOV [0x3044bba0],EAX
 */
qhandle_t cgs_media_flareShader = 0;
/* Source: uo_cgame_mp_x86.dll 0x3044bba4 (.data); refs=2 width=4; first=0x3002c5fd; owner=pm_updateviewangles.
 * Example: 3002c5fd   a3 a4 bb 44 30               MOV [0x3044bba4],EAX | 30032f3e   a1 a4 bb 44 30               MOV EAX,[0x3044bba4]
 */
qhandle_t cgs_media_wakeMarkShader = 0;
/* Source: uo_cgame_mp_x86.dll 0x3044bba8 (.data); refs=1 width=4; first=0x3002c503; owner=pm_updateviewangles.
 * Example: 3002c503   a3 a8 bb 44 30               MOV [0x3044bba8],EAX
 */
qhandle_t cgs_media_headiconAlliesFlag = 0;
/* Source: uo_cgame_mp_x86.dll 0x3044bbac (.data); refs=1 width=4; first=0x3002c4e7; owner=pm_updateviewangles.
 * Example: 3002c4e7   a3 ac bb 44 30               MOV [0x3044bbac],EAX
 */
qhandle_t cgs_media_headiconAxisFlag = 0;
/* Source: uo_cgame_mp_x86.dll 0x3044bbb0 (.data); refs=1 width=4; first=0x3002b615; owner=playercmd_finishplayerdamage.
 * Example: 3002b615   a3 b0 bb 44 30               MOV [0x3044bbb0],EAX
 */
const char *cg_soundPlayerGib = NULL;
/* Source: uo_cgame_mp_x86.dll 0x3044bbb4 (.data); refs=1 width=4; first=0x3002b62a; owner=playercmd_finishplayerdamage.
 * Example: 3002b62a   a3 b4 bb 44 30               MOV [0x3044bbb4],EAX
 */
const char *cg_soundPlayerGibBounce = NULL;
/* Source: uo_cgame_mp_x86.dll 0x3044bbb8 (.data); refs=3 width=4; first=0x30022eab.
 * cg_soundOutOfAmmo: the registered "player_out_of_ammo" local-sound identifier.
 * The cgame sound-registration pass (0x3002b560) registers "player_out_of_ammo"
 * (string 0x30078794) via engine syscall 0xc3 and stores the returned handle here:
 * the store MOV [0x3044bbb8],EAX at 0x3002b654 captures the result of the syscall
 * at 0x3002b644 whose pushed argument was that string. CG_OutOfAmmoChange
 * (0x30034a00) loads it and hands it to CG_PlaySoundAliasByName as the sound identifier.
 * Retyped from the mechanical uint32_t (owner=cmd_callvote_f was the first-touching
 * function, not the identity) to const char * to match CG_PlaySoundAliasByName.
 * Example: 30034a8a   a1 b8 bb 44 30               MOV EAX,[0x3044bbb8]
 */
const char *cg_soundOutOfAmmo = NULL;
/* Source: uo_cgame_mp_x86.dll 0x3044bbbc (.data); refs=2 width=4; first=0x30022a4f; owner=cmd_callvote_f.
 * Example: 30022a4f   8b 0d bc bb 44 30            MOV ECX,dword ptr [0x3044bbbc] | 3002b67e   a3 bc bb 44 30               MOV [0x3044bbbc],EAX
 */
const char *cg_soundLandDamage = NULL;
/* Source: uo_cgame_mp_x86.dll 0x3044bbc0 (.data); refs=1 width=4; first=0x300234bc; owner=cmd_callvote_f.
 * Example: 300234bc   a1 c0 bb 44 30               MOV EAX,[0x3044bbc0]
 */
const char *cg_soundDeath = NULL;
/* Source: uo_cgame_mp_x86.dll 0x3044bbc4 (.data); refs=1 width=4; first=0x3002318f; owner=cmd_callvote_f.
 * Example: 3002318f   8b 0d c4 bb 44 30            MOV ECX,dword ptr [0x3044bbc4]
 */
const char *cg_soundPlayerTeleportIn = NULL;
/* Source: uo_cgame_mp_x86.dll 0x3044bbc8 (.data); refs=1 width=4; first=0x300231a9; owner=cmd_callvote_f.
 * Example: 300231a9   a1 c8 bb 44 30               MOV EAX,[0x3044bbc8]
 */
const char *cg_soundPlayerTeleportOut = NULL;
/* Source: uo_cgame_mp_x86.dll 0x3044bbcc (.data); refs=1 width=4; first=0x300231ce; owner=cmd_callvote_f.
 * Example: 300231ce   8b 15 cc bb 44 30            MOV EDX,dword ptr [0x3044bbcc]
 */
const char *cg_soundItemRespawn = NULL;
/* Source: uo_cgame_mp_x86.dll 0x3044bbd0 (.data); refs=4 width=4; first=0x3001ba2b; owner=cg_drawtracer.
 * Example: 3001ba2b   a1 d0 bb 44 30               MOV EAX,[0x3044bbd0] | 3002b669   a3 d0 bb 44 30               MOV [0x3044bbd0],EAX | 3003ade6   8b 15 d0 bb 44 30            MOV EDX,dword ptr [0x3044bbd0]
 */
const char *cgs_media_playerTalkSound = 0;
/* Source: uo_cgame_mp_x86.dll 0x3044bbd4 (.data); refs=2 width=4; first=0x30023257; owner=cmd_callvote_f.
 * Example: 30023257   8b 04 85 d4 bb 44 30         MOV EAX,dword ptr [EAX*0x4 + 0x3044bbd4] | 3002b7a0   bf d4 bb 44 30               MOV EDI,0x3044bbd4
 */
const char *cg_grenadeBounceSurfaceSounds[23];
/* Source: uo_cgame_mp_x86.dll 0x3044bc30 (.data); refs=2 width=4; first=0x30023417; owner=cmd_callvote_f.
 * Example: 30023417   8b 04 bd 30 bc 44 30         MOV EAX,dword ptr [EDI*0x4 + 0x3044bc30] | 3002b7b4   bf 30 bc 44 30               MOV EDI,0x3044bc30
 */
const char *cg_grenadeExplodeSurfaceSounds[23];
/* Source: uo_cgame_mp_x86.dll 0x3044bc8c..0x3044bce8 (.data).
 * One dormant 23-surface handle bank in the contiguous media layout. */
const char *cg_unusedSurfaceSoundSet0[23];
/* Source: uo_cgame_mp_x86.dll 0x3044bce8 (.data); refs=1 width=imm; first=0x3002b7c3; owner=playercmd_finishplayerdamage.
 * Example: 3002b7c3   bf e8 bc 44 30               MOV EDI,0x3044bce8
 */
const char *cg_rocketExplodeSurfaceSounds[23];
/* Source: uo_cgame_mp_x86.dll 0x3044bd44..0x3044bda0 (.data).
 * One dormant 23-surface handle bank in the contiguous media layout. */
const char *cg_unusedSurfaceSoundSet1[23];
/* Source: uo_cgame_mp_x86.dll 0x3044bda0 (.data); refs=1 width=imm; first=0x3002b7e1; owner=playercmd_finishplayerdamage.
 * Example: 3002b7e1   bf a0 bd 44 30               MOV EDI,0x3044bda0
 */
const char *cg_artilleryExplodeSurfaceSounds[23];
/* Source: uo_cgame_mp_x86.dll 0x3044bdfc (.data); refs=1 width=imm; first=0x3002b7d2; owner=playercmd_finishplayerdamage.
 * Example: 3002b7d2   bf fc bd 44 30               MOV EDI,0x3044bdfc
 */
const char *cg_mortarExplodeSurfaceSounds[23];
/* Source: uo_cgame_mp_x86.dll 0x3044be58 (.data); refs=1 width=imm; first=0x3002b7f0; owner=playercmd_finishplayerdamage.
 * Example: 3002b7f0   bf 58 be 44 30               MOV EDI,0x3044be58
 */
const char *cg_tankExplodeSurfaceSounds[23];
/* Source: uo_cgame_mp_x86.dll 0x3044beb4..0x3044bf10 (.data).
 * A 23-surface handle bank. CG_EntityEvent indexes it through the compiler's
 * base-minus-four form [event*4 + 0x3044bf0c] for event values 0x46..0x5c. */
const char *cg_eventSurfaceSounds[23];
/* Source: uo_cgame_mp_x86.dll 0x3044bf10 (.data); refs=3 width=4; first=0x3002b7ff; owner=playercmd_finishplayerdamage.
 * Example: 3002b7ff   bf 10 bf 44 30               MOV EDI,0x3044bf10 | 30048f11   8b 04 95 10 bf 44 30         MOV EAX,dword ptr [EDX*0x4 + 0x3044bf10] | 3004906b   8b 04 b5 10 bf 44 30         MOV EAX,dword ptr [ESI*0x4 + 0x3044bf10]
 */
const char *cg_bulletSmallSurfaceSounds[23];
/* Source: uo_cgame_mp_x86.dll 0x3044bf6c (.data); refs=3 width=4; first=0x3002b80e; owner=playercmd_finishplayerdamage.
 * Example: 3002b80e   bf 6c bf 44 30               MOV EDI,0x3044bf6c | 30048f04   8b 04 95 6c bf 44 30         MOV EAX,dword ptr [EDX*0x4 + 0x3044bf6c] | 30049074   8b 04 b5 6c bf 44 30         MOV EAX,dword ptr [ESI*0x4 + 0x3044bf6c]
 */
const char *cg_bulletLargeSurfaceSounds[23];
/* Source: uo_cgame_mp_x86.dll 0x3044bfc8 (.data); refs=1 width=imm; first=0x3002b81d; owner=playercmd_finishplayerdamage.
 * Example: 3002b81d   bf c8 bf 44 30               MOV EDI,0x3044bfc8
 */
const char *cg_stepSprintSurfaceSounds[23];
/* Source: uo_cgame_mp_x86.dll 0x3044c024 (.data); refs=1 width=imm; first=0x3002b82c; owner=playercmd_finishplayerdamage.
 * Example: 3002b82c   bf 24 c0 44 30               MOV EDI,0x3044c024
 */
const char *cg_stepRunSurfaceSounds[23];
/* Source: uo_cgame_mp_x86.dll 0x3044c080 (.data); refs=1 width=imm; first=0x3002b83b; owner=playercmd_finishplayerdamage.
 * Example: 3002b83b   bf 80 c0 44 30               MOV EDI,0x3044c080
 */
const char *cg_stepWalkSurfaceSounds[23];
/* Source: uo_cgame_mp_x86.dll 0x3044c0dc (.data); refs=1 width=imm; first=0x3002b84a; owner=playercmd_finishplayerdamage.
 * Example: 3002b84a   bf dc c0 44 30               MOV EDI,0x3044c0dc
 */
const char *cg_stepProneSurfaceSounds[23];
/* Source: uo_cgame_mp_x86.dll 0x3044c138 (.data); refs=1 width=imm; first=0x3002b859; owner=playercmd_finishplayerdamage.
 * Example: 3002b859   bf 38 c1 44 30               MOV EDI,0x3044c138
 */
const char *cg_shellFlashSurfaceSounds[23];
/* Source: uo_cgame_mp_x86.dll 0x3044c194 (.data); refs=2 width=4; first=0x300232fa; owner=cmd_callvote_f.
 * Example: 300232fa   8b 04 b5 94 c1 44 30         MOV EAX,dword ptr [ESI*0x4 + 0x3044c194] | 3002b86a   89 35 94 c1 44 30            MOV dword ptr [0x3044c194],ESI
 */
const char *cg_shellFlashSounds[8];
/* Source: uo_cgame_mp_x86.dll 0x3044c1b4 (.data); refs=2 width=4; first=0x3002332f; owner=cmd_callvote_f.
 * Example: 3002332f   8b 04 b5 b4 c1 44 30         MOV EAX,dword ptr [ESI*0x4 + 0x3044c1b4] | 3002b8b0   89 35 b4 c1 44 30            MOV dword ptr [0x3044c1b4],ESI
 */
const char *cg_barrageIncomingSounds[8];
/* Source: uo_cgame_mp_x86.dll 0x3044c1d4 (.data); refs=2 width=4; first=0x30022910; owner=cmd_callvote_f.
 * Example: 30022910   a1 d4 c1 44 30               MOV EAX,[0x3044c1d4] | 3002b8ff   a3 d4 c1 44 30               MOV [0x3044c1d4],EAX
 */
const char *cg_soundGearRattleSprint = NULL;
/* Source: uo_cgame_mp_x86.dll 0x3044c1d8 (.data); refs=3 width=4; first=0x300228c4; owner=cmd_callvote_f.
 * Example: 300228c4   a1 d8 c1 44 30               MOV EAX,[0x3044c1d8] | 300229bc   a1 d8 c1 44 30               MOV EAX,[0x3044c1d8] | 3002b914   a3 d8 c1 44 30               MOV [0x3044c1d8],EAX
 */
const char *cg_soundGearRattleRun = NULL;
/* Source: uo_cgame_mp_x86.dll 0x3044c1dc (.data); refs=2 width=4; first=0x3002295c; owner=cmd_callvote_f.
 * Example: 3002295c   a1 dc c1 44 30               MOV EAX,[0x3044c1dc] | 3002b929   a3 dc c1 44 30               MOV [0x3044c1dc],EAX
 */
const char *cg_soundGearRattleWalk = NULL;
/* Source: uo_cgame_mp_x86.dll 0x3044c1e0 (.data); refs=2 width=4; first=0x30022b09; owner=cmd_callvote_f.
 * Example: 30022b09   8b 0d e0 c1 44 30            MOV ECX,dword ptr [0x3044c1e0] | 3002b93e   a3 e0 c1 44 30               MOV [0x3044c1e0],EAX
 */
const char *cg_soundMovementFoliage = NULL;
/* Source: uo_cgame_mp_x86.dll 0x3044c1e4 (.data); refs=2 width=4; first=0x3002b953; owner=playercmd_finishplayerdamage.
 * Example: 3002b953   a3 e4 c1 44 30               MOV [0x3044c1e4],EAX | 30048203   a1 e4 c1 44 30               MOV EAX,[0x3044c1e4]
 */
const char *cg_soundWhizby = NULL;
/* Source: uo_cgame_mp_x86.dll 0x3044c1e8 (.data); refs=2 width=4; first=0x3002312e; owner=cmd_callvote_f.
 * Example: 3002312e   8b 15 e8 c1 44 30            MOV EDX,dword ptr [0x3044c1e8] | 3002b9d4   a3 e8 c1 44 30               MOV [0x3044c1e8],EAX
 */
const char *cg_soundMeleeSwingLarge = NULL;
/* Source: uo_cgame_mp_x86.dll 0x3044c1ec (.data); refs=2 width=4; first=0x30023115; owner=cmd_callvote_f.
 * Example: 30023115   a1 ec c1 44 30               MOV EAX,[0x3044c1ec] | 3002b9e9   a3 ec c1 44 30               MOV [0x3044c1ec],EAX
 */
const char *cg_soundMeleeSwingSmall = NULL;
/* Source: uo_cgame_mp_x86.dll 0x3044c1f0 (.data); refs=2 width=4; first=0x30023157; owner=cmd_callvote_f.
 * Example: 30023157   8b 0d f0 c1 44 30            MOV ECX,dword ptr [0x3044c1f0] | 3002b9fe   a3 f0 c1 44 30               MOV [0x3044c1f0],EAX
 */
const char *cg_soundMeleeHit = NULL;
/* Source: uo_cgame_mp_x86.dll 0x3044c1f4 (.data); refs=2 width=4; first=0x30022b3c; owner=cmd_callvote_f.
 * Example: 30022b3c   8b 15 f4 c1 44 30            MOV EDX,dword ptr [0x3044c1f4] | 3002b968   a3 f4 c1 44 30               MOV [0x3044c1f4],EAX
 */
const char *cg_soundFatigueBreath = NULL;
/* Source: uo_cgame_mp_x86.dll 0x3044c1f8 (.data); refs=2 width=4; first=0x30022b23; owner=cmd_callvote_f.
 * Example: 30022b23   a1 f8 c1 44 30               MOV EAX,[0x3044c1f8] | 3002b980   a3 f8 c1 44 30               MOV [0x3044c1f8],EAX
 */
const char *cg_soundSprintBreathLast = NULL;
/* Source: uo_cgame_mp_x86.dll 0x3044c1fc (.data); refs=2 width=4; first=0x30023298; owner=cmd_callvote_f.
 * Example: 30023298   a1 fc c1 44 30               MOV EAX,[0x3044c1fc] | 3002b995   a3 fc c1 44 30               MOV [0x3044c1fc],EAX
 */
const char *cg_soundUsGrenadeLever = NULL;
/* Source: uo_cgame_mp_x86.dll 0x3044c200 (.data); refs=2 width=4; first=0x3002324a; owner=cmd_callvote_f.
 * Example: 3002324a   a1 00 c2 44 30               MOV EAX,[0x3044c200] | 3002b7aa   a3 00 c2 44 30               MOV [0x3044c200],EAX
 */
const char *cg_soundSatchelBounce = NULL;
/* Source: uo_cgame_mp_x86.dll 0x3044c204 (.data); refs=2 width=4; first=0x300232d6; owner=cmd_callvote_f.
 * Example: 300232d6   a1 04 c2 44 30               MOV EAX,[0x3044c204] | 3002b9aa   a3 04 c2 44 30               MOV [0x3044c204],EAX
 */
const char *cg_soundMgOverheat = NULL;
/* Source: uo_cgame_mp_x86.dll 0x3044c208 (.data); refs=2 width=4; first=0x300232c1; owner=cmd_callvote_f.
 * Example: 300232c1   a1 08 c2 44 30               MOV EAX,[0x3044c208] | 3002b9bf   a3 08 c2 44 30               MOV [0x3044c208],EAX
 */
const char *cg_soundMgOverheatVehicle = NULL;
/* Source: uo_cgame_mp_x86.dll 0x3044c20c (.data); refs=2 width=4; first=0x3002ba13; owner=playercmd_finishplayerdamage.
 * Example: 3002ba13   a3 0c c2 44 30               MOV [0x3044c20c],EAX | 3003ada8   8b 0d 0c c2 44 30            MOV ECX,dword ptr [0x3044c20c]
 */
const char *cg_soundGameMessage = NULL;
/* Source: uo_cgame_mp_x86.dll 0x3044c210 (.data); refs=1 width=4; first=0x3002ba2b; owner=playercmd_finishplayerdamage.
 * Example: 3002ba2b   a3 10 c2 44 30               MOV [0x3044c210],EAX
 */
const char *cg_soundObjectiveComplete = NULL;
/* Source: uo_cgame_mp_x86.dll 0x3044c214 (.data); refs=1 width=4; first=0x3002b5be; owner=playercmd_finishplayerdamage.
 * Example: 3002b5be   a3 14 c2 44 30               MOV [0x3044c214],EAX
 */
const char *cg_soundMpAnnounceGTwoMinutes = NULL;
/* Source: uo_cgame_mp_x86.dll 0x3044c218 (.data); refs=1 width=4; first=0x3002b5d3; owner=playercmd_finishplayerdamage.
 * Example: 3002b5d3   a3 18 c2 44 30               MOV [0x3044c218],EAX
 */
const char *cg_soundMpAnnounceATwoMinutes = NULL;
/* Source: uo_cgame_mp_x86.dll 0x3044c21c (.data); refs=1 width=4; first=0x3002b5eb; owner=playercmd_finishplayerdamage.
 * Example: 3002b5eb   a3 1c c2 44 30               MOV [0x3044c21c],EAX
 */
const char *cg_soundMpAnnounceGThirtySeconds = NULL;
/* Source: uo_cgame_mp_x86.dll 0x3044c220 (.data); refs=1 width=4; first=0x3002b600; owner=playercmd_finishplayerdamage.
 * Example: 3002b600   a3 20 c2 44 30               MOV [0x3044c220],EAX
 */
const char *cg_soundMpAnnounceAThirtySeconds = NULL;
/* Source: uo_cgame_mp_x86.dll 0x3044c224 (.data); refs=2 width=4; first=0x30022cff; owner=cmd_callvote_f.
 * Example: 30022cff   8b 0d 24 c2 44 30            MOV ECX,dword ptr [0x3044c224] | 3002b696   a3 24 c2 44 30               MOV [0x3044c224],EAX
 */
const char *cg_soundPlayerWaterIn = NULL;
/* Source: uo_cgame_mp_x86.dll 0x3044c228 (.data); refs=2 width=4; first=0x30022d19; owner=cmd_callvote_f.
 * Example: 30022d19   a1 28 c2 44 30               MOV EAX,[0x3044c228] | 3002b6ab   a3 28 c2 44 30               MOV [0x3044c228],EAX
 */
const char *cg_soundPlayerWaterOut = NULL;
/* Source: uo_cgame_mp_x86.dll 0x3044c22c (.data); refs=1 width=4; first=0x3002b714; owner=playercmd_finishplayerdamage.
 * Example: 3002b714   a3 2c c2 44 30               MOV [0x3044c22c],EAX
 */
const char *cg_soundDebrisBounce = NULL;
/* Source: uo_cgame_mp_x86.dll 0x3044c230 (.data); refs=2 width=4; first=0x3001a150; owner=veh_playercollision.
 * Example: 3001a150   8b 14 bd 30 c2 44 30         MOV EDX,dword ptr [EDI*0x4 + 0x3044c230] | 3002b6c0   a3 30 c2 44 30               MOV [0x3044c230],EAX
 */
const char *cg_soundGrenadePulse[4];
/* Source: uo_cgame_mp_x86.dll 0x3044c240 (.data); refs=1 width=4; first=0x3002ba3b; owner=playercmd_finishplayerdamage.
 * Example: 3002ba3b   a3 40 c2 44 30               MOV [0x3044c240],EAX
 */
const char *cg_soundSpotlightSpark = NULL;
/* Source: uo_cgame_mp_x86.dll 0x3044c244 (.data); refs=3 width=4; first=0x30029546; owner=cg_updateflamethrowersounds.
 * Example: 30029546   a1 44 c2 44 30               MOV EAX,[0x3044c244] | 3002955a   a1 44 c2 44 30               MOV EAX,[0x3044c244] | 3002b741   a3 44 c2 44 30               MOV [0x3044c244],EAX
 * RESOLVED cg_flameFireSound: alias-name pointer stored from cgame_syscall(0xc3,
 * "flamethrower_fire") at 0x3002b741.
 */
const char *cg_flameFireSound = NULL;
/* Source: uo_cgame_mp_x86.dll 0x3044c24c (.data); refs=2 width=4; first=0x3002522c; owner=cg_addflamechunks.
 * cg_flameStartSound: flame start-sound qhandle (writer 0x3002b756; consumed by
 * CG_EmitPlayerFlameChunks 0x3002522c). RESOLVED (role) on consume. */
const char *cg_flameStartSound = NULL;
/* Source: uo_cgame_mp_x86.dll 0x3044c250 (.data); refs=2 width=4; first=0x300294d2; owner=cg_updateflamethrowersounds.
 * Example: 300294d2   8b 15 50 c2 44 30            MOV EDX,dword ptr [0x3044c250] | 3002b76b   a3 50 c2 44 30               MOV [0x3044c250],EAX
 * RESOLVED cg_flameStreamSound: alias-name pointer stored from cgame_syscall(0xc3,
 * "flamethrower_stream") at 0x3002b76b.
 */
const char *cg_flameStreamSound = NULL;
/* Source: uo_cgame_mp_x86.dll 0x3044c258 (.data); refs=3 width=4; first=0x30029645; owner=cg_updateflamethrowersounds.
 * Example: 30029645   8b 0d 58 c2 44 30            MOV ECX,dword ptr [0x3044c258] | 300296e8   8b 0d 58 c2 44 30            MOV ECX,dword ptr [0x3044c258] | 3002b795   a3 58 c2 44 30               MOV [0x3044c258],EAX
 * RESOLVED cg_flameCooldownSound: alias-name pointer stored from cgame_syscall(0xc3,
 * "flamethrower_cooldown") at 0x3002b795.
 */
const char *cg_flameCooldownSound = NULL;
/* Source: uo_cgame_mp_x86.dll 0x3044c25c (.data); refs=1 width=4; first=0x3002b780; owner=playercmd_finishplayerdamage.
 * Example: 3002b780   a3 5c c2 44 30               MOV [0x3044c25c],EAX
 */
const char *cg_soundPlayerBoneBounce = NULL;
/* Source: uo_cgame_mp_x86.dll 0x3044c260 (.data); refs=1 width=4; first=0x3002b729; owner=playercmd_finishplayerdamage.
 * Example: 3002b729   a3 60 c2 44 30               MOV [0x3044c260],EAX
 */
const char *cg_soundDebrisHitPlayer = NULL;
/* Source: uo_cgame_mp_x86.dll 0x3044c264 (.data); refs=2 width=4; first=0x300231e8; owner=cmd_callvote_f.
 * Example: 300231e8   8b 0d 64 c2 44 30            MOV ECX,dword ptr [0x3044c264] | 3002b63f   a3 64 c2 44 30               MOV [0x3044c264],EAX
 */
const char *cg_soundFlameBarrelBounce = NULL;
/* Source: uo_cgame_mp_x86.dll 0x3044c268 (.data); refs=1 width=4; first=0x3002bffe; owner=pm_updateviewangles.
 * Example: 3002bffe   a3 68 c2 44 30               MOV [0x3044c268],EAX
 */
qhandle_t cgs_media_checkboxClear = 0;
/* Source: uo_cgame_mp_x86.dll 0x3044c26c (.data); refs=1 width=4; first=0x3002c232; owner=pm_updateviewangles.
 * Example: 3002c232   a3 6c c2 44 30               MOV [0x3044c26c],EAX
 */
qhandle_t cgs_media_checkboxChecked = 0;
/* Source: uo_cgame_mp_x86.dll 0x3044c270 (.data); refs=1 width=4; first=0x3002c470; owner=pm_updateviewangles.
 * Example: 3002c470   a3 70 c2 44 30               MOV [0x3044c270],EAX
 * Resolved from the binary (fixes a mechanical name-address collision that had labeled this
 * cgs_media_backTileShader): at 0x3002c459 the media-init (CG_RegisterGraphics 0x3002ba50)
 * pushes the string "ui/assets/checkbox_fail" (0x30078080) with trap id 0x59 and stores the
 * trap return here (0x3002c470). So 0x3044c270 is the checkbox-fail UI material handle. The
 * real cgs_media_backTileShader is 0x3044b6f0 (registered from "gfx/2d/backtile" @0x30078070
 * via CG_RegisterShader, read by CG_TileClear). Binary-adjudicated. */
qhandle_t cgs_media_checkboxFail = 0;
/* Source: uo_cgame_mp_x86.dll 0x3044c274 (.data); one contiguous array spanning
 * 0x3044c274..0x3044cab4 (== 0x3044c274 + 22*24*4). RESOLVED as
 * cg_impactEffects[CG_IMPACT_EFFECT_TYPES][CG_IMPACT_SURFACE_TYPES], superseding the
 * mechanical export which split this single array into 22 `g_trypushingentity`
 * scalars plus three overlapping `[0x400]` tables (0x3044c6f4/754/7b4) and one
 * interior `cmd_callvote_f` symbol (0x3044c7b0 == &cg_impactEffects[13][23]). The
 * mislabeled `g_trypushingentity` owner is FUN_3001dfb0's rejected size-guess name;
 * its real name is CG_RegisterImpactEffects, which fills each of the 22 rows with 24
 * surface handles (row bases pushed at 0x3001e09b..0x3001e182, each 0x60 apart).
 * Read flat as [row][weapon + surface*24] by CG_EntityEvent (0x30022810) and by the
 * weapon renderer at 0x30048f21/0x30048f27. Zero-initialized. */
qhandle_t cg_impactEffects[CG_IMPACT_EFFECT_TYPES][CG_IMPACT_SURFACE_TYPES];
/* Source: uo_cgame_mp_x86.dll 0x3044cab4 (.data); refs=2 width=4; first=0x3002c836; owner=pm_updateviewangles.
 * Example: 3002c836   a3 b4 ca 44 30               MOV [0x3044cab4],EAX | 30048f3c   8b 3d b4 ca 44 30            MOV EDI,dword ptr [0x3044cab4]
 */
qhandle_t cgs_media_fleshImpactEffect = 0;
/* Source: uo_cgame_mp_x86.dll 0x3044cab8 (.data); refs=1 width=4; first=0x30027a7f; owner=bg_finditem.
 * Example: 30027a7f   a3 b8 ca 44 30               MOV [0x3044cab8],EAX
 * Retyped/renamed cg_flameSmokeEffect (effect handle for smoke_flamethrower.efx). */
qhandle_t cg_flameSmokeEffect = 0;
/* Source: uo_cgame_mp_x86.dll 0x3044cabc (.data); refs=1 width=4; first=0x30027a94; owner=bg_finditem.
 * Example: 30027a94   a3 bc ca 44 30               MOV [0x3044cabc],EAX
 * Retyped/renamed cg_flameSmokeEffectLarge (handle for smoke_flamethrower_lg.efx). */
qhandle_t cg_flameSmokeEffectLarge = 0;
/* Source: uo_cgame_mp_x86.dll 0x3044cac0 (.data); refs=2 width=4; first=0x30021251; owner=bg_animparseanimscript.
 * Example: 30021251   8b 04 9d c0 ca 44 30         MOV EAX,dword ptr [EBX*0x4 + 0x3044cac0] | 300212df   8b 14 9d c0 ca 44 30         MOV EDX,dword ptr [EBX*0x4 + 0x3044cac0]
 */
qhandle_t cgs_media_vehicleTreadEffects[VEH_TREAD_EFFECT_COUNT] = { 0 };
/* Source: uo_cgame_mp_x86.dll 0x3044cac4 (.data); refs=1 width=4; first=0x3002c860; owner=pm_updateviewangles.
 * Example: 3002c860   a3 c4 ca 44 30               MOV [0x3044cac4],EAX
 */
/* Source: uo_cgame_mp_x86.dll 0x3044cac8 (.data); refs=1 width=4; first=0x3002c84b; owner=pm_updateviewangles.
 * Example: 3002c84b   a3 c8 ca 44 30               MOV [0x3044cac8],EAX
 */
/* Source: uo_cgame_mp_x86.dll 0x3044cacc (.data); refs=1 width=4; first=0x3002c875; owner=pm_updateviewangles.
 * Example: 3002c875   a3 cc ca 44 30               MOV [0x3044cacc],EAX
 */
/* Source: uo_cgame_mp_x86.dll 0x3044cad0 (.data); refs=1 width=4; first=0x3002c88a; owner=pm_updateviewangles.
 * Example: 3002c88a   a3 d0 ca 44 30               MOV [0x3044cad0],EAX
 */
/* Source: uo_cgame_mp_x86.dll 0x3044cad4 (.data); refs=1 width=4; first=0x3002c89f; owner=pm_updateviewangles.
 * Example: 3002c89f   a3 d4 ca 44 30               MOV [0x3044cad4],EAX
 */
/* Source: uo_cgame_mp_x86.dll 0x3044cad8 (.data); refs=1 width=4; first=0x3002c8b4; owner=pm_updateviewangles.
 * Example: 3002c8b4   a3 d8 ca 44 30               MOV [0x3044cad8],EAX
 */
/* Source: uo_cgame_mp_x86.dll 0x3044cadc (.data); refs=1 width=4; first=0x3002c8e1; owner=pm_updateviewangles.
 * Example: 3002c8e1   a3 dc ca 44 30               MOV [0x3044cadc],EAX
 */
/* Source: uo_cgame_mp_x86.dll 0x3044cae0 (.data); refs=1 width=4; first=0x3002c8cc; owner=pm_updateviewangles.
 * Example: 3002c8cc   a3 e0 ca 44 30               MOV [0x3044cae0],EAX
 */
/* Source: uo_cgame_mp_x86.dll 0x3044cae4 (.data); refs=1 width=4; first=0x3002c8f6; owner=pm_updateviewangles.
 * Example: 3002c8f6   a3 e4 ca 44 30               MOV [0x3044cae4],EAX
 */
/* Source: uo_cgame_mp_x86.dll 0x3044cae8 (.data); refs=1 width=4; first=0x3002c90b; owner=pm_updateviewangles.
 * Example: 3002c90b   a3 e8 ca 44 30               MOV [0x3044cae8],EAX
 */
/* Source: uo_cgame_mp_x86.dll 0x3044caec (.data); refs=1 width=4; first=0x3002c920; owner=pm_updateviewangles.
 * Example: 3002c920   a3 ec ca 44 30               MOV [0x3044caec],EAX
 */
/* Source: uo_cgame_mp_x86.dll 0x3044caf0 (.data); refs=1 width=4; first=0x3002c93a; owner=pm_updateviewangles.
 * Example: 3002c93a   a3 f0 ca 44 30               MOV [0x3044caf0],EAX
 */
/* Source: uo_cgame_mp_x86.dll 0x3044caf4 (.data); refs=5 width=4; first=0x3001b8ac; owner=cg_drawtracer.
 * Example: 3001b8ac   a1 f4 ca 44 30               MOV EAX,[0x3044caf4] | 3001b949   a1 f4 ca 44 30               MOV EAX,[0x3044caf4] | 3001b96f   8b 0d f4 ca 44 30            MOV ECX,dword ptr [0x3044caf4]
 */
int32_t cg_complaintClientNum = 0;
/* Source: uo_cgame_mp_x86.dll 0x3044caf8 (.data); refs=5 width=4; first=0x3001b7e8; owner=cg_drawtracer.
 * Example: 3001b7e8   a1 f8 ca 44 30               MOV EAX,[0x3044caf8] | 3001b8a0   39 0d f8 ca 44 30            CMP dword ptr [0x3044caf8],ECX | 30039553   89 35 f8 ca 44 30            MOV dword ptr [0x3044caf8],ESI
 */
int32_t cg_complaintEndTime = 0;
/* Source: uo_cgame_mp_x86.dll 0x3044cafc (.data): random startup value in [-1,1). */
float cg_initialRandomValue = 0.0f;
/* Source: uo_cgame_mp_x86.dll 0x3044cb00 (.data); base of a 0x4d0-stride
 * clientInfo_t table indexed by (corpse entity number - 0x40). All three
 * mechanical refs use this base identically after `SUB idx,0x40; IMUL idx,idx,0x4d0`:
 * 0x30021e0a (writes elem+0x3e0/+0x3e8), 0x30034708 (CG_AddPlayerCorpseEntity),
 * and 0x3003c8e6 — proving a per-corpse client animation/info array. Superseded
 * from the mechanical g_data_bg_calculateweaponangles_* scalar (the owner label
 * was a wrong first-touch artifact; this is not a weapon-angles datum).
 * Eight retail records are proven by the +0x4c4 field walk and the next
 * independent datum after the array. Coordinated mod builds may configure a
 * larger shared PLAYER_CLONE_COUNT for both game and cgame. */
clientInfo_t cg_corpseInfo[PLAYER_CLONE_COUNT];
/* Source: uo_cgame_mp_x86.dll 0x3044f18c (.data); the renderer-stats HUD detail
 * level read by CG_DrawFPS (>1 gates the detailed lines) and by an adjacent
 * unreconstructed reader at 0x3001874f. owner=cg_drawweapreticle was a rejected
 * size-guess name; the prior cg_localAnimTrees[7]+0x18 note was wrong (this address
 * is 0x21c8 past that array's base). Exact cvar name unresolved; named by role.
 * Example: 3001831f MOV EAX,[0x3044f18c] | 3001874f MOV EAX,[0x3044f18c]
 */
/* 0x3044f2ac: cg_thirdPerson.integer; cvar-table entry 0x30085710. */

/* Source: uo_cgame_mp_x86.dll 0x3044f3cc (.data); refs=1 width=4; first=0x30047be0.
 * Brass-ejection master enable flag; the sole reader CG_EjectWeaponBrass (0x30047be0)
 * gates all shell-casing effect play on it being nonzero.
 * Example: 30047be0   a1 cc f3 44 30               MOV EAX,[0x3044f3cc]
 */

/* Source: uo_cgame_mp_x86.dll 0x3044f60c (.data); refs=6 width=4; first=0x30030f1c.
 * RESOLVED: cg_currentSelectedPlayer_vmCvar.integer — HUD trap-54 emit iteration cursor (see globals.h).
 * Example: 30030f1c   a1 0c f6 44 30               MOV EAX,[0x3044f60c] | 30030f37   a3 0c f6 44 30               MOV [0x3044f60c],EAX | 30031020   a1 0c f6 44 30               MOV EAX,[0x3044f60c]
 */

/* Source: uo_cgame_mp_x86.dll 0x3044f644 (.data); refs=1 width=imm; first=0x30005c8b; owner=script_method_hudelem_destroy.
 * Example: 30005c8b   81 fe 44 f6 44 30            CMP ESI,0x3044f644
 */

/* Source: uo_cgame_mp_x86.dll 0x3044f96c (.data); refs=3 width=4; first=0x30022b7e; owner=cmd_callvote_f.
 * Example: 30022b7e   a1 6c f9 44 30               MOV EAX,[0x3044f96c] | 30022bb9   a1 6c f9 44 30               MOV EAX,[0x3044f96c] | 30022bf5   a1 6c f9 44 30               MOV EAX,[0x3044f96c]
 */

/* Source: uo_cgame_mp_x86.dll 0x3044fa8c (.data); refs=3 width=4; first=0x30019ba3.
 * cg_drawCrosshair_vmCvar.integer: externally-set (cvar-integer) HUD/3D-view draw-enable
 * gate read by CG_DrawWeaponIcon3D (0x30019ba3) and two sibling view draws; see
 * globals.h. Mechanical owner=pm_viewheighttablelerp was a first-touch mislabel. */

/* Source: uo_cgame_mp_x86.dll 0x3044fba8 (.data); refs=1 width=4; first=0x3003020f; owner=clientendframe.
 * Example: 3003020f   d8 1d a8 fb 44 30            FCOMP float ptr [0x3044fba8]
 */

/* Source: uo_cgame_mp_x86.dll 0x3044fcc8 (.data); refs=8 width=4; first=0x30016905; owner=cg_asset_parse.
 * Example: 30016905   d8 25 c8 fc 44 30            FSUB float ptr [0x3044fcc8] | 30016911   d8 05 c8 fc 44 30            FADD float ptr [0x3044fcc8] | 300169f7   d9 05 c8 fc 44 30            FLD float ptr [0x3044fcc8]
 */

/* Source: uo_cgame_mp_x86.dll 0x3044fdec (.data); refs=3 width=4; first=0x30032c2c; owner=g_isvehicleusable.
 * Example: 30032c2c   a1 ec fd 44 30               MOV EAX,[0x3044fdec] | 30032cc2   a1 ec fd 44 30               MOV EAX,[0x3044fdec] | 30032db2   a1 ec fd 44 30               MOV EAX,[0x3044fdec]
 */

/* Source: uo_cgame_mp_x86.dll 0x3044ff08 (.data); refs=5 width=4; first=0x300046f8; owner=bg_getminspreadforweapon.
 * bg_swingSpeed_vmCvar.value — the legs/torso swing stepScale used by BG_PlayerAngles
 * (0x30004550); read-only in this binary's reconstructed code, .data initializer 0.
 * Type superseded to float (see globals.h). The exporter's wrong owner label is dropped.
 * Example: 300046f8   8b 35 08 ff 44 30            MOV ESI,dword ptr [0x3044ff08] | 30004724   8b 15 08 ff 44 30            MOV EDX,dword ptr [0x3044ff08] | 3000477e   8b 15 08 ff 44 30            MOV EDX,dword ptr [0x3044ff08]
 */

/* Source: uo_cgame_mp_x86.dll 0x30450020 (.data). cg_cvarTable[177] identifies
 * this vmCvar_t as cl_serverloadmap; consumers read .string at +0x10. */
vmCvar_t cl_serverloadmap;
/* Source: uo_cgame_mp_x86.dll 0x30450148 (.data); refs=1 width=4; first=0x30025419; owner=cg_addflamechunks.
 * timescale_vmCvar.value: float divisor consumed by CG_EmitPlayerFlameChunks
 * (0x30025419). RESOLVED (role) on consume; retyped uint32_t -> float. */

/* Source: uo_cgame_mp_x86.dll 0x3045014c (.data); refs=2 width=4; first=0x3002de90; owner=g_dobjcalcpose.
 * Example: 3002de90   83 3d 4c 01 45 30 01         CMP dword ptr [0x3045014c],0x1 | 30037d9a   a1 4c 01 45 30               MOV EAX,[0x3045014c]
 */

/* Source: uo_cgame_mp_x86.dll 0x30450268 (.data); refs=4 width=4; first=0x30045fcb; owner=item_listbox_paint.
 * Example: 30045fcb   d8 0d 68 02 45 30            FMUL float ptr [0x30450268] | 30046032   d8 0d 68 02 45 30            FMUL float ptr [0x30450268] | 3004607e   d8 0d 68 02 45 30            FMUL float ptr [0x30450268]
 */

/* Source: uo_cgame_mp_x86.dll 0x30450388 (.data); refs=2 width=4; first=0x3001aa35; owner=cg_ejectweaponbrass.
 * Example: 3001aa35   d9 05 88 03 45 30            FLD float ptr [0x30450388] | 3001aa49   d8 05 88 03 45 30            FADD float ptr [0x30450388]
 */
/* 0x30450388; damage-direction arrow icon top Y. See globals.h. */
/* Source: uo_cgame_mp_x86.dll 0x304504a0 (.data); refs=1 width=imm; first=0x30035a6e; owner=pmovesingle.
 * Example: 30035a6e   68 a0 04 45 30               PUSH 0x304504a0
 */
vmCvar_t pmove_msec = {0};
/* Source: uo_cgame_mp_x86.dll 0x304504ac (.data); refs=3 width=4; first=0x30035a4d; owner=pmovesingle.
 * Example: 30035a4d   a1 ac 04 45 30               MOV EAX,[0x304504ac] | 30035a83   8b 0d ac 04 45 30            MOV ECX,dword ptr [0x304504ac] | 30035d38   8b 0d ac 04 45 30            MOV ECX,dword ptr [0x304504ac]
 */
/* Source: uo_cgame_mp_x86.dll 0x304505cc (.data); refs=1 width=4; first=0x3003a100; owner=item_enableshowviacvar.
 * Example: 3003a100   a1 cc 05 45 30               MOV EAX,[0x304505cc]
 */
/* cg_noVoiceText_vmCvar.integer: cached .integer of the cg_noVoiceText cvar. When nonzero,
 * CG_PlayVoiceChat (0x30039ff0) suppresses printing/team-chatting the chat text.
 * (owner label above is the mechanical first-touch and is incorrect). See globals.h. */
/* Source: uo_cgame_mp_x86.dll 0x304505cc. */

/* Source: uo_cgame_mp_x86.dll 0x304506e8 (.data); refs=3 width=4; first=0x3002ab11; owner=pm_switchifempty.
 * Example: 3002ab11   d9 05 e8 06 45 30            FLD float ptr [0x304506e8] | 300482e6   a1 e8 06 45 30               MOV EAX,[0x304506e8] | 30048a2c   d9 05 e8 06 45 30            FLD float ptr [0x304506e8]
 */
/* Repaired uint32_t->float: read as `float ptr` (moving-tracer length, mode A).
 * Zero-initialized in .bss (set at init by an unseen writer). */

/* Source: uo_cgame_mp_x86.dll 0x30450928 (.data); refs=1 width=4; first=0x3001aa43; owner=cg_ejectweaponbrass.
 * Example: 3001aa43   d9 05 28 09 45 30            FLD float ptr [0x30450928]
 */
/* 0x30450928; damage-direction arrow icon height. See globals.h. */
/* Source: uo_cgame_mp_x86.dll 0x30450a4c (.data); refs=3 width=4; first=0x3001c04c; owner=fire_lead.
 * Example: 3001c04c   a1 4c 0a 45 30               MOV EAX,[0x30450a4c] | 3001c066   a1 4c 0a 45 30               MOV EAX,[0x30450a4c] | 3001c079   a1 4c 0a 45 30               MOV EAX,[0x30450a4c]
 */

/* Source: uo_cgame_mp_x86.dll 0x30450b6c (.data); refs=1 width=4; first=0x30018731.
 * cg_drawSnapshot_vmCvar.integer — enable flag for the snapshot-timing debug HUD line; read by
 * CG_DrawDebugOverlays (0x30018730) to gate CG_DrawSnapshot. The owner=
 * pm_weapon_finishrechamber label was a size-collision mislabel and is corrected.
 * Example: 30018731   a1 6c 0b 45 30               MOV EAX,[0x30450b6c]
 */

/* Source: uo_cgame_mp_x86.dll 0x30450dac (.data); refs=3 width=4; first=0x3003a048; owner=item_enableshowviacvar.
 * Example: 3003a048   8b 15 ac 0d 45 30            MOV EDX,dword ptr [0x30450dac] | 3003a060   8b 0d ac 0d 45 30            MOV ECX,dword ptr [0x30450dac] | 3003a0c7   8b 0d ac 0d 45 30            MOV ECX,dword ptr [0x30450dac]
 */
/* cg_voiceSpriteTime_vmCvar.integer: runtime dword read only by CG_PlayVoiceChat
 * (0x30039ff0) as the voice-chat icon display duration added to cg.time (added once
 * for the default icon, doubled otherwise). Uninitialized in the image and not
 * written by .text in this DLL, so it is engine/externally initialized. Exact CoD
 * source symbol unresolved -> address suffix retained. (owner label above is the
 * mechanical first-touch and is incorrect.) See globals.h. */

/* Source: uo_cgame_mp_x86.dll 0x30450fec (.data); refs=1 width=4; first=0x3001d6d0; owner=veh_checkpushclients.
 * Example: 3001d6d0   a1 ec 0f 45 30               MOV EAX,[0x30450fec]
 */

/* Source: uo_cgame_mp_x86.dll 0x30451108 (.data); refs=1 width=4; first=0x30044ce6; owner=setclientviewangle.
 * Example: 30044ce6   d9 05 08 11 45 30            FLD float ptr [0x30451108]
 */

/* Source: uo_cgame_mp_x86.dll 0x30451348 (.data); refs=1 width=4; first=0x30044ed1; owner=setclientviewangle.
 * Example: 30044ed1   d9 05 48 13 45 30            FLD float ptr [0x30451348]
 */

/* Source: uo_cgame_mp_x86.dll 0x3045146c (.data); refs=3 width=4; first=0x300430a1.
 * cg_railTrailTime_vmCvar.integer — rail-trail segment lifetime in ms. Consumed only by the
 * rail-trail segment builder (0x300430a0): gates drawing (skip when <= 0), sets
 * le->endTime = cg.time + this and le->lifeRate = 1.0f / this. Read as signed int;
 * no writer in this DLL (engine/loader/cvar-provided). Supersedes owner=pmove.
 */

/* Source: uo_cgame_mp_x86.dll 0x304516ac (.data); refs=1 width=4; first=0x3001b0f0; owner=sp_script_origin.
 * Example: 3001b0f0   a1 ac 16 45 30               MOV EAX,[0x304516ac]
 * Resolved on consume by CG_DrawFixedFadeElement (0x3001b0f0): boolean enable gate
 * for a fixed-position debug/HUD draw; never written in this DLL. See globals.h. */

/* Source: uo_cgame_mp_x86.dll 0x304517c8 (.data); refs=8 width=4; first=0x300169a3; owner=cg_asset_parse.
 * Example: 300169a3   d8 1d c8 17 45 30            FCOMP float ptr [0x304517c8] | 300169b4   8b 15 c8 17 45 30            MOV EDX,dword ptr [0x304517c8] | 300169e3   d9 05 c8 17 45 30            FLD float ptr [0x304517c8]
 */

/* Source: uo_cgame_mp_x86.dll 0x304518ec (.data); refs=1 width=4; first=0x3002efed; owner=bullet_fire_extended.
 * cg_hudStanceHintPrints_vmCvar.integer — read-only gate in CG_DrawPlayerStance: when nonzero the
 * change-detection path compares cg_stanceHintChangeTime against cg.time; when zero
 * it forces cg_stanceHintChangeTime = -1. No writer appears in the exported code
 * (set by an unrecovered snapshot path); role name, exact source name unresolved.
 * Old owner label "bullet_fire_extended" was a wrong size-guess. */

/* Source: uo_cgame_mp_x86.dll 0x30451b2c (.data). cl_languagewarningsaserrors_vmCvar.integer:
 * when nonzero, a failed string translation is a fatal BG_AnimParseError(7,...) instead of
 * a Com_Printf warning. Read-only in the four cgame translate helpers (first=
 * 0x3002d707); cvar-integer snapshot; provisional role name (exact cvar name
 * unresolved). */

/* Source: uo_cgame_mp_x86.dll 0x30451c48 (.data); refs=1 width=4; first=0x3001aa07; owner=cg_ejectweaponbrass.
 * Example: 3001aa07   d9 05 48 1c 45 30            FLD float ptr [0x30451c48]
 */
/* 0x30451c48; damage-direction arrow icon width. See globals.h. */
/* Source: uo_cgame_mp_x86.dll 0x30451e8c (.data); refs=2 width=4; first=0x3001c401.
 * cg_skybox_vmCvar.integer — boolean gating the cg_refdef.rdflags bit 0x10 (draw-world vs
 * no-world-model) before trap_R_RenderScene in CG_DrawActive (0x3001c3fb); also read
 * by the refdef builder at 0x30041e53. Role name; owner=g_getnonpvsfriendlyinfo was
 * a wrong size-match. */

/* Source: uo_cgame_mp_x86.dll 0x304520cc (.data); refs=1 width=4; first=0x3001c008; owner=fire_lead.
 * Example: 3001c008   a1 cc 20 45 30               MOV EAX,[0x304520cc]
 */

/* Source: uo_cgame_mp_x86.dll 0x304521e8 (.data); refs=2 width=4; first=0x30045f1c; owner=item_listbox_paint.
 * Example: 30045f1c   d9 05 e8 21 45 30            FLD float ptr [0x304521e8] | 300468f7   d9 05 e8 21 45 30            FLD float ptr [0x304521e8]
 */

/* Source: uo_cgame_mp_x86.dll 0x30452300 (.data), string field +0x10 at
 * 0x30452310. cg_cvarTable[122] identifies the object as cg_objectiveText. */
vmCvar_t cg_objectiveText;
/* Source: uo_cgame_mp_x86.dll 0x30452428 (.data); refs=4 width=4; first=0x30019c21.
 * cg_crosshairAlpha_vmCvar.value: externally-set view/HUD fade or blend fraction,
 * gated < 0.01f by CG_DrawWeaponIcon3D and scaled by sibling view draws; see
 * globals.h. Mechanical owner=pm_viewheighttablelerp was a first-touch mislabel. */

/* Source: uo_cgame_mp_x86.dll 0x30452548 (.data); refs=10 width=4; first=0x30016a2c; owner=cg_asset_parse.
 * Example: 30016a2c   d8 1d 48 25 45 30            FCOMP float ptr [0x30452548] | 30016a39   a1 48 25 45 30               MOV EAX,[0x30452548] | 30016a4a   d8 25 48 25 45 30            FSUB float ptr [0x30452548]
 */

/* Source: uo_cgame_mp_x86.dll 0x3045266c (.data); refs=1 width=4; first=0x300349c7; owner=cg_interpolateentityposition (mislabeled; superseded).
 * cg_debugposition_vmCvar.integer — developer toggle read by CG_ResetPlayerEntity (0x30034880).
 * Example: 300349c7   39 1d 6c 26 45 30            CMP dword ptr [0x3045266c],EBX
 */

/* Source: uo_cgame_mp_x86.dll 0x3045278c (.data): cg_drawGun_vmCvar.integer —
 * field +0xc of the vmCvar_t at 0x30452780 (cg_cvarTable[1] "cg_drawGun"), not a
 * standalone global. CG_DrawCrosshair reads it twice (0x30019ed2: at full ADS the
 * crosshair is suppressed when the gun model is drawn; 0x30019fcf: the ADS
 * transition overlay reticle is drawn only when it is hidden); third reader at
 * 0x300465d5 (view-weapon add path). The owner=veh_playercollision label was the
 * consumer's size-guess mislabel.
 * Example: 30019ed2   a1 8c 27 45 30               MOV EAX,[0x3045278c]
 */

/* Source: uo_cgame_mp_x86.dll 0x30452aec (.data). cl_languagewarnings_vmCvar.integer:
 * when zero, a failed string translation is silently returned as a plain copy of
 * the input; when nonzero, the translate helpers emit a "Could not translate ..."
 * warning/error and return an "^1UNLOCALIZED(^7...^1)^7" decorated placeholder.
 * Read-only in the four cgame translate helpers (first=0x3002d6fa); cvar-integer
 * snapshot; provisional role name (exact cvar name unresolved). */

/* Source: uo_cgame_mp_x86.dll 0x30452c08 (.data); refs=1 width=4; first=0x300191d4.
 * cg_centertime_vmCvar.value — center-print fade duration in seconds, read as a
 * float by CG_DrawCenterString (0x300191b0): FLD [0x30452c08]; FMUL 1000.0; Q_rint
 * -> ms passed to CG_FadeColor as totalMsec. Mechanical export mislabeled the type
 * (uint32_t) and owner (size-guess); corrected to float here. Example:
 * 300191d4   d9 05 08 2c 45 30            FLD float ptr [0x30452c08]. */

/* Source: uo_cgame_mp_x86.dll 0x30452d2c (.data); refs=2 width=4; owner=reached_binarymover.
 * cg_chatHeight_vmCvar.integer — cvar-integer snapshot of visible team-chat lines.
 * CG_AddToTeamChat (0x30039390) clamps to [1..TEAMCHAT_HEIGHT] and uses it as the
 * ring modulus; <=0 flushes the ring. Signed compares (JL/JLE) in the machine
 * code prove int, not uint32_t. */

/* Source: uo_cgame_mp_x86.dll 0x30452e40 (.data). The cg_cvarTable entry at
 * 0x300855f0 binds this vmCvar_t to "cg_debugProneCheckDepthCheck"; the direct
 * target reads are of .integer at 0x30452e4c. */
vmCvar_t cg_debugProneCheckDepthCheck = {0};
/* Source: uo_cgame_mp_x86.dll 0x30452f6c (.data); refs=1 width=4; first=0x3001b070.
 * Example: 3001b070   a1 6c 2f 45 30               MOV EAX,[0x30452f6c]
 * Resolved on consume by CG_DrawDebugFadeElement (0x3001b070): a signed-int
 * developer/debug-draw gate. The consumer loads it, rejects the draw when it is
 * negative (JL), and — when the developer cvar is off — also rejects it when it
 * is zero (draw only when >0 or in developer mode). Never written anywhere in
 * this DLL, so its value is engine/loader-provided (same pattern as
 * cg_hudAlpha_vmCvar.value). Exact source cvar name unresolved (refs=1, no writer, no
 * string); named by proven role, address suffix kept. The mechanical
 * owner=colorbytes3 first-touch label is rejected. Retyped uint32_t -> int32_t:
 * the sole access is a signed compare. */

/* Source: uo_cgame_mp_x86.dll 0x304531a0 (.data); refs=8 width=4; first=0x3001e6cc.
 * cg_items[] — cgame per-item registered-visuals cache (itemInfo_t, stride 0x24).
 * owner=sp_func_bobbing is a mechanical first-touch/size-guess artifact (rejected;
 * the first-touching function 0x3001e680 is CG_Item, not SP_func_bobbing).
 * Example: 3001e6cc   8b 04 b5 a0 31 45 30         MOV EAX,dword ptr [ESI*0x4 + 0x304531a0] | 3001e6d6   8d 34 b5 a0 31 45 30         LEA ESI,[ESI*0x4 + 0x304531a0] | 30022d56   8d 04 85 a0 31 45 30         LEA EAX,[EAX*0x4 + 0x304531a0]
 */
/* 0x304531a0: storage sized 256 (0x100): CG_RegisterGraphics clears 0x2400 bytes
 * == 256 * sizeof(itemInfo_t). Only indices 1..133 (bg_numItems 0x86) are populated. */
itemInfo_t cg_items[256];
/* Source: uo_cgame_mp_x86.dll 0x304555a8 (.data); refs=1 width=4; first=0x30044f14; owner=setclientviewangle.
 * Example: 30044f14   d9 05 a8 55 45 30            FLD float ptr [0x304555a8]
 */

/* Source: uo_cgame_mp_x86.dll 0x304556c8 (.data); refs=4 width=4; first=0x300483f9; owner=menus_open.
 * Example: 300483f9   d8 35 c8 56 45 30            FDIV float ptr [0x304556c8] | 30048422   d9 05 c8 56 45 30            FLD float ptr [0x304556c8] | 3004842f   d9 05 c8 56 45 30            FLD float ptr [0x304556c8]
 */

/* Source: uo_cgame_mp_x86.dll 0x30455908 (.data); refs=2 width=4; first=0x300455f3.
 * cg_tracerchancelmg_vmCvar.value — the per-shot tracer spawn probability [0..1] used for the
 * "mode A" ammo types (ammoType 3/4/5: LMG/HMG/UMG). CG_SpawnTracer
 * (0x30048d60) overrides its default chance (cg_tracerchance_vmCvar.value, 0x30456208) with this
 * value when bg_weaponInfos[weapon]->ammoType is 3, 4, or 5, then draws the tracer
 * only when rand()/32768 < chance. Read (never written in this DLL; runtime/cvar-like
 * init outside .text) as a float. Type repaired uint32_t->float; the mechanical
 * owner=cg_drawplayerstance label was only the first toucher (0x30045550 reads the
 * same twin pair). Exact CoD source name unproven; role name from CG_SpawnTracer.
 * Example: 30048dee   8b 0d 08 59 45 30   MOV ECX,dword ptr [0x30455908] */

/* Source: uo_cgame_mp_x86.dll 0x30455fc8 (.data): cg_crosshairAlphaMin_vmCvar.value —
 * field +0x8 of the vmCvar_t at 0x30455fc0 (cg_cvarTable[43] "cg_crosshairAlphaMin"),
 * not a standalone global. CG_DrawCrosshair (0x3001a239/0x3001a246) clamps the
 * side-reticle alpha up to it. The owner=veh_playercollision label was the
 * consumer's size-guess mislabel.
 * Example: 3001a239   d8 1d c8 5f 45 30            FCOMP float ptr [0x30455fc8]
 */

/* Source: uo_cgame_mp_x86.dll 0x30456208 (.data); refs=3 width=4; first=0x30045556.
 * cg_tracerchance_vmCvar.value — the default per-shot tracer spawn probability [0..1]. CG_SpawnTracer
 * (0x30048d60) first gates the whole routine on cg_tracerchance_vmCvar.value > 0.0 (no tracers when
 * <= 0), uses it as the default spawn chance for non-{3,4,5} weapon classes, then draws
 * the tracer only when rand()/32768 < chance (FLD chance; FMUL 32768.0; FCOMPP vs
 * (float)rand()). Read (never written in this DLL; runtime/cvar-like init outside .text)
 * as a float. Type repaired uint32_t->float; the mechanical owner=cg_drawplayerstance
 * label was only the first toucher (0x30045550 reads the same twin pair). Exact CoD
 * source name unproven; role name from CG_SpawnTracer.
 * Example: 30048d60   d9 05 08 62 45 30   FLD float ptr [0x30456208] */
/* 0x3045632c: cg_norender_vmCvar.integer.integer; cvar-table entry 0x30085880. */

/* Source: uo_cgame_mp_x86.dll 0x30456448 (.data); refs=1 width=4; first=0x300301f3; owner=clientendframe.
 * Example: 300301f3   d8 1d 48 64 45 30            FCOMP float ptr [0x30456448]
 */

/* Source: uo_cgame_mp_x86.dll 0x30456568 (.data); refs=6 width=4; first=0x30016a70; owner=cg_asset_parse.
 * Example: 30016a70   d8 25 68 65 45 30            FSUB float ptr [0x30456568] | 30016a78   d8 05 68 65 45 30            FADD float ptr [0x30456568] | 30017093   d8 25 68 65 45 30            FSUB float ptr [0x30456568]
 */

/* Source: uo_cgame_mp_x86.dll 0x304567ac (.data); the cg_letterbox_vmCvar.integer gate.
 * Read (only) by CG_CalcVrect (3003f576 MOV EAX,[0x304567ac]) and TESTed: when
 * nonzero the vertical view size is scaled to 85%. Proven-by-role name; exact
 * source identity unresolved (writer not in the reconstructed set). Zero in the
 * image. See globals.h for rationale. */

/* Source: uo_cgame_mp_x86.dll 0x304568cc (.data); refs=2 width=4; first=0x3002b350; owner=trap_syscall_2.
 * Example: 3002b350   a1 cc 68 45 30               MOV EAX,[0x304568cc] | 3003cd45   8b 0d cc 68 45 30            MOV ECX,dword ptr [0x304568cc]
 */

/* Source: uo_cgame_mp_x86.dll 0x304569e8 (.data); refs=1 width=4; first=0x3003f7a9; owner=scr_vehicle_damagescale.
 * Example: 3003f7a9   d9 05 e8 69 45 30            FLD float ptr [0x304569e8]
 */

/* Source: uo_cgame_mp_x86.dll 0x30456b0c (.data); refs=2 width=4.
 * cvar integer enable-flag for the overlay-A developer info screen. Read at
 * 0x3001b360 (CG_DrawInfoScreens dispatcher) and 0x3001acd1 (inside its
 * overlay-A target 0x3001acc0). Never written in .text; the cvar system owns the
 * store. Renamed from the wrong mechanical owner "info_setvalueforkey". */

/* Source: uo_cgame_mp_x86.dll 0x30456d48 (.data); refs=1 width=4; first=0x30044ec1; owner=setclientviewangle.
 * Example: 30044ec1   d9 05 48 6d 45 30            FLD float ptr [0x30456d48]
 */

/* Source: uo_cgame_mp_x86.dll 0x30456f8c (.data); refs=4 width=4; first=0x3002281f; owner=cmd_callvote_f.
 * cg_debugevents_vmCvar.integer -- cached integer of the "cg_debugevents" cvar; boolean gate for the
 * entity-event debug traces (CG_EntityEvent / CG_EntityPreEvent). See globals.h.
 * Example: 3002281f   a1 8c 6f 45 30               MOV EAX,[0x30456f8c] | 3002284b   a1 8c 6f 45 30               MOV EAX,[0x30456f8c] | 30022868   a1 8c 6f 45 30               MOV EAX,[0x30456f8c]
 */

/* Source: uo_cgame_mp_x86.dll 0x304570a8 (.data); refs=2 width=4; first=0x3001c35f.
 * cg_stereoSeparation_vmCvar.value — stereo eye-separation distance (float); CG_DrawActive FLDs it
 * and scales by ±0.5f per eye to offset the view origin before trap_R_RenderScene.
 * Retyped float; owner=g_getnonpvsfriendlyinfo was a wrong size-match. */

/* Source: uo_cgame_mp_x86.dll 0x304571cc (.data); refs=1 width=4.
 * cvar integer enable-flag for the overlay-B script-VM debug screen
 * (0x30017e90). Read only at 0x3001b36e (CG_DrawInfoScreens dispatcher). Never
 * written in .text. Renamed from the wrong mechanical owner "cmd_fogswitch_f". */

/* Source: uo_cgame_mp_x86.dll 0x3045764c (.data): cg_crosshairDynamic_vmCvar.integer —
 * field +0xc of the vmCvar_t at 0x30457640 (cg_cvarTable[44] "cg_crosshairDynamic"),
 * not a standalone global. CG_DrawCrosshair (0x3001a0c0) keeps the projected
 * impact-point crosshair shift only when it is set. The owner=veh_playercollision
 * label was the consumer's size-guess mislabel.
 * Example: 3001a0c0   a1 4c 76 45 30               MOV EAX,[0x3045764c]
 */

/* Source: uo_cgame_mp_x86.dll 0x3045776c (.data); refs=1 width=4; first=0x3001a9bf; owner=cg_ejectweaponbrass.
 * Example: 3001a9bf   a1 6c 77 45 30               MOV EAX,[0x3045776c]
 */
/* 0x3045776c; ADS-anchor gate for damage arrows. See globals.h. */
/* Source: uo_cgame_mp_x86.dll 0x3045788c (.data); refs=1 width=4; first=0x3001a620.
 * cg_drawCrosshairNames_vmCvar.integer — externally-set (cvar-integer) enable gate for the
 * crosshair player-name HUD; read by CG_DrawCrosshairNames (0x3001a620).
 */

/* Source: uo_cgame_mp_x86.dll 0x304579a0 (.data). The cg_cvarTable entry at
 * 0x300855e0 binds this vmCvar_t to "cg_debugProneCheck"; the direct target
 * reads are of .integer at 0x304579ac. */
vmCvar_t cg_debugProneCheck = {0};
/* Source: uo_cgame_mp_x86.dll 0x30457be8 (.data); refs=1 width=4; first=0x30044e1f; owner=setclientviewangle.
 * Example: 30044e1f   d9 05 e8 7b 45 30            FLD float ptr [0x30457be8]
 */

/* Source: uo_cgame_mp_x86.dll 0x30457d0c (.data); refs=4 width=4; first=0x300228a6; owner=cmd_callvote_f.
 * Example: 300228a6   a1 0c 7d 45 30               MOV EAX,[0x30457d0c] | 300228f2   a1 0c 7d 45 30               MOV EAX,[0x30457d0c] | 3002293e   a1 0c 7d 45 30               MOV EAX,[0x30457d0c]
 */

/* Source: uo_cgame_mp_x86.dll 0x30457f48 (.data); refs=1 width=4; first=0x30044e11; owner=setclientviewangle.
 * Example: 30044e11   d9 05 48 7f 45 30            FLD float ptr [0x30457f48]
 */

/* Source: uo_cgame_mp_x86.dll 0x3045818c (.data); refs=1 width=4; first=0x30039ff3; owner=item_enableshowviacvar.
 * Example: 30039ff3   a1 8c 81 45 30               MOV EAX,[0x3045818c]
 */
/* cg_noVoiceChats_vmCvar.integer: cached .integer of the cg_noVoiceChats cvar. When nonzero,
 * CG_PlayVoiceChat (0x30039ff0) skips playing the voice sound and stamping the talking
 * head-icon entirely. (owner label above is the mechanical first-touch and is
 * incorrect.) See globals.h. */
/* Source: uo_cgame_mp_x86.dll 0x3045818c. */

/* Source: uo_cgame_mp_x86.dll 0x304582ac (.data); refs=1 width=4; first=0x30021004.
 * RESOLVED: cg_vehicletrails_vmCvar.integer (+0xc of the vmCvar at 0x304582a0,
 * cg_cvarTable entry 183); no separate object exists at this address. The
 * owner=bg_animparseanimscript label was a first-touch artifact. */

/* Source: uo_cgame_mp_x86.dll 0x304583c8 (.data/.bss); refs=11 width=4 (float);
 * a global fade/overlay alpha scale. Multiplied into color alpha at every read
 * site (e.g. CG_FadeColor 0x3001d200: color[3] = scale * fadeFrac). Never
 * written in this DLL; engine/loader-provided. owner=bytetodir is a first-touch
 * artifact (rejected). Retyped uint32_t -> float; exact source name unresolved,
 * named by proven role.
 * Example: 3001af5a d9 05 c8 83 45 30  FLD float ptr [0x304583c8] |
 *          3001c646 d8 4c 24 10        FMUL DWORD PTR [esp+0x10]  (scale*alpha) */

/* Source: uo_cgame_mp_x86.dll 0x3045860c (.data); refs=4 width=4; first=0x30018fe5; owner=cmd_veh_fireturret.
 * Example: 30018fe5   a1 0c 86 45 30               MOV EAX,[0x3045860c] | 30022c38   a1 0c 86 45 30               MOV EAX,[0x3045860c] | 30035841   a1 0c 86 45 30               MOV EAX,[0x3045860c]
 */

/* Source: uo_cgame_mp_x86.dll 0x3045884c (.data); refs=1 width=4; first=0x3003ac00.
 * Enable gate for the guarded CG_LocalSound_f command handler (0x3003ac00); a zero
 * value makes that handler return immediately. Only reference in the DLL; no writer
 * present, so its exact source identity is unresolved and it is named by role.
 * Retyped uint32_t -> qboolean (TESTed as a truth value). owner=menuparse_forecolor
 * was a wrong size-guess first-touch label.
 * Example: 3003ac00   a1 4c 88 45 30               MOV EAX,[0x3045884c]
 */

/* Source: uo_cgame_mp_x86.dll 0x30458960 (.data). cg_cvarTable[178] identifies
 * this vmCvar_t as cl_serverloadgametype; consumers read .string at +0x10. */
vmCvar_t cl_serverloadgametype;
/* Source: uo_cgame_mp_x86.dll 0x30458bac (.data); refs=4 width=4; first=0x30018ff3; owner=cmd_veh_fireturret.
 * Example: 30018ff3   a1 ac 8b 45 30               MOV EAX,[0x30458bac] | 30022c45   a1 ac 8b 45 30               MOV EAX,[0x30458bac] | 3003584e   a1 ac 8b 45 30               MOV EAX,[0x30458bac]
 */

/* Source: uo_cgame_mp_x86.dll 0x30458ccc (.data); refs=1; first=0x3001fb2f.
 * cg_dumpAnims_vmCvar.integer — special DObj index finalized once per frame by
 * CG_AddPacketEntities (0x3001f810). See globals.h. owner=finishspawningitem was a
 * first-touch size-guess; retyped from uint32_t to the signed index the code reads. */

/* Source: uo_cgame_mp_x86.dll 0x30458dec (.data); refs=1 width=4; first=0x30018bc0; owner=cmd_veh_fireturret.
 * Example: 30018bc0   a1 ec 8d 45 30               MOV EAX,[0x30458dec]
 */
/* Source: uo_cgame_mp_x86.dll 0x30458f08 (.data): cg_errorDecay float value. */

/* Source: uo_cgame_mp_x86.dll 0x30458f0c (.data); refs=1 width=4; first=0x30035c34; owner=pmovesingle.
 * Example: 30035c34   a1 0c 8f 45 30               MOV EAX,[0x30458f0c]
 */

/* Source: uo_cgame_mp_x86.dll 0x3045902c (.data); refs=1 width=4.
 * cvar integer enable-flag for CG_DrawViewInfoOverlay (0x3001b2b0). Read only at
 * 0x3001b37c (CG_DrawInfoScreens dispatcher). Never written in .text. Renamed
 * from the wrong mechanical owner "cmd_fogswitch_f". */

/* 0x30459140: cg.clientFrame. Incremented once by CG_DrawActiveFrame; flame
 * emitter records use it as a last-touched frame stamp. */
int32_t cg_clientFrame = 0;
/* Source: uo_cgame_mp_x86.dll 0x30459144 (.data); refs=5 width=4; first=0x30022477.
 * cg.clientNum — installed from CG_Init's clientNum argument, used to index
 * bgs.clientinfo[] (0x30022477), and compared to cg_snap->clientNum
 * by the trap-54 HUD digit emitter (0x30031321) and 0x30037498. See globals.h.
 * Retyped uint32_t->int32_t; owner=scr_vehicle_think (a first-toucher) superseded.
 * Example: 30022477   8b 15 44 91 45 30            MOV EDX,dword ptr [0x30459144] | 3002dfaf   89 0d 44 91 45 30            MOV dword ptr [0x30459144],ECX | 30031321   3b 05 44 91 45 30            CMP EAX,dword ptr [0x30459144]
 */
int32_t cg_clientNum = 0;
/* cg_demoPlayback (0x30459148): written by the active-frame driver from its
 * demoPlayback argument and read as the live-input/prediction suppression gate. */
qboolean cg_demoPlayback = qfalse;
/* Source: uo_cgame_mp_x86.dll 0x3045914c (.data); refs=1 width=4; first=0x3001bfe1; owner=fire_lead.
 * Example: 3001bfe1   a1 4c 91 45 30               MOV EAX,[0x3045914c]
 */
uint32_t g_cgScreenSuppressFlag = (uint32_t)0x00000000u;
/* Source: uo_cgame_mp_x86.dll 0x30459150 (.data); refs=5 width=4.
 * cg_lockedViewFace — the locked/cube-face render selector: 0 means "normal view",
 * 1..6 select one of six axis-aligned camera orientations. The refdef dispatcher
 * (0x30041a30) tests it != 0 to branch into the locked-view builder (0x30040360),
 * which uses (value-1) as a 6-entry jump-table index picking the viewaxis basis.
 * Named by that proven role; exact source field name unresolved and other
 * consumers (0x3001bfee) not yet reconstructed. owner=fire_lead was the mechanical
 * first-toucher and is not the identity. */
int32_t cg_lockedViewFace = 0;
/* Source: uo_cgame_mp_x86.dll 0x30459154 (.data); refs=3 width=4.
 * cg_lockedViewSize — the locked-view square viewport base size (int). The
 * locked-view builder (0x30040360) sets refdef width == height == (this + 2) and
 * derives the square fov from atan2(size+2, size). Written together with
 * cg_lockedViewFace by the request setter at 0x300421b8. Named by proven role;
 * exact source field name unresolved. owner=cg_spawntracer was a size-collision
 * mislabel (the 0x30040360 builder is not CG_SpawnTracer). */
int32_t cg_lockedViewSize = 0;
/* Source: uo_cgame_mp_x86.dll 0x30459158 (.data); refs=6 width=4; first=0x30018026.
 * cg.latestSnapshotNum — see globals.h. Written by CG_ProcessSnapshots (0x3003d30a)
 * next to the "n < cg.latestSnapshotNum" error string; read as the "snap:%i" value
 * by CG_DrawSnapshot (0x30018026).
 * Example: 30018026   8b 0d 58 91 45 30            MOV ECX,dword ptr [0x30459158] | 3003d225   8b 0d 58 91 45 30            MOV ECX,dword ptr [0x30459158] | 3003d241   8b 0d 58 91 45 30            MOV ECX,dword ptr [0x30459158]
 */
int32_t cg_latestSnapshotNum = 0;
/* Source: uo_cgame_mp_x86.dll 0x3045915c (.data); refs=3 width=4; first=0x3003d2d4.
 * cg_latestSnapshotServerTime — the engine server time (ms) of the most recent
 * snapshot. Written by CG_ProcessSnapshots (0x3003d2d0), which passes its address as
 * the second out-parameter of cgame_syscall(CG_GET_CURRENT_SNAPSHOT_NUMBER, &n,
 * &cg_latestSnapshotServerTime) and then mirrors it into cg_effectAnimTime
 * (0x3003d31c: MOV [0x3053a034],ECX). Read as a "snapshot time" base to form elapsed
 * deltas from cg.time by two consumers: the lagometer/ping ring writer at 0x30018a10
 * (cg.time - it) and 0x300421e5 (SUB EAX, it). Superseded from the size-guessed
 * mechanical g_data_script_func_playfxontag_3045915c (owner=script_func_playfxontag
 * was a wrong size-match; this is a cgame client datum, not a server GSC builtin).
 * Example: 3003d2d4   68 5c 91 45 30               PUSH 0x3045915c | 3003d316   8b 0d 5c 91 45 30            MOV ECX,dword ptr [0x3045915c] | 300421e5   2b 05 5c 91 45 30            SUB EAX,dword ptr [0x3045915c]
 */
int32_t cg_latestSnapshotServerTime = 0;
/* Source: uo_cgame_mp_x86.dll 0x30459160 (.data); refs=137 width=4; first=0x300177a5; owner=pm_interuptweaponwithsprintmove.
 * Example: 300177a5   a1 60 91 45 30               MOV EAX,[0x30459160] | 3001802c   8b 15 60 91 45 30            MOV EDX,dword ptr [0x30459160] | 30018acc   8b 15 60 91 45 30            MOV EDX,dword ptr [0x30459160]
 */
/* cg_snap: the current client snapshot pointer. CG_InstallSnapshotResetEffects
 * (FUN_3003c9d0) stores the installed snapshot here; the transition loop
 * (FUN_3003d2d0) reads snap->snapFlags (+0x00) and snap->serverTime (+0x08)
 * through it. Retyped from the mechanical uint32_t/void* to snapshot_t *. */
/* Source: uo_cgame_mp_x86.dll 0x30459160. */
snapshot_t *cg_snap = NULL;
/* cg_nextSnap (0x30459164, .data): the next/incoming client snapshot pointer,
 * paired with cg_snap. CG_BuildSolidList (0x30035030) reads it and walks
 * snap->entities[] to rebuild the per-frame mark lists. Retyped from the mechanical
 * uint32_t (the owner=cg_asset_parse label was the first-touching function, not the
 * identity). refs=32.
 * Example: 30016570   8b 0d 64 91 45 30            MOV ECX,dword ptr [0x30459164] | 3003503a first MOV EDI,[0x30459164] read by the mark-list builder */
snapshot_t *cg_nextSnap = NULL;
/* Source: uo_cgame_mp_x86.dll 0x30459168 (.data): cg_activeSnapshots[0] — first
 * double-buffered snapshot slot (see globals.h). The mechanical export captured only
 * the first dword of this large snapshot_t; retyped to the full object. Its address
 * is taken as the trap_GetSnapshot destination and compared against cg_snap.
 * Example: 3003d260   81 3d 60 91 45 30 68 91 45 30 CMP dword ptr [0x30459160],0x30459168 | 3003d271   be 68 91 45 30               MOV ESI,0x30459168
 */
snapshot_t cg_activeSnapshots0;
/* Source: uo_cgame_mp_x86.dll 0x3046e188 (.data): cg_activeSnapshots[1] — second
 * double-buffered snapshot slot (see globals.h). Truncated to its first dword by the
 * mechanical export; retyped to the full snapshot_t object.
 * Example: 3003d26a   be 88 e1 46 30               MOV ESI,0x3046e188
 */
snapshot_t cg_activeSnapshots1;
/* Source: uo_cgame_mp_x86.dll 0x304831a8 (.data); refs=16 width=4; first=0x3001ea35; owner=fire_artillery.
 * cg.frameInterpolation: the [0,1) inter-snapshot lerp fraction. Written by
 * CG_SetFrameInterpolation (0x3001f710) and read as a float lerp weight by ~16
 * consumers (e.g. pushed as the `frac` arg to interpolation helper 0x3004bd00 at
 * 0x3001ea35/0x3001eae9/0x3001ebac). Retyped from the mechanical uint32_t to float;
 * the owner=fire_artillery label was the first-touching function, not the identity.
 * Example: 3001ea35   8b 15 a8 31 48 30            MOV EDX,dword ptr [0x304831a8] | 3001eae9   8b 15 a8 31 48 30            MOV EDX,dword ptr [0x304831a8] | 3001ebac   a1 a8 31 48 30               MOV EAX,[0x304831a8]
 */
float cg_frameInterpolation = 0.0f;
/* Source: uo_cgame_mp_x86.dll 0x304831ac (.data). cg.frametime — the current client
 * frame's elapsed time in ms (see globals.h). Retyped from the mechanical uint32_t
 * to int32_t; (owner=finishspawningitem) was the first-touching function, not the
 * identity. Consumed across ~38 sites as an int->float per-frame delta. */
int32_t cg_frametime = 0;
/* Source: uo_cgame_mp_x86.dll 0x304831b0 (.data); refs=146 width=4; first=0x30016605; owner=cg_asset_parse.
 * Example: 30016605   8b 15 b0 31 48 30            MOV EDX,dword ptr [0x304831b0] | 30016643   a1 b0 31 48 30               MOV EAX,[0x304831b0] | 30016684   8b 0d b0 31 48 30            MOV ECX,dword ptr [0x304831b0]
 */
uint32_t cg_time = (uint32_t)0x00000000u;
/* Source: uo_cgame_mp_x86.dll 0x304831b4 (.data); refs=7 width=4; first=0x30035b1c; owner=pmovesingle.
 * Example: 30035b1c   8b 15 b4 31 48 30            MOV EDX,dword ptr [0x304831b4] | 30035cfa   8b 15 b4 31 48 30            MOV EDX,dword ptr [0x304831b4] | 3003c9e3   a3 b4 31 48 30               MOV [0x304831b4],EAX
 */
int32_t cg_physicsTime = 0;
/* Source: uo_cgame_mp_x86.dll 0x304831b8 (.data); refs=6 width=4; first=0x30035132; owner=cg_registerimpacteffects.
 * Example: 30035132   a1 b8 31 48 30               MOV EAX,[0x304831b8] | 30035146   a1 b8 31 48 30               MOV EAX,[0x304831b8] | 3003557f   b8 b8 31 48 30               MOV EAX,0x304831b8
 */
int32_t cg_latestSnapshotTime = 0;
/* Source: uo_cgame_mp_x86.dll 0x304831bc (.data); refs=4 width=4; first=0x30034d4a; owner=cg_addscalefade.
 * Example: 30034d4a   89 15 bc 31 48 30            MOV dword ptr [0x304831bc],EDX | 30039580   c7 05 bc 31 48 30 01 00 00 00 MOV dword ptr [0x304831bc],0x1 | 3003d009   a1 bc 31 48 30               MOV EAX,[0x304831bc]
 */
qboolean cg_initialSnapshotPending = qfalse;
/* Source: uo_cgame_mp_x86.dll 0x304831c0 (.data); refs=17 width=4; first=0x30019bdd; owner=pm_viewheighttablelerp.
 * A boolean view/render-state flag written only as 0/1 at 0x30042558/0x30042564 (from a
 * `mode >= 6` compare). Consumers disagree on its exact meaning (a "not third-person"
 * effect-tag gate in CG_mg42_DoControllers vs. a "not ready" bail in
 * CG_LatchOverlaySource), so it is left mechanical until its writer (0x30042540) is
 * reconstructed.
 * Example: 30019bdd MOV EAX,[0x304831c0] | 30042558 MOV [0x304831c0],0 | 30042564 MOV [0x304831c0],1
 */
qboolean cg_thirdPerson = qfalse;
/* Source: uo_cgame_mp_x86.dll 0x304831c4 (.data); refs=31 width=4; first=0x3001a26a.
 * Example: 3001a26a   b9 c4 31 48 30               MOV ECX,0x304831c4 | 3001fa11   bf c4 31 48 30               MOV EDI,0x304831c4 | 3002081c   be c4 31 48 30               MOV ESI,0x304831c4
 * RESOLVED: cg.predictedPlayerState base object. Its address is
 * &cg.predictedPlayerState, passed to BG/CG helpers (e.g. CG_TouchItem hands it to
 * BG_PlayerTouchesItem and BG_CanItemBeGrabbed). owner=veh_playercollision was only
 * the first toucher, not the identity. */
playerState_t cg_predictedPlayerState = { 0 };
/* Source: uo_cgame_mp_x86.dll 0x304876c8..0x30487950 (.data).
 * Complete predicted local-player centity. Its nextState, interpolated fields,
 * event latches, and voice-chat fields are all members of this one object. */
centity_t cg_predictedEventEntity = {0};
/* Source: uo_cgame_mp_x86.dll 0x30487950 (.data); refs=7 width=4; first=0x30034e01.
 * RESOLVED: cg_prevAdsFraction — previous frame's ADS zoom fraction, read/written
 * as a float scalar by CG_TrackAdsZoomDirection (0x30045480) to detect this frame's
 * zoom direction, then updated to the current cg_predictedPlayerState.adsFraction. The
 * owner=cg_addscalefade label was only the first toucher: CG_AddScaleFade
 * (0x30034d40) STOSD-zero-fills a 12-dword scratch block starting here (resets it).
 * Retyped from the mislabeled uint32 placeholder to float.
 * Example: 30034e01   bf 50 79 48 30               MOV EDI,0x30487950 | 300454d0   d9 05 50 79 48 30            FLD float ptr [0x30487950] | 3004552b   a3 50 79 48 30               MOV [0x30487950],EAX
 */
float cg_prevAdsFraction = 0.0f;
/* Source: uo_cgame_mp_x86.dll 0x30487954 (.data); refs=5 width=4; first=0x30019560.
 * RESOLVED: cg_adsZoomingIn — qboolean written each frame at 0x30045521/0x30045533
 * (1 when adsFraction rose above cg_prevAdsFraction this frame, else 0) and read by
 * CG_CalcAdsOverlayFrac (0x30019520) to select adsZoomInFrac vs adsZoomOutFrac.
 * The owner=sp_trigger_mount_no_brush label was a wrong size-based auto-name.
 * Example: 30019560   a1 54 79 48 30               MOV EAX,[0x30487954] | 30045521   a3 54 79 48 30               MOV [0x30487954],EAX
 */
qboolean cg_adsZoomingIn = qfalse;
/* Source: uo_cgame_mp_x86.dll 0x30487958..0x30487963 (.data); three consecutive
 * width-4 floats. Superseded the mechanical uint32 triple
 * g_data_com_dprintf_30487958/5c/60: 0x30045070 (CG_ApplyWeaponMovementAngles) writes v[0],
 * v[1], v[2] of a scratch vec3 into these three slots as one vector store
 * (300450a5 MOV [58],EAX | 300450aa MOV [5c],ECX | 300450b0 MOV [60],EDX), so
 * this is a single vec3_t, not three scalars. It records the weapon-movement view
 * angle offset computed and applied this frame (the "owner=com_dprintf" label
 * was a wrong size-based auto-name). Sole writer; role-proven provisional name.
 */
vec3_t cg_weaponMovementAngles = { 0.0f, 0.0f, 0.0f };
/* Source: uo_cgame_mp_x86.dll 0x30487964 (.data); refs=2 width=4; first=0x3004670d; owner=bg_playerstatetoentitystate.
 * Example: 3004670d   8b 15 64 79 48 30            MOV EDX,dword ptr [0x30487964] | 3004686c   89 15 64 79 48 30            MOV dword ptr [0x30487964],EDX
 */
float cg_weaponPositionMoveScale = 0.0f;
/* Source: uo_cgame_mp_x86.dll 0x30487968..0x30487973 (.data). */
vec3_t cg_weaponMoveAngles = { 0.0f, 0.0f, 0.0f };
/* Source: uo_cgame_mp_x86.dll 0x30487974 (.data); refs=2 width=4; first=0x300466cd; owner=bg_playerstatetoentitystate.
 * Example: 300466cd   8b 15 74 79 48 30            MOV EDX,dword ptr [0x30487974] | 3004684f   89 15 74 79 48 30            MOV dword ptr [0x30487974],EDX
 */
vec3_t cg_weaponPositionPrevAngles = { 0.0f, 0.0f, 0.0f };
/* Source: uo_cgame_mp_x86.dll 0x30487978 (.data); refs=2 width=4; first=0x300466c8; owner=bg_playerstatetoentitystate.
 * Example: 300466c8   a1 78 79 48 30               MOV EAX,[0x30487978] | 30046859   a3 78 79 48 30               MOV [0x30487978],EAX
 */
/* Source: uo_cgame_mp_x86.dll 0x3048797c (.data); refs=2 width=4; first=0x300466dd; owner=bg_playerstatetoentitystate.
 * Example: 300466dd   8b 0d 7c 79 48 30            MOV ECX,dword ptr [0x3048797c] | 30046862   89 0d 7c 79 48 30            MOV dword ptr [0x3048797c],ECX
 */
/* Source: uo_cgame_mp_x86.dll 0x30487980 (.data); refs=28 width=4; first=0x3001952c.
 * RESOLVED: cg_currentWeaponInfo — pointer to the current predicted weapon's
 * weaponInfo_t record. Cached once per predicted frame at 0x30034d79 from
 * bg_weaponInfos[cg.predictedPlayerState.currentWeapon] (0x30034d64 loads the
 * bg_weaponInfos table, indexes by currentWeapon at 0x3048329c, stores the pointer
 * here). Read across cgame view/HUD code via typed field access, e.g.
 * CG_CalcAdsOverlayFrac (0x30019520): adsZoomInFrac +0x278, adsOverlayShader +0x280.
 * The owner=sp_trigger_mount_no_brush label was a wrong size-based auto-name.
 * Example: 3001952c   8b 15 80 79 48 30            MOV EDX,dword ptr [0x30487980] | 30034d79   a3 80 79 48 30               MOV [0x30487980],EAX
 */
weaponInfo_t *cg_currentWeaponInfo = 0;
/* Source: uo_cgame_mp_x86.dll 0x30487984: prediction-error decay timestamp. */
int32_t cg_predictedErrorTime = 0;
/* Source: uo_cgame_mp_x86.dll 0x30487988..0x30487993: three-component
 * positional prediction error. */
vec3_t cg_predictedError = { 0.0f, 0.0f, 0.0f };
/* Source: uo_cgame_mp_x86.dll 0x30487994 (.data). RESOLVED: cg_predictedEventSequence,
 * the predicted-event counter incremented by CG_CheckPlayerstateEvents. See
 * globals.h. Supersedes the mechanical owner=sp_script_vehicle. */
int32_t cg_predictedEventSequence = 0;
/* Source: uo_cgame_mp_x86.dll 0x30487998..0x304879d8 (.data). RESOLVED:
 * cg_predictedEvents[MAX_PREDICTED_EVENTS], the predicted-event id ring written by
 * CG_CheckPlayerstateEvents. See globals.h. Supersedes the mechanical single-dword
 * owner=sp_script_vehicle; the 16-entry array occupies the previously-zero span up
 * to the next mechanical entry at 0x304879d8. */
int32_t cg_predictedEvents[MAX_PREDICTED_EVENTS] = {0};
/* Source: uo_cgame_mp_x86.dll 0x304879d8 (.data); refs=7 width=4; first=0x30022c76; owner=cmd_callvote_f (first-touch artifact; real owner is CG_EntityEvent).
 * FLD/FMUL/FSTP-accessed float (retyped from uint32_t).
 * Example: 30022c76   d8 0d d8 79 48 30            FMUL float ptr [0x304879d8] | 30022ca1   d9 1d d8 79 48 30            FSTP float ptr [0x304879d8] | 30022ca9   d9 05 d8 79 48 30            FLD float ptr [0x304879d8]
 */
float cg_weaponChangeViewOffset = 0.0f;
/* Source: uo_cgame_mp_x86.dll 0x304879dc (.data); refs=5 width=4; first=0x30022c58; owner=cmd_callvote_f (first-touch artifact; real owner is CG_EntityEvent).
 * Example: 30022c58   8b 35 dc 79 48 30            MOV ESI,dword ptr [0x304879dc] | 30022cc6   89 0d dc 79 48 30            MOV dword ptr [0x304879dc],ECX | 30022cf1   89 0d dc 79 48 30            MOV dword ptr [0x304879dc],ECX
 */
int32_t cg_weaponChangeViewOffsetTime = 0;
/* Source: uo_cgame_mp_x86.dll 0x304879e0 (.data): cg_impactViewKick, the signed
 * local bullet/body-hit impulse applied by the view and weapon kick envelopes. */
float cg_impactViewKick = 0.0f;
/* Source: uo_cgame_mp_x86.dll 0x304879e4 (.data): cg_impactViewKickTime, the
 * cg.time stamp paired with cg_impactViewKick. */
int32_t cg_impactViewKickTime = 0;
/* Source: uo_cgame_mp_x86.dll 0x304879e8..0x30487a77 (.data); owner=finishspawningitem
 * (first-touch size-guess). cg_dobjPreviewOrientations[3] — three per-frame scratch
 * orientations rebuilt from cg_time by CG_AddPacketEntities (0x3001f810). See
 * globals.h for the full record layout and evidence. This single typed array
 * supersedes the ~25 mechanical g_data_finishspawningitem_* dwords that captured
 * only the individually-xref'd words of one contiguous 0x90-byte scratch block.
 * Zero-initialized in the exported image; rebuilt each frame. */
cgDObjPreviewOrientation_t cg_dobjPreviewOrientations[CG_DOBJ_PREVIEW_ORIENTATION_COUNT] = {0};
/* Source: uo_cgame_mp_x86.dll 0x30487a78 (.data); refs=16 width=4; first=0x3001967e; owner=stuckinclient.
 * Example: 3001967e   da 05 78 7a 48 30            FIADD dword ptr [0x30487a78] | 300198cc   da 05 78 7a 48 30            FIADD dword ptr [0x30487a78] | 30019cd6   da 05 78 7a 48 30            FIADD dword ptr [0x30487a78]
 */
/* Source: uo_cgame_mp_x86.dll 0x30487a7c (.data); refs=9 width=4; first=0x300196a0; owner=stuckinclient.
 * Example: 300196a0   da 05 7c 7a 48 30            FIADD dword ptr [0x30487a7c] | 300198a6   da 05 7c 7a 48 30            FIADD dword ptr [0x30487a7c] | 30019cae   da 05 7c 7a 48 30            FIADD dword ptr [0x30487a7c]
 */
/* Source: uo_cgame_mp_x86.dll 0x30487a80 (.data); refs=12 width=4; first=0x30019670; owner=stuckinclient.
 * Example: 30019670   db 05 80 7a 48 30            FILD dword ptr [0x30487a80] | 30019766   db 05 80 7a 48 30            FILD dword ptr [0x30487a80] | 300198b0   db 05 80 7a 48 30            FILD dword ptr [0x30487a80]
 */
/* Source: uo_cgame_mp_x86.dll 0x30487a84 (.data); refs=14 width=4; first=0x30019692; owner=stuckinclient.
 * Example: 30019692   db 05 84 7a 48 30            FILD dword ptr [0x30487a84] | 30019737   db 05 84 7a 48 30            FILD dword ptr [0x30487a84] | 30019779   db 05 84 7a 48 30            FILD dword ptr [0x30487a84]
 */
/* Source: uo_cgame_mp_x86.dll 0x30487a88 (.data); refs=6 width=4; first=0x30019448; owner=cg_parseimpacteffects.
 * Example: 30019448   d9 05 88 7a 48 30            FLD float ptr [0x30487a88] | 30019498   d9 05 88 7a 48 30            FLD float ptr [0x30487a88] | 3001a29f   d8 35 88 7a 48 30            FDIV float ptr [0x30487a88]
 */
/* Source: uo_cgame_mp_x86.dll 0x30487a8c (.data); refs=7 width=4; first=0x3001945f; owner=cg_parseimpacteffects.
 * Example: 3001945f   d9 05 8c 7a 48 30            FLD float ptr [0x30487a8c] | 300194e1   d9 05 8c 7a 48 30            FLD float ptr [0x30487a8c] | 30019fbd   d8 35 8c 7a 48 30            FDIV float ptr [0x30487a8c]
 */
/* Source: uo_cgame_mp_x86.dll 0x30487a90..0x30487a9b (.data) — cg_refdef.vieworg,
 * the current client view/camera origin vec3 (.x@0x30487a90, .y@0x30487a94,
 * .z@0x30487a98). Mechanical owner=bg_indexforstring is the first-touching-function
 * label and is wrong. Proven vec3: written as a 3-dword copy from a stack vec3
 * (0x3003fd39 MOV [0x30487a90],EDX / [0x30487a94],EAX / [0x30487a98],ECX) and
 * consumed as a vec3 (VectorDistance base at 0x3001b390; FLD/FADD/FSTP over the
 * three components in the camera-shake aggregate at 0x3001b669). Supersedes the
 * three prior uint32_t g_data_bg_indexforstring_30487a{90,94,98} placeholders
 * (one vec3, not three ints). Exact original cg field name not fully proved.
 */
/* Source: uo_cgame_mp_x86.dll 0x30487a9c..0x30487aa7 (.data) — cg_refdef.viewaxis[0],
 * the view/aim forward direction vec3 (.x@0x30487a9c, .y@0x30487aa0, .z@0x30487aa4),
 * stored right after cg_refdef.vieworg. Proven vec3: written as unit axis vectors by
 * 0x300403e1.. and FLD'd as a 3-dword triple by CG_ScanForCrosshairEntity
 * (0x3001a4d3/508/525) to form the crosshair trace endpoint, and by the tracer/fire
 * paths (0x30026c88/0x30034c45). The mechanical owner=cg_parseimpacteffects was only
 * the first toucher. Supersedes the three prior uint32_t placeholders (one vec3, not
 * three ints); exact CoD field name unproven, role and shape proven.
 * Example: 3001a4d3   d9 05 9c 7a 48 30   FLD float ptr [0x30487a9c] | 3001a508 FLD [0x30487aa0] | 3001a525 FLD [0x30487aa4]
 */
/* Source: uo_cgame_mp_x86.dll 0x30487aa8..0x30487ab3 (.data) — cg_refdef.viewaxis[1],
 * the second row of cg.refdef.viewaxis (Q3 viewaxis[1], the "left" basis vector),
 * a vec3 stored right after cg_refdef.viewaxis[0] (viewaxis[0]). Proven vec3: the
 * locked/cube-face refdef builder (0x30040360) writes .aa8/.aac/.ab0 as one of six
 * axis-aligned unit rows via its jump-table switch, and consumers FLD the three
 * consecutive dwords in order. Supersedes the three prior uint32_t
 * g_data_cg_parseimpacteffects_30487a{a8,ac,b0} placeholders (one vec3, not three
 * ints); owner=cg_parseimpacteffects was only the first toucher. */
/* Source: uo_cgame_mp_x86.dll 0x30487ab4..0x30487abf (.data) — cg_refdef.viewaxis[2],
 * the third row of cg.refdef.viewaxis (Q3 viewaxis[2], the "up" basis vector), a
 * vec3 stored right after cg_refdef.viewaxis[1]. Proven vec3: the locked/cube-face
 * refdef builder (0x30040360) writes .ab4/.ab8/.abc as one of six axis-aligned unit
 * rows via its jump-table switch. Supersedes the three prior uint32_t
 * g_data_cg_parseimpacteffects_30487a{b4,b8,bc} placeholders (one vec3, not three
 * ints); owner=cg_parseimpacteffects was only the first toucher. */
/* Source: uo_cgame_mp_x86.dll 0x30487ac0 (.data); refs=2 width=4; first=0x300420e7; owner=script_method_player_cloneplayer.
 * Example: 300420e7   a3 c0 7a 48 30               MOV [0x30487ac0],EAX | 3004264e   89 0d c0 7a 48 30            MOV dword ptr [0x30487ac0],ECX
 */
/* Source: uo_cgame_mp_x86.dll 0x30487ac4 (.data); refs=11 width=4; first=0x3001c3fb.
 * cg_refdef.rdflags — rdflags bitfield of the cg.refdef block (base 0x30487a78 + 0x4c)
 * submitted to trap_R_RenderScene. Toggled per cg_skybox_vmCvar.integer in CG_DrawActive
 * (bit 0x10) and by the refdef builders in the 0x30040/0x30042 cluster (bit 0x8).
 * Role name; owner=g_getnonpvsfriendlyinfo was a wrong size-match. */
/* Source: uo_cgame_mp_x86.dll 0x30487a78..0x30487ac7 (.data): the contiguous
 * renderer refdef prefix saved and restored as 0x50 bytes by CG_DrawSkyBoxPortal. */
refdef_t cg_refdef = { 0 };
/* Source: uo_cgame_mp_x86.dll 0x30487ac8..0x30487ad3 (.data).
 * cg_refdefViewAngles — contiguous pitch/yaw/roll Euler vector. CG_CalcVehicleViewPos
 * passes its base to AnglesToAxisNegRight at 0x30040f1c; CG_CalcViewShake updates all
 * three components at 0x3001b6ba..0x3001b716. */
vec3_t cg_refdefViewAngles = { 0.0f, 0.0f, 0.0f };
/* Source: uo_cgame_mp_x86.dll 0x30487ad4 (.data); previous view angles passed
 * on the stack to BG_CalculateWeaponPosition_Sway at 0x30044c9c.
 * Example: 30034d9e   89 15 d4 7a 48 30            MOV dword ptr [0x30487ad4],EDX | 30044c9c   68 d4 7a 48 30               PUSH 0x30487ad4
 */
vec3_t cg_weaponSwayViewAngles = { 0.0f, 0.0f, 0.0f };
/* Source: uo_cgame_mp_x86.dll 0x30487ad8 (.data); refs=1 width=4; first=0x30034d98; owner=cg_addscalefade.
 * Example: 30034d98   89 15 d8 7a 48 30            MOV dword ptr [0x30487ad8],EDX
 */
/* Source: uo_cgame_mp_x86.dll 0x30487adc (.data); refs=1 width=4; first=0x30034d92; owner=cg_addscalefade.
 * Example: 30034d92   89 15 dc 7a 48 30            MOV dword ptr [0x30487adc],EDX
 */
/* Source: uo_cgame_mp_x86.dll 0x30487ae0 (.data); refs=3 width=4; first=0x30034db0; owner=cg_addscalefade.
 * Example: 30034db0   89 15 e0 7a 48 30            MOV dword ptr [0x30487ae0],EDX | 30044ca1   bf e0 7a 48 30               MOV EDI,0x30487ae0 | 30046777   8b 0d e0 7a 48 30            MOV ECX,dword ptr [0x30487ae0]
 */
vec3_t cg_weaponSwayAngles = { 0.0f, 0.0f, 0.0f };
/* Source: uo_cgame_mp_x86.dll 0x30487ae4 (.data); refs=2 width=4; first=0x30034daa; owner=cg_addscalefade.
 * Example: 30034daa   89 15 e4 7a 48 30            MOV dword ptr [0x30487ae4],EDX | 30046781   8b 15 e4 7a 48 30            MOV EDX,dword ptr [0x30487ae4]
 */
/* Source: uo_cgame_mp_x86.dll 0x30487ae8 (.data); refs=2 width=4; first=0x30034da4; owner=cg_addscalefade.
 * Example: 30034da4   89 15 e8 7a 48 30            MOV dword ptr [0x30487ae8],EDX | 3004676e   a1 e8 7a 48 30               MOV EAX,[0x30487ae8]
 */
/* Source: uo_cgame_mp_x86.dll 0x30487aec (.data); positional sway output passed
 * in EAX to BG_CalculateWeaponPosition_Sway at 0x30044ca6.
 * Example: 30034dc2   89 15 ec 7a 48 30            MOV dword ptr [0x30487aec],EDX | 30044ca6   b8 ec 7a 48 30               MOV EAX,0x30487aec
 */
vec3_t cg_weaponSwayOffset = { 0.0f, 0.0f, 0.0f };
/* Source: uo_cgame_mp_x86.dll 0x30487af0 (.data); refs=2 width=4; first=0x30034dbc; owner=cg_addscalefade.
 * Example: 30034dbc   89 15 f0 7a 48 30            MOV dword ptr [0x30487af0],EDX | 300453d8   d8 25 f0 7a 48 30            FSUB float ptr [0x30487af0]
 */
/* Source: uo_cgame_mp_x86.dll 0x30487af4 (.data); refs=2 width=4; first=0x30034db6; owner=cg_addscalefade.
 * Example: 30034db6   89 15 f4 7a 48 30            MOV dword ptr [0x30487af4],EDX | 300453e2   d9 05 f4 7a 48 30            FLD float ptr [0x30487af4]
 */
/* Source: uo_cgame_mp_x86.dll 0x30487af8 (.data); the "DObj info" key table, 1023
 * dwords indexed by ESI*4. Mechanical export saw only slot 0 as a scalar
 * (owner=pm_clearaimdownsightflag is the first writer, not the real owner);
 * superseded with the proven 1023-entry array shape. See globals.h for the full
 * table description and accessor/consumer list. Writers seen:
 *   300163e4  MOV [ESI*4 + 0x30487af8],EDI   (CG_FreeRegisteredHandlesLow, null)
 *   3001648e  MOV [ESI*4 + 0x30487af8],EDI   (CG_FreeRegisteredHandlesHigh, null)
 *   30021ed2  CMP [ESI*4 + 0x30487af8],EAX   (CG_UpdateEntityDObjModel, match) */
uint32_t cg_dObjInfoKeys[ENTITYNUM_NONE];
/* Source: uo_cgame_mp_x86.dll 0x30488af4 (.data); the "DObj info" model table, 1023
 * dwords indexed by ESI*4 (immediately follows cg_dObjInfoKeys; next datum at
 * 0x30489af0). Same repair as cg_dObjInfoKeys above. Writers seen:
 *   300163ee  MOV [ESI*4 + 0x30488af4],EDI   (free helper, null)
 *   30016498  MOV [ESI*4 + 0x30488af4],EDI   (free helper, null)
 *   30021edb  CMP [ESI*4 + 0x30488af4],EDI   (CG_UpdateEntityDObjModel, match) */
XModel *cg_dObjInfoHandles[ENTITYNUM_NONE];
/* Source: uo_cgame_mp_x86.dll 0x30489af0 (.data); refs=2 width=4; first=0x30040358; owner=veh_setupcollmap.
 * Example: 30040358   d9 1d f0 9a 48 30            FSTP float ptr [0x30489af0] | 300426be   a1 f0 9a 48 30               MOV EAX,[0x30489af0]
 */
float cg_zoomSensitivity = 0.0f; /* 0x30489af0; see globals.h. */
/* Source: uo_cgame_mp_x86.dll 0x30489af4..0x30489ef3 (.data); 0x400 bytes; first=0x3002ba98.
 * cg_loadingScratch[1024] — "LOADING... %s" status scratch buffer; see globals.h. Spans
 * exactly to 0x30489ef3 (element [0x3ff], the forced NUL) with the next global at 0x30489ef4.
 * Supersedes the mechanical g_data_pm_updateviewangles_30489af4 (dword) and _30489ef3 (byte)
 * placeholders, which were two views of this one array.
 */
char cg_loadingScratch[MAX_STRING_CHARS] = {0};
/* Source: uo_cgame_mp_x86.dll 0x30489ef4 (.data); refs=3 width=4 — cg_vehicleViewReset, a
 * qboolean latch: CG_SnapshotTransitionStage2 (0x30034df2) sets it to 1 on a snapshot reset;
 * CG_CalcVehicleViewValues (0x30040580) reads it (0x30040675), reseeds cg_vehicleViewPrevAxis
 * when set, and clears it back to 0 (0x3004068c). owner=cg_addscalefade was a first-toucher.
 * See globals.h. */
int32_t cg_vehicleViewReset = 0;
/* Source: uo_cgame_mp_x86.dll 0x30489ef8..0x30489f1b (.data) — cg_vehicleViewPrevAxis, the
 * previous frame's vehicle/turret tag orientation basis (axis_t, 3x3). Read/written only by
 * CG_CalcVehicleViewValues (0x30040580) for view-angle smoothing; owner=convertquattomat was a
 * mechanical first-toucher. See globals.h. */
axis_t cg_vehicleViewPrevAxis = { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
/* Source: uo_cgame_mp_x86.dll 0x30489f1c (.data); refs=2 width=4; first=0x30037dd7; owner=yawvectors.
 * Example: 30037dd7   8b 0d 1c 9f 48 30            MOV ECX,dword ptr [0x30489f1c] | 30037df3   a3 1c 9f 48 30               MOV [0x30489f1c],EAX
 */
int32_t cgs_scoreboardTime = 0;
/* Source: uo_cgame_mp_x86.dll 0x30489f20 (.data); refs=9 width=4; first=0x30036ea8.
 * cg_scoreboardNumClients — see globals.h. Signed row-count clamped to
 * MAX_CLIENTS by the scoreboard build routine at 0x30038000.
 * Example: 30036ea8   8b 15 20 9f 48 30            MOV EDX,dword ptr [0x30489f20] | 3003808c   c7 05 20 9f 48 30 40 00 00 00   MOV dword ptr [0x30489f20],0x40
 */
int32_t cg_scoreboardNumClients = 0;
/* Source: uo_cgame_mp_x86.dll 0x30489f24..0x30489f33 (.data), 16 bytes.
 * cg_scoreboardTeamScores[TEAM_COUNT] — per-team scores sent as server-command args 2/3
 * and drawn in the score column; see globals.h. Supersedes the mechanical
 * per-element split (0x30489f28/0x30489f2c
 * as cg_entitypreevent, 0x30489f30 as g_touchtriggers): one team-indexed int
 * array, not four scalars. Team-indexed store 0x300380a1 MOV [0x30489f24],EAX and
 * loads 0x3003732d MOV EAX,[EBX*4 + 0x30489f24] (header) / 0x30037c0c-c11 (body).
 */
int32_t cg_scoreboardTeamScores[TEAM_COUNT] = { 0, 0, 0, 0 };
/* Source: uo_cgame_mp_x86.dll 0x30489f34..0x30489f43 (.data), 16 bytes.
 * cg_scoreboardTeamPings[TEAM_COUNT] — per-team ping sum, then integer average (totals-row
 * ping column); see globals.h. Supersedes the mechanical per-element split (0x30489f38/
 * 0x30489f3c/0x30489f40 as g_touchtriggers): one team-indexed int array, not four
 * scalars. Team-indexed accumulate 0x300382a2 ADD [EAX*4 + 0x30489f34],ECX and
 * load 0x30037336 MOV EAX,[EBX*4 + 0x30489f34] (header).
 */
int32_t cg_scoreboardTeamPings[TEAM_COUNT] = { 0, 0, 0, 0 };
/* Source: uo_cgame_mp_x86.dll 0x30489f44..0x30489f53 (.data); refs=6+4+4+4 width=4.
 * cg_scoreboardTeamCount[team_t] — see globals.h. Proven as a 4-element int
 * array indexed by team by 0x30038295 (`inc [0x30489f44 + eax*4]`) and
 * 0x30037121 (`mov eax,[ebx*4 + 0x30489f44]`).
 */
int32_t cg_scoreboardTeamCount[TEAM_COUNT] = { 0, 0, 0, 0 };
/* Source: uo_cgame_mp_x86.dll 0x30489f54..0x3048a553 (.data), 0x600 bytes.
 * cg_scoreboardEntries[64] — collected scoreboard rows (cgScore_t, stride 0x18),
 * see globals.h. Supersedes the mechanical single-dword symbol
 * g_data_item_correctedtextrect_30489f54: the build routine at 0x30038060 `rep
 * stosd`s 0x180 dwords here (0x600 == 64*0x18, ending exactly at 0x3048a554), so
 * the datum is a 64-element array, not one dword; the item_correctedtextrect
 * owner label was the exporter's wrong size-match guess.
 * Example: 3003783d   be 54 9f 48 30   MOV ESI,0x30489f54 (ScoresList base) |
 *          300380ff   bf 54 9f 48 30   MOV EDI,0x30489f54 (build STOSD base) |
 *          30038154   be 54 9f 48 30   MOV ESI,0x30489f54 (build fill base)
 */
cgScore_t cg_scoreboardEntries[64];
/* Source: uo_cgame_mp_x86.dll 0x3048a554 (.data); refs=7 width=4; first=0x3001af19; owner=bytetodir.
 * Example: 3001af19   a1 54 a5 48 30               MOV EAX,[0x3048a554] | 3001afd3   a1 54 a5 48 30               MOV EAX,[0x3048a554] | 3001bd35   c7 05 54 a5 48 30 01 00 00 00 MOV dword ptr [0x3048a554],0x1
 */
qboolean cg_scoreboardShowing = qfalse;
/* Source: uo_cgame_mp_x86.dll 0x3048a55c (.data); refs=5 width=4; first=0x3001af46; owner=bytetodir.
 * Example: 3001af46   8b 15 5c a5 48 30            MOV EDX,dword ptr [0x3048a55c] | 3001afec   8b 15 5c a5 48 30            MOV EDX,dword ptr [0x3048a55c] | 3001bd30   a3 5c a5 48 30               MOV [0x3048a55c],EAX
 */
int32_t cg_scoreboardShowTime = 0;
/* Source: uo_cgame_mp_x86.dll 0x3048a560 (.data); refs=13 width=4; first=0x300327c3; owner=com_parseonline.
 * Example: 300327c3   a1 60 a5 48 30               MOV EAX,[0x3048a560] | 300327d2   a3 60 a5 48 30               MOV [0x3048a560],EAX | 300327d9   c7 05 60 a5 48 30 00 00 00 00 MOV dword ptr [0x3048a560],0x0
 */
int32_t cg_scoreboardScrollPos = 0;
/* Source: uo_cgame_mp_x86.dll 0x3048a564 (.data); refs=6 width=4; first=0x300370a2.
 * cg_scoreboardOverflowed — the scoreboard "flowed off the bottom" latch; see
 * globals.h. Read at 0x300370a2 / 0x30037420, set to 1 at 0x300370f0, cleared per
 * frame by the body drawer. Mechanical owner=g_damage was a wrong size-match guess.
 * Example: 300370a2   a1 64 a5 48 30               MOV EAX,[0x3048a564] | 300370f0   c7 05 64 a5 48 30 01 00 00 00 MOV dword ptr [0x3048a564],0x1 | 30037420   a1 64 a5 48 30               MOV EAX,[0x3048a564]
 */
qboolean cg_scoreboardOverflowed = qfalse;
/* Source: uo_cgame_mp_x86.dll 0x3048a568 (.data); the scoreboard "fragged by" name
 * buffer char[0x20]. Supersedes the mis-captured cg_scoreboardFadedOut byte and the
 * aliased 0x3048a587 terminator symbol (both were byte slices of this one string).
 * Example: 3002251d PUSH 0x3048a568 (strncpy dest) | 30031a90 MOV AL,[0x3048a568]
 * (name[0] gate) | 30031aaa PUSH 0x3048a568 (va %s). See globals.h for the full
 * proof (strncpy count 0x1f + terminator zero at 0x3048a587). */
char cg_fraggedByName[32] = { 0 };
/* Source: uo_cgame_mp_x86.dll 0x3048a588..0x3048a9ac (.data).
 * Dormant spectator/HUD fields in the cg aggregate. The preceding name buffer
 * has a proven bound, and no instruction or relocation addresses this interval. */
uint8_t cg_unreferencedSpectatorState[1060];
/* cg.centerPrint* HUD state. Resolved from CG_PriorityCenterPrint (0x30019050,
 * "Center Print" tag @0x30076cdc) and the renderer at 0x300191b0. Contiguous
 * layout: time(4) @a9ac, charWidth(4) @a9b0, y(4) @a9b4,
 * string[1024] @a9b8..adb8,
 * lines(4) @adb8, priority(4) @adbc. (Mechanical owner=pm_weapon_allowreload
 * was a size-guess mislabel; 0x3048adb7 was the last byte of the string buffer,
 * not a separate datum.) */
/* Source: uo_cgame_mp_x86.dll 0x3048a9ac (.data). Written cg_time+2000 @0x30019102. */
int32_t cg_centerPrintTime = 0;
/* Source: uo_cgame_mp_x86.dll 0x3048a9b0 (.data). (int)charWidth @0x3001911b; FILD @0x30019220. */
int32_t cg_centerPrintCharWidth = 0;
/* Source: uo_cgame_mp_x86.dll 0x3048a9b4 (.data). (int)y @0x30019111; FILD @0x30019256. */
int32_t cg_centerPrintY = 0;
/* Source: uo_cgame_mp_x86.dll 0x3048a9b8 (.data). Q_strncpyz dest, destsize 1024;
 * last byte @0x3048adb7 forced to 0 at 0x3001909d. */
char cg_centerPrintString[MAX_STRING_CHARS] = {0};
/* Source: uo_cgame_mp_x86.dll 0x3048adb8 (.data). Set 1 @0x30019127; INC @0x30019157/0x30019175. */
int32_t cg_centerPrintLines = 0;
/* Source: uo_cgame_mp_x86.dll 0x3048adbc (.data). Read @0x30019060; written @0x30019090. */
int32_t cg_centerPrintPriority = 0;
/* Source: uo_cgame_mp_x86.dll 0x3048adc0..0x3048ade4 (.bss, zero-init).
 * cg_screenFade: the full-screen fade/flash overlay state block. The mechanical export
 * split it into ten g_data_script_func_getangledelta_* dwords with a bogus owner/name
 * (the rejected size-guess). The sole consumer, CG_DrawFade (0x3001ab90), proves the
 * real shape: endTime (int, cg_time ms) at +0x00, rate (float, 0 => inactive) at +0x04,
 * a current RGBA at +0x08, and a stored source RGBA at +0x18. Superseded to one typed
 * struct. See cg_screen_fade_t in globals.h. */
cg_screen_fade_t cg_screenFade = {0};
/* Source: uo_cgame_mp_x86.dll 0x3048ade8 (.data); refs=3 width=4; first=0x30034a76.
 * cg_outOfAmmoState: persistent low/out-of-ammo warning state, owned solely by
 * CG_OutOfAmmoChange (0x30034a00). 0 = has ammo / not warned; 1 = warned with some
 * ammo remaining; 2 = warned with zero ammo. Reset to 0 when the scaled ammo total
 * reaches threshold. Retyped from the mechanical uint32_t (owner=quateigentrace was
 * the first-touching function label, not the identity).
 * Example: 30034a76   8b 0d e8 ad 48 30            MOV ECX,dword ptr [0x3048ade8] | 30034aa1   89 0d e8 ad 48 30            MOV dword ptr [0x3048ade8],ECX | 30034ab0   c7 05 e8 ad 48 30 00 00 00 00 MOV dword ptr [0x3048ade8],0x0
 */
int32_t cg_outOfAmmoState = 0;
/* Source: uo_cgame_mp_x86.dll 0x3048adec (.data); refs=4 width=4; first=0x3001a59c.
 * RESOLVED (role): cg_crosshairEntNum — the client/entity number under the crosshair.
 * CG_ScanForCrosshairEntity (0x3001a4d0) writes the traced entity number here
 * (0x3001a59c MOV [0x3048adec],ECX) after gating on hittable contents, != local
 * clientNum, and < MAX_CLIENTS; the crosshair-name HUD drawer (0x3001a65a/0x3001a6ea)
 * reads it as the va("%s", name) index. The mechanical owner=cg_drawscoreboard was
 * only the first-touching label and is itself a wrong name-guess (see
 * CG_ScanForCrosshairEntity). Exact CoD symbol unconfirmed; named by proven role.
 */
int32_t cg_crosshairEntNum = 0;
/* Source: uo_cgame_mp_x86.dll 0x3048adf0 (.data); refs=3 width=4; first=0x3001a5a2.
 * RESOLVED (role): cg_crosshairEntTime — the cg_time latched when the crosshair
 * entity was last acquired. CG_ScanForCrosshairEntity stores cg_time here
 * (0x3001a5a2 MOV [0x3048adf0],EAX); the drawer at 0x3001a640 uses it as the
 * CG_FadeColor start time so the name fades after the target is lost. Provisional
 * role name; exact CoD symbol unconfirmed.
 */
int32_t cg_crosshairEntTime = 0;
/* Source: uo_cgame_mp_x86.dll 0x3048adfc (.data); refs=2 width=4; first=0x3001a6f0.
 * cg_crosshairHealthEntNum — entity whose health is latched for the crosshair-name
 * tint; written at 0x3003cfab, compared to cg_crosshairEntNum at 0x3001a6f0.
 * Example: 3001a6f0   3b 15 fc ad 48 30            CMP EDX,dword ptr [0x3048adfc] | 3003cfab   a3 fc ad 48 30               MOV [0x3048adfc],EAX
 */
int32_t cg_crosshairHealthEntNum = 0;
/* Source: uo_cgame_mp_x86.dll 0x3048ae00 (.data); refs=2 width=4; first=0x3001a6f8.
 * cg_crosshairHealth — latched health (0..100) of cg_crosshairHealthEntNum;
 * written at 0x3003cfb6, FILDed and scaled by 0.01 at 0x3001a6f8 for the name tint.
 * Example: 3001a6f8   db 05 00 ae 48 30            FILD dword ptr [0x3048ae00] | 3003cfb6   89 0d 00 ae 48 30            MOV dword ptr [0x3048ae00],ECX
 */
int32_t cg_crosshairHealth = 0;
/* Source: uo_cgame_mp_x86.dll 0x3048ae08 (.data); refs=5 width=4; first=0x3001a5e6; owner=pm_jumpforsurface.
 * Example: 3001a5e6   89 0d 08 ae 48 30            MOV dword ptr [0x3048ae08],ECX | 300303e0   8b 35 08 ae 48 30            MOV ESI,dword ptr [0x3048ae08] | 3003042e   89 3d 08 ae 48 30            MOV dword ptr [0x3048ae08],EDI
 */
cursorHint_t cg_usableHintKind = CURSOR_HINT_OFF; /* 0x3048ae08 */
/* Source: uo_cgame_mp_x86.dll 0x3048ae0c (.data); refs=4 width=4; first=0x3001a5d4; owner=pm_jumpforsurface.
 * Example: 3001a5d4   89 0d 0c ae 48 30            MOV dword ptr [0x3048ae0c],ECX | 3003040c   8b 15 0c ae 48 30            MOV EDX,dword ptr [0x3048ae0c] | 3003049c   a1 0c ae 48 30               MOV EAX,[0x3048ae0c]
 */
/* CG_FadeColor start time (startMsec): latched from cg_time by
 * CG_LatchOverlaySource (0x3001a5d4) and consumed as the fade start by the
 * drawer's CG_FadeColor call (FUN_3001d200 does cg_time - startTime). */
/* Source: uo_cgame_mp_x86.dll 0x3048ae0c. */
uint32_t cg_overlayFadeStartTime = (uint32_t)0x00000000u;
/* Source: uo_cgame_mp_x86.dll 0x3048ae10 (.data); refs=3 width=4; first=0x3001a5da; owner=pm_jumpforsurface.
 * Example: 3001a5da   89 15 10 ae 48 30            MOV dword ptr [0x3048ae10],EDX | 30030406   8b 0d 10 ae 48 30            MOV ECX,dword ptr [0x3048ae10] | 30039543   89 35 10 ae 48 30            MOV dword ptr [0x3048ae10],ESI
 */
/* CG_FadeColor duration (totalMsec): latched by CG_LatchOverlaySource and
 * consumed as the fade length by the drawer's CG_FadeColor call. */
/* Source: uo_cgame_mp_x86.dll 0x3048ae10. */
uint32_t cg_overlayFadeDuration = (uint32_t)0x00000000u;
/* Source: uo_cgame_mp_x86.dll 0x3048ae14 (.data); refs=3 width=4; first=0x3001a5f2; owner=pm_jumpforsurface.
 * Example: 3001a5f2   89 15 14 ae 48 30            MOV dword ptr [0x3048ae14],EDX | 30030be1   a1 14 ae 48 30               MOV EAX,[0x3048ae14] | 30030c08   db 05 14 ae 48 30            FILD dword ptr [0x3048ae14]
 */
int32_t cg_usableHintColorByte = 0; /* 0x3048ae14 */
/* Source: uo_cgame_mp_x86.dll 0x3048ae18 (.data); refs=2 width=4; first=0x3001a5fe; owner=pm_jumpforsurface.
 * Example: 3001a5fe   a3 18 ae 48 30               MOV [0x3048ae18],EAX | 30030973   a1 18 ae 48 30               MOV EAX,[0x3048ae18]
 */
int32_t cg_usableHintCommandIndex = 0; /* 0x3048ae18 */
/* Source: uo_cgame_mp_x86.dll 0x3048ae24 (.data) — cg_damageFeedbackTime: cg.time
 * latched at the very top of CG_DamageFeedback (0x30034acc copies cg_time here
 * unconditionally on every hit). Write-only in this DLL (refs=1, no reader here);
 * a "time of last damage-feedback" latch. The owner=cg_addpacketentities label was
 * a size-match artifact; renamed by proven role.
 * Example: 30034acc   a3 24 ae 48 30               MOV [0x3048ae24],EAX
 */
uint32_t cg_damageFeedbackTime = (uint32_t)0x00000000u;
/* Source: uo_cgame_mp_x86.dll 0x3048ae28 (.data); refs=2 width=4; first=0x3001ab00; owner=pm_weapon_finishweaponbreakdown.
 * Example: 3001ab00   a1 28 ae 48 30               MOV EAX,[0x3048ae28] | 3001ab44   c7 05 28 ae 48 30 00 00 00 00 MOV dword ptr [0x3048ae28],0x0
 */
uint32_t s_voiceMenuStartTime = (uint32_t)0x00000000u;
/* Source: uo_cgame_mp_x86.dll 0x3048ae38 (.data); refs=1 width=4; first=0x30022666; owner=vectordistance2d.
 * Example: 30022666   a3 38 ae 48 30               MOV [0x3048ae38],EAX
 */
int32_t cg_lastRequestedWeapon = 0;
/* Source: uo_cgame_mp_x86.dll 0x3048ae3c (.data); refs=2 width=4; first=0x30022678; owner=vectordistance2d.
 * Example: 30022678   89 0d 3c ae 48 30            MOV dword ptr [0x3048ae3c],ECX | 3003953d   89 35 3c ae 48 30            MOV dword ptr [0x3048ae3c],ESI
 */
int32_t cg_weaponSelectTimeA = 0;
/* Source: uo_cgame_mp_x86.dll 0x3048ae40 (.data); refs=1 width=4; first=0x3002267e; owner=vectordistance2d.
 * Example: 3002267e   89 0d 40 ae 48 30            MOV dword ptr [0x3048ae40],ECX
 */
int32_t cg_weaponSelectTimeB = 0;
/* Source: uo_cgame_mp_x86.dll 0x3048ae44 (.data); width=4.
 * cg_weaponSelectTime — cg.time (ms) of the last weapon (re)selection; drives the
 * fading weapon-name overlay (see globals.h). Zero-initialized in .data. Written by
 * the weapon-select cluster and read as CG_FadeColor's startMsec. The mechanical
 * owner=vectordistance2d label was only a first-touch artifact. */
int32_t cg_weaponSelectTime = 0;
/* Source: uo_cgame_mp_x86.dll 0x3048ae50 (.data); refs=2 width=4; first=0x300395af; owner=cg_drawsoundoverlay.
 * Example: 300395af   a3 50 ae 48 30               MOV [0x3048ae50],EAX | 3004734f   81 fd 50 ae 48 30            CMP EBP,0x3048ae50
 */
float cg_weaponSelectSlotScale[8];
/* Source: uo_cgame_mp_x86.dll 0x3048ae70 (.data); refs=3 width=4; first=0x300395e3; owner=cg_drawsoundoverlay.
 * Example: 300395e3   89 35 70 ae 48 30            MOV dword ptr [0x3048ae70],ESI | 30046bbb   8b 15 70 ae 48 30            MOV EDX,dword ptr [0x3048ae70] | 30046bc9   a3 70 ae 48 30               MOV [0x3048ae70],EAX
 */
int32_t cg_weaponSelectLastTime = 0;
/* Source: uo_cgame_mp_x86.dll 0x3048ae74..0x3048aed3 (.data) — cg_damageDirIndicators[8],
 * the active damage-direction HUD arrow ring (serverTime/duration/yaw per slot).
 * Registered by CG_AddDamageDirection (0x30034ac0), drawn by CG_DrawDamageDirectionIndicators
 * (0x3001a980), zeroed on reset (0x30039595 / 0x30034e36). Supersedes the mechanical
 * g_data_cg_ejectweaponbrass_3048ae74 array and the g_data_cg_addpacketentities_3048ae7{8,c}
 * field placeholders (a brass/shell size-guess; this is a 2D indicator ring). See globals.h.
 * Example: 3001aa1c   be 74 ae 48 30               MOV ESI,0x3048ae74 | 30034ca1   89 86 74 ae 48 30            MOV dword ptr [ESI + 0x3048ae74],EAX
 */
cg_damageDirIndicator_t cg_damageDirIndicators[CG_DAMAGE_DIRECTION_SLOT_COUNT] = {{0, 0, 0.0f}};
/* Source: uo_cgame_mp_x86.dll 0x3048aed4 (.data) — cg_damageDirLatestServerTime, latched
 * right after the array (writer 0x30034d28). Mechanical owner=cg_ejectweaponbrass rejected.
 * Example: 3001aaeb   81 fe d4 ae 48 30            CMP ESI,0x3048aed4 | 30034d28   89 0d d4 ae 48 30            MOV dword ptr [0x3048aed4],ECX
 */
int32_t cg_damageDirLatestServerTime = 0;
/* Source: uo_cgame_mp_x86.dll 0x3048aee8 (.data); refs=2 width=4; first=0x3001a13c; owner=veh_playercollision.
 * Example: 3001a13c   a1 e8 ae 48 30               MOV EAX,[0x3048aee8] | 3001a170   89 35 e8 ae 48 30            MOV dword ptr [0x3048aee8],ESI
 */
int32_t cg_grenadePulseLastSpecialTime = 0;
/* Source: uo_cgame_mp_x86.dll 0x3048aeec (.data); refs=1 width=4; first=0x3002e54b.
 * cg_suppressMarksGate: nonzero => CG_ImpactMark returns without making a
 * mark. Exact source name unresolved (single reader, no recovered writer); address
 * suffix retained as the disambiguator. owner=veh_unlinkplayer label rejected.
 * Example: 3002e54b   a1 ec ae 48 30               MOV EAX,[0x3048aeec]
 */
int32_t cg_suppressMarksGate = 0;
/* Source: uo_cgame_mp_x86.dll 0x3048af0c (.data); refs=3 width=4; first=0x3001a8f0; owner=dynasink (rejected).
 * cg_damageFlashEndTime: consumed by CG_DrawFlashDamage (0x3001a8e0), set to
 * cg_time+500 by the trigger path (0x30034d1e) and zeroed by reset (0x3003959d).
 */
int32_t cg_damageFlashEndTime = 0;
/* Source: uo_cgame_mp_x86.dll 0x3048af10 (.data); refs=6 width=4; first=0x3001a90d; owner=dynasink (rejected).
 * cg_damageFlashScale: signed float magnitude of the damage flash; FMUL'd into the
 * fade alpha by CG_DrawFlashDamage (0x3001a90d), written by 0x30034b3c/0x30034c6b.
 */
float cg_damageFlashScale = 0.0f;
/* Source: uo_cgame_mp_x86.dll 0x3048af14 (.data) — cg_damageFlashX: the horizontal
 * component of the directional damage blend, paired with cg_damageFlashScale (the
 * value/magnitude at 0x3048af10). CG_DamageFeedback (0x30034ac0) writes them as a
 * {X, value} pair: X = -DotProduct(cg_refdef.viewaxis[1], damageDir)*kick (0x30034c3f),
 * and the all-directions special case (yaw==255 && pitch==255) sets X=0 (0x30034b30).
 * A float, read together with cg_damageFlashScale by the damage-blend renderers
 * (0x3003fbba, 0x30046729) and cleared by the effect reset (0x30034e13). This is the
 * classic Quake3/CoD cg.damageX. owner=cg_addpacketentities was a size-match artifact.
 * Example: 30034b30   c7 05 14 af 48 30 00 00 00 00 MOV dword ptr [0x3048af14],0x0 | 30034c3f   d9 1d 14 af 48 30            FSTP float ptr [0x3048af14] | 30034e13   a3 14 af 48 30               MOV [0x3048af14],EAX
 */
float cg_damageFlashX = 0.0f;
/* Source: uo_cgame_mp_x86.dll 0x3048af18: current bobCycle phase in radians. */
float cg_bobCyclePhase = 0.0f;
/* Source: uo_cgame_mp_x86.dll 0x3048af1c (.data); refs=7 width=4; first=0x30034dec; owner=cg_addscalefade.
 * Example: 30034dec   89 15 1c af 48 30            MOV dword ptr [0x3048af1c],EDX | 3003fbb0   8b 0d 1c af 48 30            MOV ECX,dword ptr [0x3048af1c] | 3003fd50   8b 35 1c af 48 30            MOV ESI,dword ptr [0x3048af1c]
 */
float cg_weaponMoveSpeed = 0.0f;
/* Source: uo_cgame_mp_x86.dll 0x3048af20..0x3048b000 (.data).
 * Unreferenced loader-zero view/weapon state in the original cg aggregate. */
uint8_t cg_unreferencedViewWeaponStateA[224];
/* Source: uo_cgame_mp_x86.dll 0x3048b000 (.data); refs=1 width=4; first=0x300465f4; owner=bg_playerstatetoentitystate.
 * Example: 300465f4   39 35 00 b0 48 30            CMP dword ptr [0x3048b000],ESI
 */
qboolean cg_viewWeaponSuppressed = qfalse;
/* Source: uo_cgame_mp_x86.dll 0x3048b004..0x3048b057 (.bss, zero-initialized).
 * The fxTest command state shared by CG_FxTest (0x3003f430) and the periodic
 * emitter (0x30042110): a 64-byte effect-name string, world origin, last emit
 * time, and interval. Q_strncpyz(..., 63) plus the explicit NUL store at +0x3f
 * proves that 0x3048b004 is char[64], not an effectDef_t payload. */
char         cg_periodicEffectName[MAX_QPATH]; /* 0x3048b004: name handed to CG_FX_REGISTER_EFFECT */
vec3_t      cg_periodicEffectOrigin;      /* 0x3048b044: origin handed to CG_PLAY_EFFECT_ORIGIN */
int32_t     cg_periodicEffectLastTime;    /* 0x3048b050: cg.time at last emit */
int32_t     cg_periodicEffectInterval;    /* 0x3048b054: min ms between emits (>=1 gate) */
/* Source: uo_cgame_mp_x86.dll 0x3048b058..0x3048b063 (.data) — cg_viewKickVel,
 * the view-kick angular-velocity vec3 (.x@0x3048b058, .y@0x3048b05c, .z@0x3048b060).
 * CG_UpdateViewKick (0x3003f9f0) iterates it as a 3-float array in lockstep with
 * cg_viewKickAngles (integrates a centering acceleration into each component,
 * zeroes a component on center/clamp). Init function (0x30034de6/de0/dda) zeroes
 * all three; 0x30042592/88/7e also zeroes them as a unit. Consolidates the three
 * uint32_t g_data_cg_addscalefade_3048b05{8,c}/3048b060 placeholders into one vec3.
 */
vec3_t cg_viewKickVel = { 0.0f, 0.0f, 0.0f };
/* Source: uo_cgame_mp_x86.dll 0x3048b064..0x3048b06f (.data) — cg_viewKickAngles,
 * the view-kick angular-offset vec3 (.x@0x3048b064, .y@0x3048b068, .z@0x3048b06c)
 * added to the view during first-person rendering. CG_UpdateViewKick (0x3003f9f0)
 * integrates cg_viewKickVel into it, clamps each component to +/-10.0 deg, and
 * resets to 0 on a zero crossing. Other view code reads the components (FADD
 * [0x3048b064] at 0x300426f7, [0x3048b068] at 0x3004270a, [0x3048b06c] at
 * 0x3004271a). Consolidates the three uint32_t g_data_cg_addscalefade_3048b06{4,8,c}
 * placeholders into one vec3.
 */
vec3_t cg_viewKickAngles = { 0.0f, 0.0f, 0.0f };
/* Source: uo_cgame_mp_x86.dll 0x3048b070..0x3048b07b (.data) — cg_adsViewErrorAngles,
 * the persistent ADS view-error (idle aim-wander) angular-offset vec3
 * (.x@0x3048b070, .y@0x3048b074, .z@0x3048b078). CG_UpdateAdsViewError (0x30036070)
 * steps .x and .y (FADD/FSTP [0x3048b070] at 0x30036180/0x3003618e, [0x3048b074] at
 * 0x3003619c/0x300361aa); the sway integrator at 0x30041746 writes .z. The view
 * builder at 0x300426ed adds this vec3 to cg_viewKickAngles (FLD [0x3048b070] +
 * [0x3048b064] at 0x300426ed/0x300426f7, and the parallel .y/.z at 0x30042704/
 * 0x30042714). Consolidates the three uint32_t g_data_*_3048b07{0,4,8} placeholders
 * into one vec3; the scriptent_moveaxis / g_moverpush owner labels were mechanical
 * first-touchers and are wrong.
 */
vec3_t cg_adsViewErrorAngles = { 0.0f, 0.0f, 0.0f };
/* Source: uo_cgame_mp_x86.dll 0x3048b07c..0x3048b0b8 (.data).
 * Five dormant vec3-sized lanes in the contiguous view-kick/ADS state block. */
vec3_t cg_unreferencedViewVectors[5];
/* Source: uo_cgame_mp_x86.dll 0x3048b0b8 (.data); refs=4 width=4; first=0x30019379; owner=cg_parseimpacteffects.
 * Example: 30019379   a1 b8 b0 48 30               MOV EAX,[0x3048b0b8] | 3004680f   d9 1d b8 b0 48 30            FSTP float ptr [0x3048b0b8] | 30046838   a3 b8 b0 48 30               MOV [0x3048b0b8],EAX
 */
float cg_effectProjAnglePitch = 0.0f;
/* Source: uo_cgame_mp_x86.dll 0x3048b0bc (.data); refs=4 width=4; first=0x30019373; owner=cg_parseimpacteffects.
 * Example: 30019373   8b 0d bc b0 48 30            MOV ECX,dword ptr [0x3048b0bc] | 30046822   d9 1d bc b0 48 30            FSTP float ptr [0x3048b0bc] | 3004683d   89 0d bc b0 48 30            MOV dword ptr [0x3048b0bc],ECX
 */
float cg_effectProjAngleYaw = 0.0f;
/* Source: uo_cgame_mp_x86.dll 0x3048b0c0..0x3048b0cb (.data) — cg_adsViewOffset,
 * the ADS view-relative offset vec3 (.x@0x3048b0c0, .y@0x3048b0c4, .z@0x3048b0c8).
 * CG_CalcAdsViewOffset (0x300451a0) writes it as one vec3 —
 * out[i] = (pos[i] - cg_refdef.vieworg[i]) * adsFraction — and both it and
 * 0x30046a00 zero all three dwords when ADS is inactive. Supersedes the three
 * prior uint32_t g_data_script_method_scriptbuiltin_setrig_3048b0c{0,4,8}
 * placeholders (one vec3, not three ints). Exact original cg field name not
 * fully proved.
 * Example: 300451d9   d9 1d c0 b0 48 30            FSTP float ptr [0x3048b0c0] | 30045206   c7 05 c0 b0 48 30 00 00 00 00 MOV dword ptr [0x3048b0c0],0x0
 */
vec3_t cg_adsViewOffset = { 0.0f, 0.0f, 0.0f };
/* Source: uo_cgame_mp_x86.dll 0x3048b0cc (.data); refs=3 width=4; first=0x30034e22; owner=cg_addscalefade.
 * Example: 30034e22   a3 cc b0 48 30               MOV [0x3048b0cc],EAX | 30046734   8b 0d cc b0 48 30            MOV ECX,dword ptr [0x3048b0cc] | 30046876   a3 cc b0 48 30               MOV [0x3048b0cc],EAX
 */
vec3_t cg_weaponPositionBaseAngles = { 0.0f, 0.0f, 0.0f };
/* Source: uo_cgame_mp_x86.dll 0x3048b0d0 (.data); refs=3 width=4; first=0x30034e1d; owner=cg_addscalefade.
 * Example: 30034e1d   a3 d0 b0 48 30               MOV [0x3048b0d0],EAX | 30046747   8b 15 d0 b0 48 30            MOV EDX,dword ptr [0x3048b0d0] | 3004687f   89 0d d0 b0 48 30            MOV dword ptr [0x3048b0d0],ECX
 */
/* Source: uo_cgame_mp_x86.dll 0x3048b0d4 (.data); refs=3 width=4; first=0x30034e18; owner=cg_addscalefade.
 * Example: 30034e18   a3 d4 b0 48 30               MOV [0x3048b0d4],EAX | 3004673e   a1 d4 b0 48 30               MOV EAX,[0x3048b0d4] | 30046889   89 15 d4 b0 48 30            MOV dword ptr [0x3048b0d4],EDX
 */
/* Source: uo_cgame_mp_x86.dll 0x3048b0d8 (.data); refs=4 width=4; first=0x30034e31; owner=cg_addscalefade.
 * Example: 30034e31   a3 d8 b0 48 30               MOV [0x3048b0d8],EAX | 3004675a   8b 0d d8 b0 48 30            MOV ECX,dword ptr [0x3048b0d8] | 30046896   a3 d8 b0 48 30               MOV [0x3048b0d8],EAX
 */
/* Source: uo_cgame_mp_x86.dll 0x3048b0dc (.data); refs=3 width=4; first=0x30034e2c; owner=cg_addscalefade.
 * Example: 30034e2c   a3 dc b0 48 30               MOV [0x3048b0dc],EAX | 30046764   8b 15 dc b0 48 30            MOV EDX,dword ptr [0x3048b0dc] | 3004689b   89 0d dc b0 48 30            MOV dword ptr [0x3048b0dc],ECX
 */
/* Source: uo_cgame_mp_x86.dll 0x3048b0d8..0x3048b0e3 (.data). */
vec3_t cg_weaponRecoilAngles = { 0.0f, 0.0f, 0.0f };
/* cg_specialTagPlacement — supersedes the 13 mechanical
 * g_data_pm_beginreloadloop_3048b0{e4..110} scalars (the "pm_beginreloadloop" owner
 * was only the first toucher, not the subsystem). One fixed entity placement, laid
 * out as orientation_t across 0x3048b0e4..0x3048b113 (0x30 bytes):
 *   origin  @ 0x3048b0e4 (+0x00): 3001ff8e MOV EAX,[0x3048b0e4] / 3002ae05 reads;
 *                                 written at 300460d4/300460e1/300460bc.
 *   axis[0] @ 0x3048b0f0 (+0x0c): 0x3048b0f0/f4/f8.
 *   axis[1] @ 0x3048b0fc (+0x18): 0x3048b0fc/100/104.
 *   axis[2] @ 0x3048b108 (+0x24): 0x3048b108/10c/110.
 * Zero-initialized in the exported image; written as a unit by the tag/effect setup
 * around 0x300460b0.. and read as origin+axis by CG_GetEntityOriginAxis (0x3002adb0)
 * and 0x3001fec0. Provisional name by role. */
orientation_t cg_specialTagPlacement = {
    { 0.0f, 0.0f, 0.0f },
    { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } },
};
/* Source: uo_cgame_mp_x86.dll 0x3048b114 (.data); refs=5 width=4; first=0x3001bffb; owner=fire_lead.
 * Example: 3001bffb   a1 14 b1 48 30               MOV EAX,[0x3048b114] | 30034511   8b 0d 14 b1 48 30            MOV ECX,dword ptr [0x3048b114] | 30034d8c   89 15 14 b1 48 30            MOV dword ptr [0x3048b114],EDX
 */
uint32_t g_cgScreenReadyState = (uint32_t)0x00000000u;
/* Source: uo_cgame_mp_x86.dll 0x3048b118..0x3048b52c (.data).
 * Loader-zero, unreferenced tail of the cg view/effect aggregate before the
 * separately proven camera-shake array. */
uint8_t cg_unreferencedViewEffectState[1044];
/* Source: uo_cgame_mp_x86.dll 0x3048b52c..0x3048b5bc (.data): the fixed 4-slot
 * camera-shake table, cg_shakeSource_t cg_shakeSources[4] (stride 0x24). This
 * supersedes the mechanical per-dword captures the exporter made inside the
 * array: 0x3048b52c (slot0.startMsec, MOV ECX,0x3048b52c / LEA EDI,[EDI*4+base]),
 * 0x3048b548/0x3048b56c/0x3048b590/0x3048b5b4 (slots 0..3 scaledAmplitude, the
 * FLD sources in the weakest-slot min scan). All-zero .data initializer. Written
 * by CG_AddCameraShake (0x3001b420, REP MOVSD of a 0x24 stack copy), read by
 * CG_EvaluateCameraShakeSource (0x3001b390) and the aggregate walker (0x3001b550).
 */
cg_shakeSource_t cg_shakeSources[4] = {
    { 0, 0.0f, 0.0f, 0.0f, { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f },
    { 0, 0.0f, 0.0f, 0.0f, { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f },
    { 0, 0.0f, 0.0f, 0.0f, { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f },
    { 0, 0.0f, 0.0f, 0.0f, { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f },
};
/* Source: uo_cgame_mp_x86.dll 0x3048b5bc (.data); refs=5 width=4; first=0x3001b4ac.
 * cg_shakeSpinPhase — the random phase offset (radians) applied to the camera-shake
 * sway. NOT part of cg_shakeSources[4] (that array ends at 0x3048b5bc); the array's
 * end address is only used as the loop-end bound in CG_AddCameraShake (CMP at
 * 0x3001b4ac). Proven float: CG_CalcViewShake (0x3001b550) FSTPs a fresh random phase
 * in [-PI, PI) here on frames with no active shake (0x3001b606) and FADDs it as the
 * phase argument of the three FSIN sway terms (0x3001b6a5/6d0/6fb). Type repaired
 * uint32_t->float; mechanical owner=script_method_scriptbuiltin_viewki rejected. */
float cg_shakeSpinPhase = 0.0f;
/* Source: uo_cgame_mp_x86.dll 0x3048b5c0 (.data); refs=3 width=4; first=0x3001b5b1.
 * cg_shakeExternAmplitude — an externally-supplied shake amplitude (float) written by
 * 0x3001f901 (MOV [0x3048b5c0],EBP) and merged into the shake maximum by
 * CG_CalcViewShake: 0x3001b5b1 FLDs it and FCOMPs it against the running max of the
 * table's scaledAmplitude values, adopting it when larger. Type repaired
 * uint32_t->float; mechanical owner=item_slider_paint size-match rejected.
 * Example: 3001b5b1   d9 05 c0 b5 48 30            FLD float ptr [0x3048b5c0] | 3001b5c2   8b 15 c0 b5 48 30            MOV EDX,dword ptr [0x3048b5c0] | 3001f901   89 2d c0 b5 48 30            MOV dword ptr [0x3048b5c0],EBP
 */
float cg_shakeExternAmplitude = 0.0f;
/* Source: uo_cgame_mp_x86.dll 0x3048b5c4 (.data); refs=1 width=4; first=0x30039597; owner=cg_drawsoundoverlay.
 * Example: 30039597   89 35 c4 b5 48 30            MOV dword ptr [0x3048b5c4],ESI
 */
int32_t cg_shakeRestartLatch = 0;
/* Source: uo_cgame_mp_x86.dll 0x3048b5c8 (.data); refs=3 width=4; first=0x3001d391.
 * cg_hudSpinBaseTime — the spinning-HUD animation base time (float). Writers FSTP a
 * float here from atof of config string 11 (CG_ConfigString11Modified 0x3001d391 and
 * 0x3002e350); the sole consumer CG_UpdateHudSpinAngle FSUBs it (0x3001d3aa). Type
 * repaired uint32_t->float; mechanical owner=bg_getweapontypename size-match rejected. */
float cg_hudSpinBaseTime = 0.0f;
/* Source: uo_cgame_mp_x86.dll 0x3048b5cc (.data); refs=5 width=4; first=0x3001d412; owner=pm_beginweaponchange.
 * Example: 3001d412   a1 cc b5 48 30               MOV EAX,[0x3048b5cc] | 3001d68a   d9 1d cc b5 48 30            FSTP float ptr [0x3048b5cc] | 3001d69b   89 2d cc b5 48 30            MOV dword ptr [0x3048b5cc],EBP
 */
float cg_hudSpinAngle = 0.0f;
/* Source: uo_cgame_mp_x86.dll 0x3048b5d0 (.data); refs=29 width=4; first=0x3001d481.
 * cg_hudSpinVel — angular velocity (float, deg/s) of the spinning HUD element
 * (angle = cg_hudSpinAngle at 0x3048b5cc). All 29 refs live in CG_UpdateHudSpinAngle,
 * which integrates/damps/clamps it. Type repaired uint32_t->float (all x87). owner
 * label pm_beginweaponchange was a size-guess and is rejected.
 * Example: 3001d481   d9 05 d0 b5 48 30            FLD float ptr [0x3048b5d0] | 3001d49a   d9 05 d0 b5 48 30            FLD float ptr [0x3048b5d0] | 3001d514   d8 2d d0 b5 48 30            FSUBR float ptr [0x3048b5d0]
 */
float cg_hudSpinVel = 0.0f;
/* Source: uo_cgame_mp_x86.dll 0x3048b5d4 (.data); refs=10 width=4; first=0x300168c5; owner=cg_asset_parse.
 * Example: 300168c5   d8 25 d4 b5 48 30            FSUB float ptr [0x3048b5d4] | 30016942   d8 25 d4 b5 48 30            FSUB float ptr [0x3048b5d4] | 30016ee8   d8 25 d4 b5 48 30            FSUB float ptr [0x3048b5d4]
 */
float cg_compassRefYaw = 0.0f;
/* Source: uo_cgame_mp_x86.dll 0x3048b5d8 (.data); refs=18 width=4; first=0x3001b5a9; owner=item_slider_paint.
 * Example: 3001b5a9   81 ff d8 b5 48 30            CMP EDI,0x3048b5d8 | 3001d7ad   d9 05 d8 b5 48 30            FLD float ptr [0x3048b5d8] | 3001d7c6   d9 05 d8 b5 48 30            FLD float ptr [0x3048b5d8]
 */
float cg_compassRefVel = 0.0f;
/* Source: uo_cgame_mp_x86.dll 0x3048b5dc (.data); refs=3 width=4; first=0x30016617; owner=cg_asset_parse.
 * Example: 30016617   89 91 dc b5 48 30            MOV dword ptr [ECX + 0x3048b5dc],EDX | 30016693   89 8f dc b5 48 30            MOV dword ptr [EDI + 0x3048b5dc],ECX | 300167f2   be dc b5 48 30               MOV ESI,0x3048b5dc
 */
cgCompassBlip_t cg_compassFriendlies[CG_COMPASS_BLIP_COUNT] = { 0 };
/* Source: uo_cgame_mp_x86.dll 0x3048b5e0 (.data); refs=4 width=4; first=0x30016623; owner=cg_asset_parse.
 * Example: 30016623   89 91 e0 b5 48 30            MOV dword ptr [ECX + 0x3048b5e0],EDX | 3001674e   d9 9f e0 b5 48 30            FSTP float ptr [EDI + 0x3048b5e0] | 3001677b   89 97 e0 b5 48 30            MOV dword ptr [EDI + 0x3048b5e0],EDX
 */
/* Source: uo_cgame_mp_x86.dll 0x3048b5e4 (.data); refs=3 width=4; first=0x30016635; owner=cg_asset_parse.
 * Example: 30016635   89 91 e4 b5 48 30            MOV dword ptr [ECX + 0x3048b5e4],EDX | 3001675c   d9 9f e4 b5 48 30            FSTP float ptr [EDI + 0x3048b5e4] | 30016781   89 87 e4 b5 48 30            MOV dword ptr [EDI + 0x3048b5e4],EAX
 */
/* Source: uo_cgame_mp_x86.dll 0x3048b5e8 (.data); refs=3 width=4; first=0x3001663b; owner=cg_asset_parse.
 * Example: 3001663b   89 81 e8 b5 48 30            MOV dword ptr [ECX + 0x3048b5e8],EAX | 30016787   89 8f e8 b5 48 30            MOV dword ptr [EDI + 0x3048b5e8],ECX | 300167b0   d9 9f e8 b5 48 30            FSTP float ptr [EDI + 0x3048b5e8]
 */
/* Source: uo_cgame_mp_x86.dll 0x3048b5ec (.data); refs=4 width=4; first=0x30016648; owner=cg_asset_parse.
 * Example: 30016648   39 81 ec b5 48 30            CMP dword ptr [ECX + 0x3048b5ec],EAX | 30016655   89 81 ec b5 48 30            MOV dword ptr [ECX + 0x3048b5ec],EAX | 300167cd   39 87 ec b5 48 30            CMP dword ptr [EDI + 0x3048b5ec],EAX
 */
/* Source: uo_cgame_mp_x86.dll 0x3048badc (.data); refs=3 width=4; first=0x30016cbb; owner=cg_drawcompasstanks.
 * Example: 30016cbb   89 91 dc ba 48 30            MOV dword ptr [ECX + 0x3048badc],EDX | 30016d38   89 87 dc ba 48 30            MOV dword ptr [EDI + 0x3048badc],EAX | 30016e37   bd dc ba 48 30               MOV EBP,0x3048badc
 */
cgCompassBlip_t cg_compassTanks[CG_COMPASS_BLIP_COUNT] = { 0 };
/* Source: uo_cgame_mp_x86.dll 0x3048bae0 (.data); refs=5 width=4; first=0x30016bcd; owner=cg_asset_parse.
 * Example: 30016bcd   81 ff e0 ba 48 30            CMP EDI,0x3048bae0 | 30016cc7   89 91 e0 ba 48 30            MOV dword ptr [ECX + 0x3048bae0],EDX | 30016dbd   d9 9f e0 ba 48 30            FSTP float ptr [EDI + 0x3048bae0]
 */
/* Source: uo_cgame_mp_x86.dll 0x3048bae4 (.data); refs=3 width=4; first=0x30016cd3; owner=cg_drawcompasstanks.
 * Example: 30016cd3   89 91 e4 ba 48 30            MOV dword ptr [ECX + 0x3048bae4],EDX | 30016dcb   d9 9f e4 ba 48 30            FSTP float ptr [EDI + 0x3048bae4] | 30016df0   89 97 e4 ba 48 30            MOV dword ptr [EDI + 0x3048bae4],EDX
 */
/* Source: uo_cgame_mp_x86.dll 0x3048bae8 (.data); refs=4 width=4; first=0x30016cf0; owner=cg_drawcompasstanks.
 * Example: 30016cf0   89 81 e8 ba 48 30            MOV dword ptr [ECX + 0x3048bae8],EAX | 30016cfb   89 91 e8 ba 48 30            MOV dword ptr [ECX + 0x3048bae8],EDX | 30016df6   89 87 e8 ba 48 30            MOV dword ptr [EDI + 0x3048bae8],EAX
 */
/* Source: uo_cgame_mp_x86.dll 0x3048baec (.data); refs=1 width=4; first=0x30016ce2; owner=cg_drawcompasstanks.
 * Example: 30016ce2   89 91 ec ba 48 30            MOV dword ptr [ECX + 0x3048baec],EDX
 */
/* Source: uo_cgame_mp_x86.dll 0x3048bfdc (.data); refs=4 width=4; first=0x3001c468.
 * RESOLVED (on consume by CG_WeaponSway_ApplyShellShock, 0x30044c10): pointer to
 * the active shell-shock parameter record; its first field (+0x00) is the
 * shell-shock duration (ms) that the sway scaler divides the elapsed time by
 * (0x30044c3e MOV ECX,[0x3048bfdc]; 0x30044c4a MOV ECX,[ECX] -> duration). The
 * writer CG_DrawActiveFrame (0x300424e0) sets it either to the default params row
 * (0x30448624) or to an indexed entry of the shell-shock params table
 * (imul idx,0x7c; add 0x30448624). Grouped with cg_shellShockSwayStartTime /
 * cg_shellShockSwayDuration, which the same writer stores together. The mechanical
 * owner=g_getnonpvsfriendlyinfo label was only the first toucher, not the identity.
 * Example: 3004250d MOV [0x3048bfdc],EBX | 30044c4a MOV ECX,[ECX]. */
shellshock_t *cg_shellShockSwayParams = NULL;
/* Source: uo_cgame_mp_x86.dll 0x3048bfe0 (.data); refs=5 width=4; first=0x300171ec.
 * RESOLVED (on consume by CG_WeaponSway_ApplyShellShock, 0x30044c10): the
 * shell-shock sway start time (signed ms). The sway scaler forms the eased
 * envelope's remaining = cg_shellShockSwayDuration - cg.time + cg_shellShockSwayStartTime
 * (0x30044c21..0x30044c34). Written together with the params pointer / duration by
 * CG_DrawActiveFrame (0x300424e0). Mechanical owner=cg_drawcompasstanks was only a
 * first toucher; retyped uint32->int32 for the signed time arithmetic.
 * Example: 30042513 MOV [0x3048bfe0],EAX | 30044c27 MOV ESI,[0x3048bfe0]. */
int32_t cg_shellShockSwayStartTime = 0;
/* Source: uo_cgame_mp_x86.dll 0x3048bfe4 (.data); refs=3 width=4; first=0x3001c45d.
 * Active shellshock duration (signed ms). CG_WeaponSway_ApplyShellShock computes
 * startTime + duration - cg.time from this and cg_shellShockSwayStartTime. Written
 * together with the params pointer by CG_DrawActiveFrame (0x300424e0). Mechanical
 * owner=g_getnonpvsfriendlyinfo was only a first toucher; retyped uint32->int32.
 * Example: 3004253b MOV [0x3048bfe4],EDI | 30044c21 MOV EAX,[0x3048bfe4]. */
int32_t cg_shellShockSwayDuration = 0;
/* Source: uo_cgame_mp_x86.dll 0x3048bfe8 (.data); refs=5; first=0x3003c122.
 * Shellshock looping-sound end time (a signed millisecond timestamp; 0 = no
 * shellshock sound active). Mechanical uint32/owner=player_getmethod repaired: the
 * writer in CG_UpdateShellShockSound (0x3003c4ef) stores a computed future
 * cg_time-relative deadline (entity+0x64 - startTime + cg_time + duration), and
 * readers there compare cg_time against it (0x3003c4ac JGE) to decide whether the
 * sound has finished. CG_EndShellShockSound (0x3003c0f0) tests it for nonzero,
 * and when a sound is active registers "shellshock_end_abort" and plays it, then
 * clears this to 0. Retyped to int32_t for the signed time comparison. */
int32_t cg_shellshockSoundEndTime = 0;
/* Source: uo_cgame_mp_x86.dll 0x3048bfec (.data). Shellshock mouse-sensitivity
 * scale. Retyped uint32 -> float: all accesses are x87 (FLD/FSTP/FCOMP), and
 * the writers store the float bit patterns 0x3f800000 (1.0f) and computed floats,
 * never integers. Raw .data byte value is 0; the neutral 1.0f is written at
 * runtime by CG_EndShellShock (0x3003c1d0) / CG_EndShellShockMouse (0x3003c170).
 * Renamed from mechanical g_data_pm_lightlandingforsurface_3048bfec (wrong
 * owner/width) on consume by CG_EndShellShock.
 * Example: 3003c1f5 MOV dword ptr [0x3048bfec],0x3f800000 (=1.0f) | 3003c559 mouse update writes the interpolated sensitivity.
 */
float cg_shellshockMouseSensitivityScale = 0.0f;
/* Source: uo_cgame_mp_x86.dll 0x3048bff0 (.data). Shellshock screen-blur amount,
 * component X. Retyped uint32 -> float (x87 FLD/FUCOMP accesses). Raw .data value
 * 0. Renamed from mechanical g_data_pm_jump_3048bff0 on consume by CG_EndShellShock.
 * Example: 3003b679 FLD float ptr [0x3048bff0] | 3003c20b MOV dword ptr [0x3048bff0],0x0 (=0.0f).
 */
float cg_shellshockScreenBlurX = 0.0f;
/* Source: uo_cgame_mp_x86.dll 0x3048bff4 (.data). Shellshock screen-blur amount,
 * component Y. Retyped uint32 -> float (x87 FLD/FUCOMP accesses). Raw .data value
 * 0. Renamed from mechanical g_data_pm_jump_3048bff4 on consume by CG_EndShellShock.
 * Example: 3003b68e FLD float ptr [0x3048bff4] | 3003c215 MOV dword ptr [0x3048bff4],0x0 (=0.0f).
 */
float cg_shellshockScreenBlurY = 0.0f;
/* Source: uo_cgame_mp_x86.dll 0x3048bff8 (.data); refs=3 width=4; first=0x3003b833.
 * cg_fadeOverlayActive: CG_UpdateFadeOverlay's (0x3003b7e0) "drew overlay last frame"
 * latch; it is the only accessor of all 3 refs. Mechanical owner=itemparse_forecolor
 * label was a size-guess and is corrected here.
 * Example: 3003b833   a1 f8 bf 48 30               MOV EAX,[0x3048bff8] | 3003b858   a3 f8 bf 48 30               MOV [0x3048bff8],EAX | 3003b861   c7 05 f8 bf 48 30 00 00 00 00 MOV dword ptr [0x3048bff8],0x0
 */
int32_t cg_fadeOverlayActive = 0;
/* Source: uo_cgame_mp_x86.dll 0x3048bffc (.data); width=4.
 * cg_shellShockStartTime: cg.time (ms) captured when "cg_shellshock" triggers.
 * Written by CG_ShellShock_f (0x30017572) and read by scene reader 0x30042160
 * (0x300424fd). The mechanical owner=concatargs label was a wrong size-guess
 * name for 0x300174b0. */
int32_t cg_shellShockStartTime = 0;
/* Source: uo_cgame_mp_x86.dll 0x3048c000 (.data); width=4.
 * cg_shellShockDuration: shellshock duration (ms) for "cg_shellshock". Written
 * by CG_ShellShock_f (0x30017577) and read by scene reader 0x30042160
 * (0x30042502). The mechanical owner=concatargs label was a wrong size-guess
 * name for 0x300174b0. */
int32_t cg_shellShockDuration = 0;
/* Source: uo_cgame_mp_x86.dll 0x3048c004 (.data) — cg_adsViewErrorLatched, the
 * one-shot latch guarding the ADS view-error step in CG_UpdateAdsViewError
 * (0x30036070): the step runs only while this is 0, then sets it to 1 (0x30036110);
 * a frame that fails the scope/overlay gate clears it to 0 (0x300361b8), re-arming
 * the next activation. Also cleared by the cgame state reset at 0x30034e4e (MOV
 * [0x3048c004],EDX with EDX==0). Modeled int32 boolean; the owner=cg_addscalefade
 * label was the first toucher, not the identity.
 */
int32_t cg_adsViewErrorLatched = 0;
/* 0x3048c030 RESOLVED: cg_crosshairNoGun_vmCvar.string (+0x10 of the vmCvar at
 * 0x3048c020); no separate object exists at this address. See globals.h. */

/* Source: uo_cgame_mp_x86.dll 0x3048c14c (.data); refs=1 width=4; first=0x3002b370; owner=trap_syscall_3.
 * Example: 3002b370   a1 4c c1 48 30               MOV EAX,[0x3048c14c]
 */

/* Source: uo_cgame_mp_x86.dll 0x3048c268 (.data); refs=2 width=4; first=0x3002ab1d; owner=pm_switchifempty.
 * Example: 3002ab1d   8b 0d 68 c2 48 30            MOV ECX,dword ptr [0x3048c268] | 30048a38   8b 15 68 c2 48 30            MOV EDX,dword ptr [0x3048c268]
 */
/* Repaired uint32_t->float: read as `float ptr` (moving-tracer width, mode B).
 * Zero-initialized in .bss (set at init by an unseen writer). */

/* Source: uo_cgame_mp_x86.dll 0x3048c388 (.data); refs=1 width=4; first=0x30048541; owner=veh_updateclient.
 * Example: 30048541   d9 05 88 c3 48 30            FLD float ptr [0x3048c388]
 */

/* Source: uo_cgame_mp_x86.dll 0x3048c458 (.data); refs=1 width=4; first=0x30048eda; owner=registeritem.
 * Example: 30048eda   8d ad 58 c4 48 30            LEA EBP,[EBP + 0x3048c458]
 */

/* Source: uo_cgame_mp_x86.dll 0x3048c4a8 (.data); refs=31 width=4; first=0x300167e5.
 * RESOLVED (role): cg_hudCompassSize_vmCvar.value — a float animation fraction, read-only in the
 * exported code (FLD/FMUL only). The tag/render family and weapon-select draws slide a
 * screen element with `(cg_hudCompassSize_vmCvar.value - 1.0f)*112.0f + rect.x` (0x30031cf1,
 * 0x30031db4, 0x30031ecc, 0x30031feb, 0x3002fe82) and FMUL by it as a scale elsewhere
 * (0x30016a7e, 0x3002f91b, 0x300302db). Set by the asset/init path (owner=cg_asset_parse,
 * its first toucher; no writer in the exported set). Exact CoD source name unproven.
 * Example: 300167e5   d9 05 a8 c4 48 30            FLD float ptr [0x3048c4a8] | 30016814   d9 05 a8 c4 48 30            FLD float ptr [0x3048c4a8] | 30016826   d9 05 a8 c4 48 30            FLD float ptr [0x3048c4a8]
 */

/* Source: uo_cgame_mp_x86.dll 0x3048c5cc (.data); refs=1 (0x3002cb21) width=4.
 * Sole consumer CG_PlaySoundAliasByName (0x3002ca80) loads this dword and passes it as
 * the final argument of cgame_syscall(CG_SUBTITLE, ...). Zero in the image; no .text
 * writer, so engine/externally-initialized. Provisional role name (was mechanical
 * g_data_script_addlistitem_3048c5cc; the script_addlistitem owner was a wrong
 * size-based guess rejected during CG_PlaySoundAliasByName reconstruction).
 * 3002cb21   8b 15 cc c5 48 30            MOV EDX,dword ptr [0x3048c5cc] */

/* Source: uo_cgame_mp_x86.dll 0x3048c6e0 (.data).
 * cg_entities — MAX_GENTITIES records, stride 0x288. The typed element layout and
 * stride are proven in client_recovered.h; machine-code users index this base with
 * IMUL index,0x288 before accessing centity fields. */
centity_t cg_entities[MAX_GENTITIES];
/* 0x3048c6f8 is cg_entities[0].currentStatePos.trBase (+0x18), not a second global.
 * All five users multiply the entity index by 0x288 before adding this address,
 * proving the pointer is &cg_entities[index].currentStatePos.trBase. */
/* 0x3048c768 is the low byte of cg_entities[index].stateFilter (+0x88):
 * 30047ea4 indexes the array with the already-proven 0x288 centity stride. */
/* 0x3048c8c8 is cg_entities[0].currentValid (+0x1e8), not separate storage. Every
 * indexed use has the cg_entities 0x288-byte stride. */
/* Source: uo_cgame_mp_x86.dll 0x3052e6ec (.data); refs=1 width=4; first=0x30035a7e; owner=pmovesingle.
 * Example: 30035a7e   a1 ec e6 52 30               MOV EAX,[0x3052e6ec]
 */

/* Source: uo_cgame_mp_x86.dll 0x3052e808 (.data); refs=1 width=4; first=0x300187e9; owner=reached_binarymover.
 * Example: 300187e9   d8 2d 08 e8 52 30            FSUBR float ptr [0x3052e808]
 */

/* Source: uo_cgame_mp_x86.dll 0x3052e80c (.data); refs=2 width=4; owner=reached_binarymover.
 * cg_chatTime_vmCvar.integer — cvar-integer snapshot (ms a team-chat line stays visible).
 * CG_AddToTeamChat flushes the ring when <=0 (signed JLE); the team-info drawer
 * uses it as the fade window. */

/* Source: uo_cgame_mp_x86.dll 0x3052e8c8 (.data); refs=2 width=imm; first=0x3003c9fb; owner=debugdumpanims.
 * Example: 3003c9fb   3d c8 e8 52 30               CMP EAX,0x3052e8c8 | 3003d36b   3d c8 e8 52 30               CMP EAX,0x3052e8c8
 */

/* Source: uo_cgame_mp_x86.dll 0x3052ea4c (.data) — cg_hudDamageIconTime_vmCvar.integer:
 * the display lifetime (ms) copied into cg_damageDirIndicators[best].duration when
 * CG_DamageFeedback (0x30034c8c) registers a new damage-direction arrow. Read-only
 * here (refs=1); set elsewhere (config/tunable). owner=cg_addpacketentities was a
 * size-match artifact; renamed by proven role.
 * Example: 30034c8c   8b 0d 4c ea 52 30            MOV ECX,dword ptr [0x3052ea4c]
 */

/* Source: uo_cgame_mp_x86.dll 0x3052ec8c (.data); refs=1 width=4; first=0x30048f2d; owner=registeritem.
 * Example: 30048f2d   8b 0d 8c ec 52 30            MOV ECX,dword ptr [0x3052ec8c]
 */
/* Source: uo_cgame_mp_x86.dll 0x3052ec8c. */

/* Source: uo_cgame_mp_x86.dll 0x3052edac (.data); refs=2 width=4; first=0x3003a2da; owner=playercmd_cloneplayer.
 * Example: 3003a2da   a1 ac ed 52 30               MOV EAX,[0x3052edac] | 3003adc1   a1 ac ed 52 30               MOV EAX,[0x3052edac]
 */
/* 0x3052edac; repaired mechanical name */
/* Source: uo_cgame_mp_x86.dll 0x3052eec0..0x3052efd0 (.data); the "cg_viewsize"
 * vmCvar_t mirror. base 0x3052eec0 pushed as the vmCvar handle by CG_CalcVrect
 * (3003f538/3003f55b PUSH 0x3052eec0); its .integer field at +0xc == 0x3052eecc
 * is read there (3003f528 MOV ESI,[0x3052eecc]) as the clamp input. The two former
 * mechanical symbols _3052eec0 and _3052eecc were the same struct split across two
 * addresses; merged into one vmCvar_t. Zero-initialized in the image (the engine
 * fills it at trap_Cvar_Register). See globals.h for the resolved name/rationale.
 */
vmCvar_t cg_viewSizeCvar;
/* Source: uo_cgame_mp_x86.dll 0x3052efec (.data); refs=3 width=4; first=0x30003f6f; owner=bg_evaluatetrajectory.
 * Example: 30003f6f   83 3d ec ef 52 30 01         CMP dword ptr [0x3052efec],0x1 | 3000421f   83 3d ec ef 52 30 02         CMP dword ptr [0x3052efec],0x2 | 30034323   83 3d ec ef 52 30 05         CMP dword ptr [0x3052efec],0x5
 */
/* 0x3052efec: supersedes g_data_bg_evaluatetrajectory_3052efec */
/* Source: uo_cgame_mp_x86.dll 0x3052f10c (.data); refs=7 width=4; first=0x30035905; owner=pmovesingle.
 * Example: 30035905   a1 0c f1 52 30               MOV EAX,[0x3052f10c] | 30035b60   a1 0c f1 52 30               MOV EAX,[0x3052f10c] | 30035c14   a1 0c f1 52 30               MOV EAX,[0x3052f10c]
 */
/* 0x3052f10c: cgame developer/debug-print gate (see globals.h). */
/* Source: uo_cgame_mp_x86.dll 0x3052f34c (.data); refs=1 width=4; first=0x30035680.
 * Example: 30035680   a1 4c f3 52 30               MOV EAX,[0x3052f34c]
 * RESOLVED (behavioral): cg_predictItems_vmCvar.integer — cached integer value of the
 * cg_predictItems_vmCvar.integer cvar; CG_TouchItem (0x30035680) early-returns when it is 0.
 * owner=axiscopy was a size-guess mislabel. */

/* Source: uo_cgame_mp_x86.dll 0x3052f6ac (.data); refs=1 width=4; first=0x3001bd53; owner=pm_airmove (mislabel).
 * Example: 3001bd53   a1 ac f6 52 30               MOV EAX,[0x3052f6ac]
 * RESOLVED: cg_descriptiveText_vmCvar.integer — boolean enable gate for the spectator
 * follow-mode key-hint HUD; the sole consumer CG_DrawSpectatorFollowHints
 * (0x3001bd50) reads it as `if (!cg_descriptiveText_vmCvar.integer) return;`.
 */

/* Source: uo_cgame_mp_x86.dll 0x3052f7c8 (.data); refs=2 width=4; first=0x3003ffef; owner=bg_parseweaponinfospecificfieldtyp.
 * Example: 3003ffef   8b 0d c8 f7 52 30            MOV ECX,dword ptr [0x3052f7c8] | 30040352   d8 35 c8 f7 52 30            FDIV float ptr [0x3052f7c8]
 */
/* 0x3052f7c8; see globals.h. Base FOV degrees (cg_fov cvar mirror). */
/* Source: uo_cgame_mp_x86.dll 0x3052f8ec (.data); refs=2 width=4; first=0x3001b07c; owner=colorbytes3 (mechanical owner wrong).
 * Resolved on consume by Com_DPrintf (0x3002b470): developer-mode gate, the cached
 * integer of the Quake3 `developer` cvar. Both readers test it as a boolean and
 * skip developer-only work when zero. See globals.h for the full note.
 * Example: 3001b07c   8b 0d ec f8 52 30            MOV ECX,dword ptr [0x3052f8ec] | 3002b482   a1 ec f8 52 30               MOV EAX,[0x3052f8ec]
 */

/* Source: uo_cgame_mp_x86.dll 0x3052fb28 (.data); refs=1 width=4; first=0x30044e31; owner=setclientviewangle.
 * Example: 30044e31   d9 05 28 fb 52 30            FLD float ptr [0x3052fb28]
 */

/* Source: uo_cgame_mp_x86.dll 0x3052fc4c (.data); refs=1 width=4; first=0x3001a5ce.
 * Example: 3001a5ce   8b 15 4c fc 52 30            MOV EDX,dword ptr [0x3052fc4c]
 */

/* Source: uo_cgame_mp_x86.dll 0x3052ffa8 (.data); refs=2 (0x3002cac7, 0x3002caf3) width=4.
 * Sole consumer CG_PlaySoundAliasByName (0x3002ca80) reads it with FLD float ptr and
 * scales by 1000.0f into a millisecond time. Retyped uint32_t -> float to match the
 * float access. Zero in the image; no .text writer, so engine/externally-set.
 * Provisional role name (was mechanical g_data_script_addlistitem_3052ffa8; the
 * script_addlistitem owner was a wrong size-based guess).
 * 3002cac7   d9 05 a8 ff 52 30            FLD float ptr [0x3052ffa8] */

/* Source: uo_cgame_mp_x86.dll 0x305300cc (.data); refs=4 width=4; first=0x30019bd0; owner=pm_viewheighttablelerp.
 * Example: 30019bd0   a1 cc 00 53 30               MOV EAX,[0x305300cc] | 30019eb2   a1 cc 00 53 30               MOV EAX,[0x305300cc] | 3001fb3f   39 2d cc 00 53 30            CMP dword ptr [0x305300cc],EBP
 */

/* Source: uo_cgame_mp_x86.dll 0x305301ec (.data); refs=3 width=4; first=0x300303b2; owner=pm_footsteps.
 * Example: 300303b2   a1 ec 01 53 30               MOV EAX,[0x305301ec] | 3003044b   a1 ec 01 53 30               MOV EAX,[0x305301ec] | 30030475   a1 ec 01 53 30               MOV EAX,[0x305301ec]
 */
/* 0x305301ec */
/* Source: uo_cgame_mp_x86.dll 0x30530308 (.data); refs=1 width=4; first=0x3003f798; owner=scr_vehicle_damagescale.
 * Example: 3003f798   d8 25 08 03 53 30            FSUB float ptr [0x30530308]
 */

/* Source: uo_cgame_mp_x86.dll 0x30530428 (.data); refs=1 width=4; first=0x30044eb5; owner=setclientviewangle.
 * Example: 30044eb5   d9 05 28 04 53 30            FLD float ptr [0x30530428]
 */

/* Source: uo_cgame_mp_x86.dll 0x30530668 (.data); refs=3 width=4; first=0x3002ab23; owner=pm_switchifempty.
 * Example: 3002ab23   d9 05 68 06 53 30            FLD float ptr [0x30530668] | 300482f1   8b 0d 68 06 53 30            MOV ECX,dword ptr [0x30530668] | 30048a3e   d9 05 68 06 53 30            FLD float ptr [0x30530668]
 */
/* Repaired uint32_t->float: read as `float ptr` (moving-tracer length, mode B).
 * Zero-initialized in .bss (set at init by an unseen writer). */

/* Source: uo_cgame_mp_x86.dll 0x30530780 (.data); refs=2 width=4; first=0x3002a9e8.
 * cg_localEntities: the fixed pool of MAX_LOCAL_ENTITIES (128) localEntity_t (stride
 * 0xec == 236 bytes, spanning 0x30530780..0x30537d80). CG_InitLocalEntities (0x3002a9e0)
 * zeroes the whole pool (rep stosd 0x1d80 dwords = 30208 = 128*236) and chains the free
 * list through ->next. The mechanical export owner=anglenormalize180accurate was a size
 * guess (this is not an angle helper); it also split two interior element addresses into
 * their own symbols: 0x30530784 == &cg_localEntities[0].next (was
 * g_data_..._30530784) and 0x30537c98 == &cg_localEntities[127].next (was
 * g_data_..._30537c98). All three are fields inside this one array and are unified here. */
localEntity_t cg_localEntities[MAX_LOCAL_ENTITIES];
/* Source: uo_cgame_mp_x86.dll 0x30537d80 (.data); refs=8 width=4; first=0x3002a9fe.
 * cg_freeLocalEntities: head of the localEntity_t free list (chained via ->next).
 * CG_InitLocalEntities sets it to &cg_localEntities[0]; consumed by
 * CG_AllocLocalEntity / CG_FreeLocalEntity. (owner label was wrong.) */
struct localEntity_s *cg_freeLocalEntities = 0;
/* Source: uo_cgame_mp_x86.dll 0x30537da0 (.data); refs=7+4 width=4; first=0x3002a9ef.
 * cg_activeLocalEntities: sentinel of the circular doubly-linked active list.
 * .prev at +0x0 (0x30537da0), .next at +0x4 (0x30537da4); CG_InitLocalEntities
 * self-links both at runtime. Unifies the mechanically-split symbols that were at
 * 0x30537da0 (was g_data_..._30537da0) and 0x30537da4 (was g_data_..._30537da4). */
localEntity_t cg_activeLocalEntities;
/* Source: uo_cgame_mp_x86.dll 0x30537ea0 (.data); refs=2 width=4; first=0x30018c9e; owner=cmd_veh_fireturret.
 * Example: 30018c9e   db 04 8d a0 7e 53 30         FILD dword ptr [ECX*0x4 + 0x30537ea0] | 300421ee   89 04 95 a0 7e 53 30         MOV dword ptr [EDX*0x4 + 0x30537ea0],EAX
 */
int32_t cg_lagometerFrameSamples[LAG_SAMPLES] = { 0 };
/* Source: uo_cgame_mp_x86.dll 0x305380a0 (.data); refs=3 width=4; first=0x30018c92; owner=cmd_veh_fireturret.
 * Example: 30018c92   8b 0d a0 80 53 30            MOV ECX,dword ptr [0x305380a0] | 300421df   8b 15 a0 80 53 30            MOV EDX,dword ptr [0x305380a0] | 300421f5   ff 05 a0 80 53 30            INC dword ptr [0x305380a0]
 */
int32_t cg_lagometerFrameCount = 0;
/* Source: uo_cgame_mp_x86.dll 0x305380a4 (.data): cg_lagometer — the network-lag graph
 * ring (see lagometer_t in globals.h). The three former mechanical symbols were three
 * fields of this one object: snapshotFlags at 0x305380a4, snapshotSamples at 0x305382a4
 * (+0x200), snapshotCount at 0x305384a4 (+0x400). Retyped/unified from the three
 * uint32_t symbols (owner=vector4scale was the first-touching function, not the identity).
 * Example: 30018a4c   c7 04 85 a4 82 53 30 ff ff ff ff MOV dword ptr [EAX*0x4 + 0x305382a4],0xffffffff | 30018a57   ff 05 a4 84 53 30            INC dword ptr [0x305384a4] | 30018e28   f6 04 8d a4 80 53 30 01      TEST byte ptr [ECX*0x4 + 0x305380a4],0x1
 */
lagometer_t cg_lagometer = { { 0 }, { 0 }, 0 };
/* Source: uo_cgame_mp_x86.dll 0x305384c0 (.data); refs=3 width=4; first=0x30030f3c.
 * RESOLVED: cg_hudEmitClientTable[] — index->clientNum table (see globals.h). Sized to
 * the 8-dword span before the next distinct symbol at 0x305384e0; exact extent unproven.
 * Example: 30030f3c   8b 04 85 c0 84 53 30         MOV EAX,dword ptr [EAX*0x4 + 0x305384c0] | 3003103b   8b 04 85 c0 84 53 30         MOV EAX,dword ptr [EAX*0x4 + 0x305384c0] | 3003120b   8b 04 85 c0 84 53 30         MOV EAX,dword ptr [EAX*0x4 + 0x305384c0]
 */
int32_t cg_hudEmitClientTable[8] = {0};
/* Source: uo_cgame_mp_x86.dll 0x305384e0..0x305385e0 (.data).
 * Static 256-byte trap-54 output/scratch buffer passed by address at 0x300319c8. */
char cg_hudEmitScratch[CG_HUD_STRING_BUFFER_SIZE] = {0};
/* Source: uo_cgame_mp_x86.dll 0x305385e0 (.data); refs=3 width=4; first=0x30030f2d.
 * RESOLVED: cg_hudEmitCount — active-entry count / cursor upper bound (see globals.h).
 * Example: 30030f2d   3b 05 e0 85 53 30            CMP EAX,dword ptr [0x305385e0] | 3003102c   3b 05 e0 85 53 30            CMP EAX,dword ptr [0x305385e0] | 300311fc   3b 05 e0 85 53 30            CMP EAX,dword ptr [0x305385e0]
 */
int32_t cg_hudEmitCount = 0;
/* 0x30538600 is cg_trapStringBufferA[256] (defined near the top of this file);
 * the mechanical uint32_t alias for this address was removed as a duplicate. */
/* 0x30538700 is cg_trapStringBufferB[0], passed by address at 0x30031a28; the
 * mechanical bg_playanimname dword alias was a false first-touch name. */
/* Source: uo_cgame_mp_x86.dll 0x3053880c (.data); refs=1 width=4; read-only dev-print
 * gate for PM_Weapon_PrintWeaponState. Renamed from the size-guess mechanical
 * owner=calcmuzzlepoint (the real owner 0x30014710 is PM_Weapon). See globals.h.
 * Example: 3001479f   a1 0c 88 53 30               MOV EAX,[0x3053880c]
 */

/* Source: uo_cgame_mp_x86.dll 0x3053892c (.data); refs=1 width=4; read-only dev-print
 * gate for PM_Weapon_PrintWeaponAnim. Renamed from the same size-guess
 * mechanical owner=calcmuzzlepoint. See globals.h.
 * Example: 300147ba   a1 2c 89 53 30               MOV EAX,[0x3053892c]
 */

/* Source: uo_cgame_mp_x86.dll 0x30538a48 (.data): cg_bobMax_vmCvar.value. Its registered cvar
 * name is the literal "cg_bobMax_vmCvar.value" at 0x30078be4; the value is consumed as a float
 * maximum-amplitude argument by both view bob-factor calls at 0x3003fd64/0x3003fd85. */

/* Source: uo_cgame_mp_x86.dll 0x30538b68 (.data); refs=5 width=4; first=0x30014ddf; owner=vector5add.
 * cg_bobAmplitudeProne_vmCvar.value — prone-stance bob amplitude scale (see globals.h). Retyped
 * to float, superseding the mechanical uint32_t. Zero-initialized in .data; set at
 * runtime. Consumed by BG_GetVerticalBobFactor (0x30014dd0) and the bob family.
 */

/* Source: uo_cgame_mp_x86.dll 0x30538c88 (.data); refs=5 width=4; first=0x30014def; owner=vector5add.
 * cg_bobAmplitudeDucked_vmCvar.value — crouched-stance bob amplitude scale (see globals.h).
 */

/* Source: uo_cgame_mp_x86.dll 0x30538da8 (.data); refs=5 width=4; first=0x30014df7; owner=vector5add.
 * cg_bobAmplitudeStanding_vmCvar.value — standing (default) stance bob amplitude scale (see globals.h).
 */

/* Source: uo_cgame_mp_x86.dll 0x30538ecc (.data); refs=1 width=4; first=0x3000ffe5; owner=pm_setmovementdir.
 * Example: 3000ffe5   a1 cc 8e 53 30               MOV EAX,[0x30538ecc]
 */

/* Source: uo_cgame_mp_x86.dll 0x30538fec (.data); refs=1 width=4; first=0x3000cde9; owner=veh_findvaliddismountspot.
 * Example: 3000cde9   db 05 ec 8f 53 30            FILD dword ptr [0x30538fec]
 */

/* Source: uo_cgame_mp_x86.dll 0x3053910c (.data). bg_foliagesnd_minspeed.integer — horizontal-speed
 * floor (units/s, signed int) below which PM_FoliageSounds (0x3000c110) plays no foliage
 * sound and above which it interpolates the sound interval. Never written in this DLL
 * (cvar/config owned). See globals.h.
 * Example: 3000c110   db 05 0c 91 53 30            FILD dword ptr [0x3053910c] | 3000c155   a1 0c 91 53 30               MOV EAX,[0x3053910c]
 */

/* Source: uo_cgame_mp_x86.dll 0x3053922c (.data). bg_viewheight_prone_vmCvar.integer — prone-stance
 * muzzle view height; sole reader CG_CalcMuzzlePoint (FILD [0x3053922c]). See globals.h. */

/* Source: uo_cgame_mp_x86.dll 0x3053934c (.data). bg_foliagesnd_fastinterval.integer — foliage-sound
 * repeat interval (ms, signed int) at/above the speed ceiling; upper end of the
 * speed->interval lerp in PM_FoliageSounds (0x3000c110). Never written in this DLL
 * (cvar/config owned). See globals.h.
 * Example: 3000c185   a1 4c 93 53 30               MOV EAX,[0x3053934c]
 */

/* Source: uo_cgame_mp_x86.dll 0x30539468 (.data). Upper endpoint of the
 * PM_CrashLand fall-height-to-damage envelope. */

/* Source: uo_cgame_mp_x86.dll 0x30539580..0x3053958b (.data); refs=16/11/8 width=4;
 * first=0x30008da1; mechanical owner=cg_drawcrosshairnames (first toucher, not identity).
 * pml.forward: the pmove locals' horizontal forward basis vector (pml.forward). The three
 * contiguous dwords 0x30539580/84/88 are one vec3_t: built from the view angles (fstp'd as a
 * triple at 0x30009364/78/88), z-flattened and normalized by PM_AirMove (0x30009060: MOVs
 * $0x0->[0x30539588], VectorNormalize with ESI=0x30539580), then read component-wise to form
 * the movement wish-velocity. Superseded from three separate mechanical uint32 dwords to one
 * float vector because the machine code proves the 12-byte float-vector shape.
 * Example: 30008dbe   d9 05 80 95 53 30            FLD float ptr [0x30539580] | 30009364 FSTP [0x30539580] | 300090ce MOV [0x30539588],0
 */
/* The corresponding pml source object is shared in bg_pmove_state.c. */
/* Source: uo_cgame_mp_x86.dll 0x3053958c..0x30539597 (.data); refs=16/12/9 width=4;
 * first=0x30008f95; mechanical owner=lookatkiller (first toucher, not identity).
 * pml.right: the pmove locals' horizontal right basis vector (pml.right). The three contiguous
 * dwords 0x3053958c/90/94 are one vec3_t, the sibling of pml.forward above: z-flattened
 * (MOV $0x0->[0x30539594]) and normalized (VectorNormalize with ESI=0x3053958c) by PM_AirMove,
 * then combined with pml.forward to build the wish-velocity. Superseded to a float vector.
 * Example: 300090f5   d9 05 8c 95 53 30            FLD float ptr [0x3053958c] | 300093db FSTP [0x3053958c] | 300090d8 MOV [0x30539594],0
 */
/* Source: uo_cgame_mp_x86.dll 0x30539598..0x305395a3 (.data).
 * pml.up (pml.up): pmove locals' up basis vector, the third AngleVectors output.
 * PmoveSingle passes &pml.up in EBX to AngleVectors (0x3000e38f/e39e). Superseded to
 * a float vector from the mechanical single-dword (owner label was wrong). */
/* Source: uo_cgame_mp_x86.dll 0x305395a4 (.data); refs=33 width=4; first=0x3000854e.
 * pml.frametime (pml.frametime): pmove frame time in seconds. Written by PmoveSingle
 * setup at 0x3000e2f9..0x3000e345 as (float)pml.msec * 0.001f (FILD [0x305395a8];
 * FMUL [0x3007bd94]=0.001; FSTP [0x305395a4]) and read as a float throughout the
 * pmove physics (e.g. 30008625 FMUL [0x305395a4] in PM_Accelerate). The mechanical
 * owner=vectoangles label is only the first toucher, not the datum's identity.
 * Example: 30008625   d8 0d a4 95 53 30   FMUL float ptr [0x305395a4]
 */
/* Source: uo_cgame_mp_x86.dll 0x305395a8 (.data); refs=21 width=4; first=0x3000bd05.
 * pml.msec: the current pmove frame time delta in milliseconds. Set in PmoveSingle
 * setup (0x3000e2ce) as msec = to->serverTime - from->serverTime, then clamped to
 * >=1 and <=200 (0xc8). The mechanical owner=window_paint label was merely the
 * first function to touch the datum and is NOT its identity. Consumed as a signed
 * int by the timer-drop code (PM_DropTimers, 0x3000c320: SUB/CMP against it) and
 * FILD'd elsewhere for float lerp math. Signed int32_t.
 */
/* Source: uo_cgame_mp_x86.dll 0x305395ac (.data); refs=16 width=4; first=0x30008470; owner=vectoangles.
 * pml.walking: qboolean flag for whether the pmove locals consider the player to be
 * walking on a walkable ground plane this frame (pml.walking). Resolved by consumer:
 * PM_GroundTrace (0x3000a2a0) clears it to 0 together with pml.groundPlane (0x305395b0)
 * and sets the playerState groundEntityNum to ENTITYNUM_NONE on the ground-miss path
 * (the exact Quake3 PM_GroundTraceMissed epilogue: groundEntityNum=NONE, groundPlane=false,
 * walking=false, at 0x3000a3bc/0x3000a3d1). Superseded from the mechanical uint32/owner
 * label (owner=vectoangles was a first-touch artifact, not the identity).
 * Example: 30008470   8b 0d ac 95 53 30            MOV ECX,dword ptr [0x305395ac] | 30008c83   89 15 ac 95 53 30            MOV dword ptr [0x305395ac],EDX | 30009661   a1 ac 95 53 30               MOV EAX,[0x305395ac]
 */
/* Source: uo_cgame_mp_x86.dll 0x305395b0 (.data); refs=18 width=4; first=0x30008c77;
 * mechanical owner=scr_getgametypenameforscript (first toucher, not identity).
 * pml.groundPlane: qboolean flag for whether the pmove locals hold a valid ground trace
 * plane this frame (pml.groundPlane). PM_Jump (0x30008c70) clears it to 0 on take-off
 * (30008c77 MOV [0x305395b0],EDX with EDX=0); PM_AirMove (0x30009060) gates its slope
 * clip on it (30009155 MOV EAX,[0x305395b0]; TEST EAX,EAX; JZ skip -> only clip velocity
 * against pml.groundTrace.normal when a ground plane exists). Superseded from uint32 to int32_t
 * (qboolean). NOTE for adjudication: the ground-trace setup cluster near 0x3000a2xx and the
 * PM_GroundTrace-family at 0x3000d49a.. also write/read this address; those functions are not
 * yet reconstructed, so the exact set/producer semantics beyond the on-ground flag are
 * unverified here.
 * Example: 30008c77 MOV [0x305395b0],EDX(=0) | 30009155 MOV EAX,[0x305395b0]; TEST EAX,EAX
 */
/* Source: uo_cgame_mp_x86.dll 0x305395b4 (.data); refs=13 width=4; first=0x30008c7d;
 * mechanical owner=scr_getgametypenameforscript (first toucher, not identity).
 * pml.groundLiftFlag: pmove-locals flag written alongside pml.groundPlane/pml.walking by the
 * ground-trace cluster. PM_GroundTrace (0x3000a2a0) sets it to 1 when the vertical probe trace
 * detects a near/flat lift-style contact (fraction below the 0.015625f snap threshold on the
 * on-ground path, or a sub-1.0 fraction one unit below on the airborne path) and clears it to 0
 * otherwise. The exact original pml field name is unresolved (Quake3 pml has no single obvious
 * counterpart), so it is named provisionally by its proven "vertical lift/near-ground contact"
 * role. Superseded from the mechanical uint32/owner label.
 * Example: 30008c7d   89 15 b4 95 53 30            MOV dword ptr [0x305395b4],EDX | 3000a1d6   89 15 b4 95 53 30            MOV dword ptr [0x305395b4],EDX | 3000a3b6   89 1d b4 95 53 30            MOV dword ptr [0x305395b4],EBX
 */
/* Source: uo_cgame_mp_x86.dll 0x305395b8 (.data); refs=3 width=48; first=0x3000a26e.
 * pml.groundTrace: the pmove locals' cached ground-trace result (pml.groundTrace, a
 * 48-byte trace_t). All three writers copy a whole trace_t here with `rep movsd` of
 * 0xc dwords (48 bytes): PM_CorrectAllSolid (0x3000a26e), and the two ground-trace
 * callers at 0x3000a50e / 0x3000a5ae. The trace's normal sub-field at +0x10 lands
 * at 0x305395c8 and surfaceFlags at +0x1c lands at 0x305395d4. Consumers access
 * those members directly; no overlapping globals or compatibility aliases exist.
 * (owner=display_mousemove was a wrong first-touch/size guess.)
 * Example: 3000a26e   bf b8 95 53 30               MOV EDI,0x305395b8 (rep movsd, ECX=0xc)
 */
/* Source: uo_cgame_mp_x86.dll 0x305395e8 (.data); refs=2 width=4; first=0x3000ece4.
 * pml.maxClipImpact (pml.maxClipImpact): running high-watermark of the largest
 * negated velocity-into-plane dot magnitude PM_SlideMove has seen this run. In the
 * two-plane clip PM_SlideMove computes the (negated) into-plane speed, and when it
 * exceeds this latch it stores the new maximum here (0x3000ece4 FCOMP; 0x3000ecf5
 * MOV writes the bit pattern of the float back). A debug/telemetry scratch float;
 * PmoveSingle (0x3000e050) resets it to 0 at the start of each pmove run. Supersedes the mechanical
 * g_data_pm_stepslidemove_305395e8 (owner=pm_stepslidemove was a first-touch/size
 * artifact; the real consumer is PM_SlideMove at 0x3000e930). Matches the server
 * pml_s::maxClipImpact (DAT_00202408). */
/* Source: uo_cgame_mp_x86.dll 0x305395ec..0x305395f7 (.data).
 * pml.previousOrigin (pml.previous_origin): pre-move playerState origin snapshot.
 * Written element-wise by PmoveSingle (0x3000e305/e316/e322) and differenced against
 * the post-move origin at its tail (0x3000e652/e65b/e668). Superseded from three
 * mechanical uint32 dwords (owner item_setfocus was a wrong first-touch label). */
/* Source: uo_cgame_mp_x86.dll 0x305395f8..0x30539603 (.data).
 * pml.previousVelocity (pml.previous_velocity): pre-move playerState velocity
 * snapshot. Written element-wise by PmoveSingle (0x3000e32e/e339/e34b); read by the
 * pmove velocity-decay/air code (0x30009d49/d64). Superseded from three mechanical
 * uint32 dwords (owner g_checkforcursorhints/g_spawnturret were wrong labels). */
/* Source: uo_cgame_mp_x86.dll 0x30539604 (.data); refs=3 width=4; first=0x3000c290.
 * pml.previousWaterLevel: previous-frame latch of pm->waterlevel (+0xf1), used
 * by PM_WaterEvents (0x3000c290) for 0<->nonzero edge detection; written by
 * PmoveSingle at 0x3000e5bd (movzbl waterlevel) after PM_SetWaterLevel (0x3000a7a0)
 * updates it. The mechanical owner=bg_calculateweaponposition_baseang label is a
 * first-touch/size artifact, not the identity. Named by proven role.
 * Example: 3000c290   a1 04 96 53 30               MOV EAX,[0x30539604] | 3000c2d6   39 15 04 96 53 30            CMP dword ptr [0x30539604],EDX | 3000e5bd   a3 04 96 53 30               MOV [0x30539604],EAX
 */
/* Source: uo_cgame_mp_x86.dll 0x30539608 (.data); refs=62 width=4; first=0x3000d8b2.
 * pml.weaponInfo: cached pointer to the current weapon's weaponInfo_t record. The
 * mechanical owner=bg_bullet_endpos label was the first toucher, not the identity;
 * the writer at 0x3000e0e7 sets it from bg_weaponInfos[weaponIndex], and
 * PM_Weapon_AddFiringAimSpreadScale (0x30014240) consumes ->fireAimSpreadScale (+0x2ac).
 * Example: 3000d8b2   8b 0d 08 96 53 30            MOV ECX,dword ptr [0x30539608] | 3000e0ea   89 15 08 96 53 30            MOV dword ptr [0x30539608],EDX | 3000e363   89 15 08 96 53 30            MOV dword ptr [0x30539608],EDX
 */
/* Source: uo_cgame_mp_x86.dll 0x3053962c (.data); refs=1 width=4; first=0x3000c43f.
 * bg_nofatigue.integer: shared fatigue-disable gate consumed by PM_UpdateFatigue
 * (0x3000c420); when nonzero, sprinting never drains fatigueScale.
 * Example: 3000c43f   8b 0d 2c 96 53 30            MOV ECX,dword ptr [0x3053962c]
 */
/* Source: uo_cgame_mp_x86.dll 0x30539620..0x30539730 (.data).
 * Full vmCvar_t whose .integer at +0x0c is the fatigue-disable gate. */
vmCvar_t bg_nofatigue;
/* Source: uo_cgame_mp_x86.dll 0x30539748 (.data); refs=2 width=4; first=0x3000cd60; owner=veh_findvaliddismountspot.
 * Example: 3000cd60   d9 05 48 97 53 30            FLD float ptr [0x30539748] | 3000cd8c   d8 25 48 97 53 30            FSUB float ptr [0x30539748]
 */

/* Source: uo_cgame_mp_x86.dll 0x3053974c (.data); refs=1 width=4; first=0x3000cdce; owner=veh_findvaliddismountspot.
 * Example: 3000cdce   db 05 4c 97 53 30            FILD dword ptr [0x3053974c]
 */

/* Source: uo_cgame_mp_x86.dll 0x30539850 (.data); refs=186 width=4; first=0x3000829c.
 * pm: pointer to the current BG animation player-DObj context. The
 * mechanical owner=bg_getweaponindexforname label was only the first touching
 * function and is NOT the identity; the writers at 0x3000e050/0x3000e740 install
 * this pointer while preparing a player's animation state. Zero-initialized
 * (installed at runtime).
 * Example: 3000829c   8b 0d 50 98 53 30            MOV ECX,dword ptr [0x30539850] | 300082c8   a1 50 98 53 30               MOV EAX,[0x30539850] | 30008312   a1 50 98 53 30               MOV EAX,[0x30539850]
 */
/* The corresponding pm source object is shared in bg_pmove_state.c. */
/* Source: uo_cgame_mp_x86.dll 0x3053986c (.data). bg_foliagesnd_resetinterval.integer — quiet-time delay
 * (ms, signed int) added to playerState_t::foliageSoundTime (+0x38) on the below-floor
 * path of PM_FoliageSounds (0x3000c110) before the foliage timer is reset to 0. Never
 * written in this DLL (cvar/config owned). See globals.h.
 * Example: 3000c135   8b 3d 6c 98 53 30            MOV EDI,dword ptr [0x3053986c]
 */

/* Source: uo_cgame_mp_x86.dll 0x3053998c (.data). bg_foliagesnd_slowinterval.integer — foliage-sound
 * repeat interval (ms, signed int) at the speed floor; base of the speed->interval lerp in
 * PM_FoliageSounds (0x3000c110). Never written in this DLL (cvar/config owned). See globals.h.
 * Example: 3000c18d   2b 05 8c 99 53 30            SUB EAX,dword ptr [0x3053998c] | 3000c19d   da 05 8c 99 53 30            FIADD dword ptr [0x3053998c]
 */

/* Source: uo_cgame_mp_x86.dll 0x30539aac (.data). bg_viewheight_standing.integer — default/standing
 * stance view height; read by CG_CalcMuzzlePoint (fallthrough FILD) and PM_VerifyPronePosition.
 * See globals.h. */

/* Source: uo_cgame_mp_x86.dll 0x30539bc8 (.data). Lower endpoint of the
 * PM_CrashLand fall-height-to-damage envelope. */

/* Source: uo_cgame_mp_x86.dll 0x30539cec (.data); refs=3 width=4; first=0x3000cb14; owner=veh_findvaliddismountspot.
 * Example: 3000cb14   8b 3d ec 9c 53 30            MOV EDI,dword ptr [0x30539cec] | 3000cb4e   db 05 ec 9c 53 30            FILD dword ptr [0x30539cec] | 3000cbd3   db 05 ec 9c 53 30            FILD dword ptr [0x30539cec]
 */

/* Source: uo_cgame_mp_x86.dll 0x30539e0c (.data). bg_viewheight_crouched_vmCvar.integer — crouch-stance muzzle
 * view height; sole reader CG_CalcMuzzlePoint (FILD [0x30539e0c]). See globals.h. */

/* Source: uo_cgame_mp_x86.dll 0x30539f2c (.data). bg_foliagesnd_maxspeed.integer — horizontal-speed
 * ceiling (units/s, signed int) of the speed->interval interpolation in PM_FoliageSounds
 * (0x3000c110); the speed fraction is clamped to 1.0 at this ceiling. Never written in this
 * DLL (cvar/config owned). See globals.h.
 * Example: 3000c160   8b 15 2c 9f 53 30            MOV EDX,dword ptr [0x30539f2c]
 */

/* Source: uo_cgame_mp_x86.dll 0x3053a030 (.data); refs=14 width=4; first=0x30003b75; owner=bg_evaluatetrajectory.
 * Example: 30003b75   8b 0d 30 a0 53 30            MOV ECX,dword ptr [0x3053a030] | 30003bf2   2b 05 30 a0 53 30            SUB EAX,dword ptr [0x3053a030] | 30003cbe   a1 30 a0 53 30               MOV EAX,[0x3053a030]
 */
/* cg_effectTime: the effect subsystem's current time base (ms).
 * CG_InstallSnapshotResetEffects (FUN_3003c9d0) mirrors the snapshot serverTime
 * here alongside cg_time; effect code FILDs it and subtracts it for elapsed math.
 * Provisional role name (bg_evaluatetrajectory is the mechanical first-toucher). */
/* Source: uo_cgame_mp_x86.dll 0x3053a030. */
uint32_t cg_effectTime = (uint32_t)0x00000000u;
/* Source: uo_cgame_mp_x86.dll 0x3053a034 (.data); refs=5 width=4; first=0x300040f2.
 * cg_effectAnimTime: a third effect-subsystem time value (ms), sibling of cg_effectTime
 * (0x3053a030) and cg_effectFrameTime (0x3053a038). All three are seeded together by the
 * effect reset (0x3002df81..df8b); this one is separately refreshed at 0x3003d31c. Used by
 * BG_RunLerpFrameRate (0x30004050) as the animation-blend clock (diffed against
 * emitter->lastEffectTime). Supersedes the mechanical g_data_launchitem_3053a034 (owner
 * label was the size-guessed first-toucher). See globals.h. Zero-initialized in .data.
 * Example: 300040f2 MOV EBP,[0x3053a034] | 30004143 MOV EAX,[0x3053a034] | 3000425e MOV EAX,[0x3053a034]
 */
int32_t cg_effectAnimTime = 0;
/* Source: uo_cgame_mp_x86.dll 0x3053a038 (.data); refs=6; first=0x300043ca.
 * cg_effectFrameTime: effect-subsystem integer time value (ms), sibling of
 * cg_effectTime. Consumed only as int->float (FILD) and scaled to per-frame lerp
 * magnitudes by BG_Player_DoControllers (0x30005746 x0.36, 0x300057f6 x0.1) and
 * by effect code (0x300043ca/0x30004442). Superseded from the mechanical dword
 * g_data_bg_setupsharedammoindexes_3053a038 (owner was the first toucher).
 * Provisional role name; see globals.h. Zero-initialized in .data. */
uint32_t cg_effectFrameTime = (uint32_t)0x00000000u;
/* Source: uo_cgame_mp_x86.dll 0x3053a040 (.data); refs=3 width=4; first=0x30001509; owner=cmd_veh_freevehicle.
 * RESOLVED: 0x3053a040 is weaponStrings, the 128-entry
 * bg_indexed_string_t value table for bgAnimConditionTypes[ANIM_COND_WEAPON].
 * It occupies exactly [0x3053a040, 0x3053a440) = 0x400 bytes = 128 * 8-byte
 * { name, hash } entries (the next distinct .data symbol is at 0x3053a440).
 * Built by BG_InitWeaponStrings (0x30001500): the .a040 / .a044
 * writes are table[0].name and table[0].hash, and the indexed
 * [EBP*8 + 0x3053a040] / [EBP*8 + 0x3053a044] writes fill entries 1.. .
 * Supersedes the two mechanical g_data_cmd_veh_freevehicle_3053a040/..a044
 * fragments (mislabeled after the wrong first-touching owner) with the single
 * typed array; declared in client_recovered.h. Zero-initialized like the .data
 * BSS the exporter observed (the table is fully rebuilt at setup time).
 * Example: 30001509 MOV EDI,0x3053a040 | 30001515 MOV [0x3053a040],EAX |
 *          3000153f MOV [EBP*0x8 + 0x3053a040],ESI |
 *          3000151f MOV [0x3053a044],EAX | 30001580 MOV [EBP*0x8 + 0x3053a044],EBX
 */
/* The corresponding source object is weaponStrings in
 * src/bg/bg_animation_script_data.c. */
/* Source: uo_cgame_mp_x86.dll 0x3053a440..0x305f5334 (.data).
 * Complete zero-filled bgs state: the BG table, resolved tree/animation state,
 * callback addresses, and 64 clientInfo_t records. Every internal boundary and
 * the full REP-STOSD reset extent are asserted in client_recovered.h. */
bgs_t bgs;
/* 0x3053a484 is NOT a distinct global: it is bgs.animationTable.entries[0] + 0x44
 * (bg_static_animation_t.moveSpeed of entry 0). The lone reference at 0x30004230,
 * `MOV ECX,[EAX + 0x3053a484]` with EAX = animIndex*0x5c, is
 * bgs.animationTable.entries[animIndex].moveSpeed. The former mechanical
 * g_data_launchitem_3053a484 storage aliased into the bgs.animationTable.entries[] array above
 * (two symbols owning one address); removed. Consumers index bgs.animationTable.entries[]. */
/* 0x3053a490 / 0x3053a494 are NOT distinct globals: they are bgs.animationTable.entries[0] + 0x50
 * / +0x54 (bg_static_animation_t.flags / .stateFlags of entry 0), consumed by
 * BG_AnimPlayerConditions (0x30004860) as bgs.animationTable.entries[animNum].flags/.stateFlags.
 * The former mechanical g_data_bg_getminspreadforweapon_3053a490 / g_data_fire_grenade_3053a494
 * storage aliased into the bgs.animationTable.entries[] array above (two symbols owning one
 * address); removed. Consumers index bgs.animationTable.entries[]. */
/* Source: uo_cgame_mp_x86.dll 0x305e1f0c (.data); refs=4 width=2/4; first=0x30003eff; owner=bg_evaluatetrajectory.
 * Example: 30003eff   0f b7 15 0c 1f 5e 30         MOVZX EDX,word ptr [0x305e1f0c] | 30003fce   0f b7 15 0c 1f 5e 30         MOVZX EDX,word ptr [0x305e1f0c] | 30005c26   a3 0c 1f 5e 30               MOV [0x305e1f0c],EAX
 */
/* Source: uo_cgame_mp_x86.dll 0x305e1f10 (.data); refs=4 width=2/4; first=0x30003f3e; owner=bg_evaluatetrajectory.
 * Example: 30003f3e   0f b7 0d 10 1f 5e 30         MOVZX ECX,word ptr [0x305e1f10] | 30004012   0f b7 05 10 1f 5e 30         MOVZX EAX,word ptr [0x305e1f10] | 30005c2b   89 0d 10 1f 5e 30            MOV dword ptr [0x305e1f10],ECX
 */
/* Source: uo_cgame_mp_x86.dll 0x305e1f14 (.data); refs=2 width=2/4; first=0x30005c31; owner=vectorlength (mislabeled; superseded).
 * A copy of bgs.turningAnimHandle (0x305e1f30), written as a full int32 by
 * BG_FindAnimTrees (0x30005be0) alongside the torso/legs resolved
 * copies (0x305e1f0c/0x305e1f10). Read back as a 16-bit bone index
 * (MOVZX word) at 0x30034945. Supersedes the mechanical size-guess owner name.
 * Example: 30005c31   89 15 14 1f 5e 30            MOV dword ptr [0x305e1f14],EDX | 30034945   0f b7 0d 14 1f 5e 30         MOVZX ECX,word ptr [0x305e1f14]
 */
/* Source: uo_cgame_mp_x86.dll 0x305e1f20 (.data); the loaded master "multiplayer"
 * anim tree. Written by the loader-setup function (0x30005be0) and read back by
 * BG_LoadAnimTreeInstances (0x30005c40) as the source tree for the per-entity
 * instances it creates. See bgs.multiplayerAnimTree in globals.h. Renamed from the
 * mechanical g_data_vectorlength_* (owner label was a first-touch artifact).
 */
/* Source: uo_cgame_mp_x86.dll 0x305e1f24 (.data); refs=3 width=2/4; first=0x30003dd9; owner=bg_evaluatetrajectory (mislabeled; superseded).
 * Bone index for "root"; filled by BG_FindAnims (0x30005b50).
 * Example: 30003dd9   a1 24 1f 5e 30               MOV EAX,[0x305e1f24] | 30005b50   68 24 1f 5e 30               PUSH 0x305e1f24 | 300348a9   0f b7 0d 24 1f 5e 30         MOVZX ECX,word ptr [0x305e1f24]
 */
/* Source: uo_cgame_mp_x86.dll 0x305e1f28 (.data); refs=2 width=4; first=0x30005b65; owner=pm_getjumpfactor (mislabeled; superseded).
 * Bone index for "torso".
 * Example: 30005b65   68 28 1f 5e 30               PUSH 0x305e1f28 | 30005c09   a1 28 1f 5e 30               MOV EAX,[0x305e1f28]
 */
/* Source: uo_cgame_mp_x86.dll 0x305e1f2c (.data); refs=2 width=4; first=0x30005b7a; owner=pm_getjumpfactor (mislabeled; superseded).
 * Bone index for "legs".
 * Example: 30005b7a   68 2c 1f 5e 30               PUSH 0x305e1f2c | 30005c0e   8b 0d 2c 1f 5e 30            MOV ECX,dword ptr [0x305e1f2c]
 */
/* Source: uo_cgame_mp_x86.dll 0x305e1f30 (.data); refs=2 width=4; first=0x30005b8f; owner=pm_getjumpfactor (mislabeled; superseded).
 * Bone index for "turning".
 * Example: 30005b8f   68 30 1f 5e 30               PUSH 0x305e1f30 | 30005c14   8b 15 30 1f 5e 30            MOV EDX,dword ptr [0x305e1f30]
 */
/* Source: uo_cgame_mp_x86.dll 0x305e1f34 (.data); refs=40 width=4; first=0x30002f55; owner=clearregistereditems.
 * RESOLVED IDENTITY: this address is the base of bgs.clientinfo — the per-client
 * BG animation / player-state array, element type clientInfo_t (a dedicated
 * exactly-0x4d0-byte element; NOT the oversized playerState_t — see globals.h),
 * stride 0x4d0. Proven by BG_EvaluateConditions (0x30002ee0) and
 * CG_DrawPlayerLocation (0x30031280), both indexing `0x305e1f34 + clientNum*0x4d0`;
 * element 0's conditionWords sub-block (+0x468) is bgs.clientinfo[0].conditionWords[0] at
 * 0x305e239c, confirming they are one array. The mechanical "clearregistereditems"
 * owner label is the exporter's wrong first-toucher and is superseded here.
 * Example: 30002f55   8d b1 34 1f 5e 30            LEA ESI,[ECX + 0x305e1f34] | 300032c0   8d b6 34 1f 5e 30            LEA ESI,[ESI + 0x305e1f34] | 30016582   8b 90 34 1f 5e 30            MOV EDX,dword ptr [EAX + 0x305e1f34]
 *
 * Outer extent (MAX_CLIENTS) is not proven by these consumers; storage sized to the
 * standard MP client count (64) as a provisional bound, mirroring per-client conditionWords.
 * CONSOLIDATION THREAD: the later g_data_* fragments at 0x305e1f40.. are interior
 * columns of element 0 of THIS array and should be folded in once their consumers are
 * reconstructed.
 */
/* 0x305e1f60 was the mechanical fragment for an interior column of bgs.clientinfo[]
 * element 0 (element base 0x305e1f34, so 0x305e1f60 == +0x2c). It is now the per-client
 * clientInfo_t.team field (see globals.h), owned by bgs.clientinfo[] storage
 * below; the standalone g_data_cg_asset_parse_305e1f60 definition is removed to avoid two
 * symbols aliasing the same storage. */
/* 0x305e2334 / 0x305e2338 are interior columns of bgs.clientinfo[] element 0
 * (element base 0x305e1f34): 0x305e2334 == +0x400 (gunHandLeft), 0x305e2338 ==
 * +0x404 (dobjNeedsUpdate). They are folded into clientInfo_t and backed by the
 * bgs.clientinfo[64] storage above; the mechanical
 * g_data_pm_beginaimdownsight_305e2334/_305e2338 definitions are removed here to
 * avoid two symbols aliasing the same storage. Consumer:
 * CG_SetGunHandFromNotetracks (0x3001f760). */
/* Source: uo_cgame_mp_x86.dll 0x305e239c (.data); refs=12 width=4; first=0x300034b4; owner=bg_loadanimtreeinstances.
 * RESOLVED IDENTITY: this address is
 * bgs.clientinfo[0].conditionWords[0][0], an interior member of
 * the per-client player-animation array. The mechanically split fragments
 * 0x305e239c (conditionWords[0][0]) and 0x305e23a0 (conditionWords[0][1],
 * first=0x300034be) are superseded by the typed owner; the
 * "bg_loadanimtreeinstances" owner label was a size-match artifact —
 * the first-touching function 0x30003490 is BG_UpdateConditionValue, not the
 * anim-tree loader.
 * Example: 300034b4   c7 80 9c 23 5e 30 ... MOV dword ptr [EAX + 0x305e239c],0x0 | 300034e9   89 0c c5 9c 23 5e 30   MOV dword ptr [EAX*0x8 + 0x305e239c],ECX
 *
 * The inner dimension is BG_ANIM_MAX_CONDITIONS (11), proven by the 0x58-byte
 * +0x468..+0x4bf member extent and the matching script-condition count. The
 * machine's multiplier 154 is the 0x4d0-byte outer player-record stride measured
 * in 8-byte condition entries (0x4d0 / 8), not an inner array bound. The outer
 * storage is provisionally sized to the standard MP client count (64).
 * CONSOLIDATION THREAD: the later g_data_* fragments at 0x305e23a4.. (owners
 * cg_registersounds/turret_think/fire_grenade) are columns of THIS array and
 * should be folded in once their consumers are reconstructed.
 */
/* 0x305e23c4 / 0x305e23c8 are interior columns of bgs.clientinfo[] element 0
 * (element base 0x305e1f34): 0x305e23c4 == +0x490, 0x305e23c8 == +0x494. They are
 * folded into clientInfo_t.conditionWords[ANIM_COND_MOVETYPE][2] and backed by the
 * bgs.clientinfo[64] storage above; the mechanical g_data_turret_think_305e23c4/
 * _305e23c8 definitions are removed here to avoid two symbols aliasing the same
 * storage. Consumers: BG_ExecuteCommand (0x300031d0), anim-slot writer (0x30004c7a). */
/* 0x305e23f8 is bgs.clientinfo[0].animTree (+0x4c4), not a
 * separately allocated array. BG_LoadAnimTreeInstances walks that field with the
 * containing clientInfo_t stride. */

/* The cgame registration table at 0x300851f0 proves each address below is the
 * base of a complete zero-initialized vmCvar_t, even where no direct Windows
 * instruction references the object. */
/* Source: uo_cgame_mp_x86.dll 0x304572e0 (.data); cg_cvarTable[0]. */
vmCvar_t cg_ignore;
/* Source: uo_cgame_mp_x86.dll 0x304584e0 (.data); cg_cvarTable[18]. */
vmCvar_t cg_drawCrosshairPickups;
/* Source: uo_cgame_mp_x86.dll 0x304406a0 (.data); cg_cvarTable[20]. */
vmCvar_t cg_drawRewards;
/* Source: uo_cgame_mp_x86.dll 0x304529c0 (.data); cg_cvarTable[31]. */
vmCvar_t cg_hudStanceFlash_r;
/* Source: uo_cgame_mp_x86.dll 0x3052eb60 (.data); cg_cvarTable[32]. */
vmCvar_t cg_hudStanceFlash_g;
/* Source: uo_cgame_mp_x86.dll 0x30455ea0 (.data); cg_cvarTable[33]. */
vmCvar_t cg_hudStanceFlash_b;
/* Source: uo_cgame_mp_x86.dll 0x30456680 (.data); cg_cvarTable[40]. */
vmCvar_t cg_weaponCycleDelay;
/* Source: uo_cgame_mp_x86.dll 0x3052f220 (.data); cg_cvarTable[69]. */
vmCvar_t cg_noplayeranims;
/* Source: uo_cgame_mp_x86.dll 0x304218a0 (.data); cg_cvarTable[86]. */
vmCvar_t cg_drawTeamOverlay;
/* Source: uo_cgame_mp_x86.dll 0x30455c60 (.data); cg_cvarTable[91]. */
vmCvar_t cg_hudFiles;
/* Source: uo_cgame_mp_x86.dll 0x30530540 (.data); cg_cvarTable[102]. */
vmCvar_t cg_currentSelectedPlayerName;
/* Source: uo_cgame_mp_x86.dll 0x30413340 (.data); cg_cvarTable[103]. */
vmCvar_t cg_deadbodyque;
/* Source: uo_cgame_mp_x86.dll 0x30451580 (.data); cg_cvarTable[104]. */
vmCvar_t g_gametype;
/* Source: uo_cgame_mp_x86.dll 0x30451fa0 (.data); cg_cvarTable[106]. */
vmCvar_t cg_animState;
/* Source: uo_cgame_mp_x86.dll 0x30450ec0 (.data); cg_cvarTable[107]. */
vmCvar_t cl_waitForFire;
/* Source: uo_cgame_mp_x86.dll 0x30457ac0 (.data); cg_cvarTable[109]. */
vmCvar_t cg_autoscreenshot;
/* Source: uo_cgame_mp_x86.dll 0x3052fa00 (.data); cg_cvarTable[110]. */
vmCvar_t cg_autodemo;
/* Source: uo_cgame_mp_x86.dll 0x30451220 (.data); cg_cvarTable[111]. */
vmCvar_t cg_showdemoname;
/* Source: uo_cgame_mp_x86.dll 0x30458720 (.data); cg_cvarTable[114]. */
vmCvar_t version;
/* Source: uo_cgame_mp_x86.dll 0x30455d80 (.data); cg_cvarTable[125]. */
vmCvar_t r_optimize;
/* Source: uo_cgame_mp_x86.dll 0x3044f840 (.data); cg_cvarTable[126]. */
vmCvar_t r_optimizeXModels;
/* Source: uo_cgame_mp_x86.dll 0x30453080 (.data); cg_cvarTable[180]. */
vmCvar_t cg_atmos;
/* Source: uo_cgame_mp_x86.dll 0x30451d60 (.data); cg_cvarTable[181]. */
vmCvar_t cg_atmosDense;

/* cg_cvarTable-proven vmCvar_t objects consolidated from field-only globals. */
/* Source: uo_cgame_mp_x86.dll 0x30452780 (.data); cg_cvarTable[1] "cg_drawGun". */
vmCvar_t cg_drawGun_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x305301e0 (.data); cg_cvarTable[2] "cg_cursorHints". */
vmCvar_t cg_cursorHints_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x3052fc40 (.data); cg_cvarTable[3] "cg_hintFadeTime". */
vmCvar_t cg_hintFadeTime_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x3052f7c0 (.data); cg_cvarTable[4] "cg_fov". */
vmCvar_t cg_fov_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x304567a0 (.data); cg_cvarTable[6] "cg_letterbox". */
vmCvar_t cg_letterbox_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x304570a0 (.data); cg_cvarTable[7] "cg_stereoSeparation_vmCvar.value". */
vmCvar_t cg_stereoSeparation_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x3044fde0 (.data); cg_cvarTable[8] "cg_shadows". */
vmCvar_t cg_shadows_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x304520c0 (.data); cg_cvarTable[9] "cg_draw2D". */
vmCvar_t cg_draw2D_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30450a40 (.data); cg_cvarTable[10] "cg_drawStatus". */
vmCvar_t cg_drawStatus_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x3044f180 (.data); cg_cvarTable[11] "cg_drawFPS". */
vmCvar_t cg_drawFPS_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30456b00 (.data); cg_cvarTable[12] "cg_drawSoundOverlay". */
vmCvar_t cg_drawSoundOverlay_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x304571c0 (.data); cg_cvarTable[13] "cg_drawScriptUsage". */
vmCvar_t cg_drawScriptUsage_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30459020 (.data); cg_cvarTable[14] "cg_drawShader". */
vmCvar_t cg_drawShader_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30450b60 (.data); cg_cvarTable[15] "cg_drawSnapshot_vmCvar.integer". */
vmCvar_t cg_drawSnapshot_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x3044fa80 (.data); cg_cvarTable[16] "cg_drawCrosshair". */
vmCvar_t cg_drawCrosshair_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30457880 (.data); cg_cvarTable[17] "cg_drawCrosshairNames_vmCvar.integer". */
vmCvar_t cg_drawCrosshairNames_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30421e40 (.data); cg_cvarTable[19] "sv_night". */
vmCvar_t sv_night_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x304583c0 (.data); cg_cvarTable[21] "cg_hudAlpha". */
vmCvar_t cg_hudAlpha_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x3048c4a0 (.data); cg_cvarTable[22] "cg_hudCompassSize". */
vmCvar_t cg_hudCompassSize_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x304219c0 (.data); cg_cvarTable[23] "cg_hudCompassMaxRange". */
vmCvar_t cg_hudCompassMaxRange_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30452540 (.data); cg_cvarTable[24] "cg_hudCompassMinRange". */
vmCvar_t cg_hudCompassMinRange_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30456560 (.data); cg_cvarTable[25] "cg_hudCompassMinRadius". */
vmCvar_t cg_hudCompassMinRadius_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30450fe0 (.data); cg_cvarTable[26] "cg_hudCompassSpringyPointers". */
vmCvar_t cg_hudCompassSpringyPointers_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x3044fba0 (.data); cg_cvarTable[27] "cg_hudObjectiveMinHeight". */
vmCvar_t cg_hudObjectiveMinHeight_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30456440 (.data); cg_cvarTable[28] "cg_hudObjectiveMaxHeight". */
vmCvar_t cg_hudObjectiveMaxHeight_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x304517c0 (.data); cg_cvarTable[29] "cg_hudObjectiveMaxRange". */
vmCvar_t cg_hudObjectiveMaxRange_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x3044fcc0 (.data); cg_cvarTable[30] "cg_hudObjectiveMinAlpha". */
vmCvar_t cg_hudObjectiveMinAlpha_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x304518e0 (.data); cg_cvarTable[34] "cg_hudStanceHintPrints". */
vmCvar_t cg_hudStanceHintPrints_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30451c40 (.data); cg_cvarTable[35] "cg_hudDamageIconWidth". */
vmCvar_t cg_hudDamageIconWidth_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30450920 (.data); cg_cvarTable[36] "cg_hudDamageIconHeight". */
vmCvar_t cg_hudDamageIconHeight_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30450380 (.data); cg_cvarTable[37] "cg_hudDamageIconOffset". */
vmCvar_t cg_hudDamageIconOffset_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x3052ea40 (.data); cg_cvarTable[38] "cg_hudDamageIconTime". */
vmCvar_t cg_hudDamageIconTime_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30457760 (.data); cg_cvarTable[39] "cg_hudDamageIconInScope". */
vmCvar_t cg_hudDamageIconInScope_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30440340 (.data); cg_cvarTable[41] "cg_weaponSelect". */
vmCvar_t cg_weaponSelect_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30452420 (.data); cg_cvarTable[42] "cg_crosshairAlpha". */
vmCvar_t cg_crosshairAlpha_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30455fc0 (.data); cg_cvarTable[43] "cg_crosshairAlphaMin". */
vmCvar_t cg_crosshairAlphaMin_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30457640 (.data); cg_cvarTable[44] "cg_crosshairDynamic". */
vmCvar_t cg_crosshairDynamic_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x3044f3c0 (.data); cg_cvarTable[46] "cg_brass". */
vmCvar_t cg_brass_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30412ec0 (.data); cg_cvarTable[47] "cg_marks". */
vmCvar_t cg_marks_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30458de0 (.data); cg_cvarTable[48] "cg_lagometer". */
vmCvar_t cg_lagometer_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30451460 (.data); cg_cvarTable[49] "cg_railTrailTime_vmCvar.integer". */
vmCvar_t cg_railTrailTime_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x304521e0 (.data); cg_cvarTable[50] "cg_gunX_vmCvar.value". */
vmCvar_t cg_gunX_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30421ae0 (.data); cg_cvarTable[51] "cg_gunY_vmCvar.value". */
vmCvar_t cg_gunY_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30450260 (.data); cg_cvarTable[52] "cg_gunZ_vmCvar.value". */
vmCvar_t cg_gunZ_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30457f40 (.data); cg_cvarTable[53] "cg_gun_move_f". */
vmCvar_t cg_gun_move_f_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30457be0 (.data); cg_cvarTable[54] "cg_gun_move_r". */
vmCvar_t cg_gun_move_r_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x3052fb20 (.data); cg_cvarTable[55] "cg_gun_move_u". */
vmCvar_t cg_gun_move_u_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30530420 (.data); cg_cvarTable[56] "cg_gun_ofs_f". */
vmCvar_t cg_gun_ofs_f_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30456d40 (.data); cg_cvarTable[57] "cg_gun_ofs_r". */
vmCvar_t cg_gun_ofs_r_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30451340 (.data); cg_cvarTable[58] "cg_gun_ofs_u". */
vmCvar_t cg_gun_ofs_u_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x304555a0 (.data); cg_cvarTable[59] "cg_gun_move_rate". */
vmCvar_t cg_gun_move_rate_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30451100 (.data); cg_cvarTable[60] "cg_gun_move_minspeed". */
vmCvar_t cg_gun_move_minspeed_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30452c00 (.data); cg_cvarTable[61] "cg_centertime". */
vmCvar_t cg_centertime_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30451e80 (.data); cg_cvarTable[62] "cg_skybox". */
vmCvar_t cg_skybox_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30452660 (.data); cg_cvarTable[65] "cg_debugposition". */
vmCvar_t cg_debugposition_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30456f80 (.data); cg_cvarTable[66] "cg_debugevents". */
vmCvar_t cg_debugevents_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30458600 (.data); cg_cvarTable[68] "cg_nopredict". */
vmCvar_t cg_nopredict_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x3052f100 (.data); cg_cvarTable[70] "cg_showmiss". */
vmCvar_t cg_showmiss_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30457d00 (.data); cg_cvarTable[71] "cg_footsteps". */
vmCvar_t cg_footsteps_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30456200 (.data); cg_cvarTable[72] "cg_tracerchance". */
vmCvar_t cg_tracerchance_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30455900 (.data); cg_cvarTable[73] "cg_tracerchancelmg". */
vmCvar_t cg_tracerchancelmg_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x304556c0 (.data); cg_cvarTable[74] "cg_tracerSpeed". */
vmCvar_t cg_tracerSpeed_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30530660 (.data); cg_cvarTable[75] "cg_tracerlength". */
vmCvar_t cg_tracerlength_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x3048c260 (.data); cg_cvarTable[76] "cg_tracerwidth". */
vmCvar_t cg_tracerwidth_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x304506e0 (.data); cg_cvarTable[77] "cg_tracerlengthlmg". */
vmCvar_t cg_tracerlengthlmg_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x304407c0 (.data); cg_cvarTable[78] "cg_tracerwidthlmg". */
vmCvar_t cg_tracerwidthlmg_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x304569e0 (.data); cg_cvarTable[80] "cg_thirdPersonRange_vmCvar.value". */
vmCvar_t cg_thirdPersonRange_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30530300 (.data); cg_cvarTable[81] "cg_thirdPersonAngle_vmCvar.value". */
vmCvar_t cg_thirdPersonAngle_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x3044f2a0 (.data); cg_cvarTable[82] "cg_thirdPerson". */
vmCvar_t cg_thirdPerson_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30452d20 (.data); cg_cvarTable[84] "cg_chatHeight". */
vmCvar_t cg_chatHeight_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x3052f340 (.data); cg_cvarTable[85] "cg_predictItems_vmCvar.integer". */
vmCvar_t cg_predictItems_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30440460 (.data); cg_cvarTable[87] "cg_stats". */
vmCvar_t cg_stats_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x3052e6e0 (.data); cg_cvarTable[89] "pmove_fixed_vmCvar.integer". */
vmCvar_t pmove_fixed_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x3044f960 (.data); cg_cvarTable[93] "cl_stanceTemp". */
vmCvar_t cl_stanceTemp_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30421c00 (.data); cg_cvarTable[94] "cg_noTaunt". */
vmCvar_t cg_noTaunt_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30450da0 (.data); cg_cvarTable[95] "cg_voiceSpriteTime". */
vmCvar_t cg_voiceSpriteTime_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x3052eda0 (.data); cg_cvarTable[96] "cg_teamChatsOnly_vmCvar.integer". */
vmCvar_t cg_teamChatsOnly_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30458180 (.data); cg_cvarTable[97] "cg_noVoiceChats". */
vmCvar_t cg_noVoiceChats_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x304505c0 (.data); cg_cvarTable[98] "cg_noVoiceText". */
vmCvar_t cg_noVoiceText_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x305300c0 (.data); cg_cvarTable[99] "cl_paused". */
vmCvar_t cl_paused_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30458ba0 (.data); cg_cvarTable[100] "g_synchronousClients". */
vmCvar_t g_synchronousClients_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30456320 (.data); cg_cvarTable[105] "cg_norender_vmCvar.integer". */
vmCvar_t cg_norender_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30458cc0 (.data); cg_cvarTable[108] "cg_dumpAnims". */
vmCvar_t cg_dumpAnims_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x3052f8e0 (.data); cg_cvarTable[112] "developer". */
vmCvar_t developer_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30452f60 (.data); cg_cvarTable[113] "con_minicon". */
vmCvar_t con_minicon_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x304516a0 (.data); cg_cvarTable[115] "cg_subtitles". */
vmCvar_t cg_subtitles_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x3052ffa0 (.data); cg_cvarTable[116] "cg_subtitleMinTime". */
vmCvar_t cg_subtitleMinTime_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x3048c5c0 (.data); cg_cvarTable[117] "cg_subtitleWidth". */
vmCvar_t cg_subtitleWidth_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x304568c0 (.data); cg_cvarTable[118] "cg_gameMessageWidth". */
vmCvar_t cg_gameMessageWidth_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x3048c140 (.data); cg_cvarTable[119] "cg_gameBoldMessageWidth". */
vmCvar_t cg_gameBoldMessageWidth_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30452ae0 (.data); cg_cvarTable[120] "cl_languagewarnings". */
vmCvar_t cl_languagewarnings_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30451b20 (.data); cg_cvarTable[121] "cl_languagewarningsaserrors". */
vmCvar_t cl_languagewarningsaserrors_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30421d20 (.data); cg_cvarTable[123] "cg_scoreboardScrollStep_vmCvar.integer". */
vmCvar_t cg_scoreboardScrollStep_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x3052f6a0 (.data); cg_cvarTable[124] "cg_descriptiveText". */
vmCvar_t cg_descriptiveText_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30539aa0 (.data); cg_cvarTable[127] "bg_viewheight_standing". */
vmCvar_t bg_viewheight_standing;
/* Source: uo_cgame_mp_x86.dll 0x30539e00 (.data); cg_cvarTable[128] "bg_viewheight_crouched". */
vmCvar_t bg_viewheight_crouched_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30539220 (.data); cg_cvarTable[129] "bg_viewheight_prone". */
vmCvar_t bg_viewheight_prone_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30539ce0 (.data); cg_cvarTable[130] "bg_ladder_yawcap". */
vmCvar_t bg_ladder_yawcap;
/* Source: uo_cgame_mp_x86.dll 0x30538fe0 (.data); cg_cvarTable[132] "bg_lmg_yawcap". */
vmCvar_t bg_lmg_yawcap;
/* Source: uo_cgame_mp_x86.dll 0x30539100 (.data); cg_cvarTable[133] "bg_foliagesnd_minspeed". */
vmCvar_t bg_foliagesnd_minspeed;
/* Source: uo_cgame_mp_x86.dll 0x30539f20 (.data); cg_cvarTable[134] "bg_foliagesnd_maxspeed". */
vmCvar_t bg_foliagesnd_maxspeed;
/* Source: uo_cgame_mp_x86.dll 0x30539980 (.data); cg_cvarTable[135] "bg_foliagesnd_slowinterval". */
vmCvar_t bg_foliagesnd_slowinterval;
/* Source: uo_cgame_mp_x86.dll 0x30539340 (.data); cg_cvarTable[136] "bg_foliagesnd_fastinterval". */
vmCvar_t bg_foliagesnd_fastinterval;
/* Source: uo_cgame_mp_x86.dll 0x30539860 (.data); cg_cvarTable[137] "bg_foliagesnd_resetinterval". */
vmCvar_t bg_foliagesnd_resetinterval;
/* Source: uo_cgame_mp_x86.dll 0x30539bc0 (.data); cg_cvarTable[138] "bg_fallDamageMinHeight". */
vmCvar_t bg_fallDamageMinHeight;
/* Source: uo_cgame_mp_x86.dll 0x30539460 (.data); cg_cvarTable[139] "bg_fallDamageMaxHeight". */
vmCvar_t bg_fallDamageMaxHeight;
/* Source: uo_cgame_mp_x86.dll 0x30538920 (.data); cg_cvarTable[140] "bg_debugWeaponAnim". */
vmCvar_t bg_debugWeaponAnim;
/* Source: uo_cgame_mp_x86.dll 0x30538800 (.data); cg_cvarTable[141] "bg_debugWeaponState". */
vmCvar_t bg_debugWeaponState;
/* Source: uo_cgame_mp_x86.dll 0x30538ec0 (.data); cg_cvarTable[142] "bg_debugWeaponMessages". */
vmCvar_t bg_debugWeaponMessages_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30538da0 (.data); cg_cvarTable[143] "cg_bobAmplitudeStanding". */
vmCvar_t cg_bobAmplitudeStanding_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30538c80 (.data); cg_cvarTable[144] "cg_bobAmplitudeDucked". */
vmCvar_t cg_bobAmplitudeDucked_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30538b60 (.data); cg_cvarTable[145] "cg_bobAmplitudeProne". */
vmCvar_t cg_bobAmplitudeProne_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30538a40 (.data); cg_cvarTable[146] "cg_bobMax_vmCvar.value". */
vmCvar_t cg_bobMax_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x3052efe0 (.data); cg_cvarTable[174] "cg_debuganim". */
vmCvar_t cg_debuganim_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x3044ff00 (.data); cg_cvarTable[175] "bg_swingSpeed". */
vmCvar_t bg_swingSpeed_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x3052ec80 (.data); cg_cvarTable[176] "cg_blood_vmCvar.integer". */
vmCvar_t cg_blood_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30458840 (.data); cg_cvarTable[182] "cg_announcerSounds". */
vmCvar_t cg_announcerSounds_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x304582a0 (.data); cg_cvarTable[183] "cg_vehicletrails". */
vmCvar_t cg_vehicletrails_vmCvar;

/* cg_cvarTable-proven vmCvar_t objects consolidated from field-only globals. */

/* Remaining cg_cvarTable objects with consolidated field fragments. */
/* Source: uo_cgame_mp_x86.dll 0x3048c020 (.data); cg_cvarTable[45] "cg_crosshairNoGun". */
vmCvar_t cg_crosshairNoGun_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30458f00 (.data); cg_cvarTable[67] "cg_errordecay". */
vmCvar_t cg_errordecay_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x3048c380 (.data); cg_cvarTable[79] "cg_tracernightscale". */
vmCvar_t cg_tracernightscale_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x3052e800 (.data); cg_cvarTable[83] "cg_chatTime". */
vmCvar_t cg_chatTime_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30450140 (.data); cg_cvarTable[88] "timescale". */
vmCvar_t timescale_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30413100 (.data); cg_cvarTable[92] "cl_stance". */
vmCvar_t cl_stance_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x3044f600 (.data); cg_cvarTable[101] "cg_currentSelectedPlayer". */
vmCvar_t cg_currentSelectedPlayer_vmCvar;
/* Source: uo_cgame_mp_x86.dll 0x30539740 (.data); cg_cvarTable[131] "bg_prone_yawcap". */
vmCvar_t bg_prone_yawcap;
