#include <stdint.h>
#include <string.h>

#include "client/engine/scripting/script_compile.h"

/* Recovered yacc parser driver and the 124 game-specific grammar reductions.
 * Windows evidence: CoDUOMP.exe 0x00490f50..0x00491cbb and its reduction
 * dispatch table at 0x00491cc0. The same grammar in the reconstructed Linux
 * engine supplies semantic names, but Windows instruction flow, rule numbers,
 * table probes, stack operations, and helper arguments remain authoritative. */

enum {
    SCRIPT_YY_EMPTY_LOOKAHEAD = -1,
    SCRIPT_YY_EOF_TOKEN = 0,
    SCRIPT_YY_ERROR_TOKEN = 0x100,
    SCRIPT_YY_INITIAL_STATE = 0,
    SCRIPT_YY_ERROR_TRANSLATED_TOKEN = 1,
    SCRIPT_YY_FIRST_NONTERMINAL = 90,
    SCRIPT_YY_UNDEFINED_TRANSLATED_TOKEN = 115,
    SCRIPT_YY_MAX_EXTERNAL_TOKEN = 343,
    SCRIPT_YY_FINAL_STATE = 251,
    SCRIPT_YYPACT_NINF = -32768,
    SCRIPT_YYTABLE_NINF = -32768,
    SCRIPT_YY_PARSE_ACCEPT = 0,
    SCRIPT_YY_PARSE_ERROR = 1,
    SCRIPT_YY_PARSE_OVERFLOW = 2,
};

/* Source: CoDUOMP.exe 0x00490f30..0x00490f4a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00490f30_00490f4b.mcode.
 * Name: exact same-module Mac symbol __yy_memcpy. The generated parser uses
 * this overlap-agnostic forward byte copy only while growing its stacks. */
static void __yy_memcpy(void *destination, const void *source, int32_t byteCount)
{
    uint8_t *write = destination;
    const uint8_t *read = source;

    for (int32_t index = 0; index < byteCount; ++index)
        write[index] = read[index];
}

/* NOT_FROM_ORIGINAL_SOURCE: file-local state used only to factor the original
 * parser's register/local-stack reduction code into readable helpers. */
static int16_t *script_yyssp;
/* NOT_FROM_ORIGINAL_SOURCE: see the factoring note above. */
static sval_u script_yyval;
/* NOT_FROM_ORIGINAL_SOURCE: see the factoring note above. */
static sval_u *script_yyvsp;

/* NOT_FROM_ORIGINAL_SOURCE: readable bounds check for generated yytable probes. */
static qboolean ScriptYyTableIndexIsValid(int32_t tableIndex)
{
    return tableIndex >= 0 && tableIndex <= SCRIPT_YYTABLE_INDEX_MAX;
}

/* NOT_FROM_ORIGINAL_SOURCE: names the generated yycheck table probe. */
static qboolean ScriptYyCheckMatches(int32_t tableIndex, int32_t value)
{
    return ScriptYyTableIndexIsValid(tableIndex) != qfalse && script_yycheck[tableIndex] == value;
}

/* NOT_FROM_ORIGINAL_SOURCE: names the generated state/value stack push. */
static void ScriptYyPushValue(int16_t state, sval_u value, uint32_t sourcePos)
{
    script_yyssp++;
    *script_yyssp = state;
    script_yyvsp++;
    *script_yyvsp = value;
    script_yyvsp->source.sourcePos = sourcePos;
}

/* NOT_FROM_ORIGINAL_SOURCE: factors generated yacc reduction actions from yyparse. */
static void ScriptYyRunReduction(int32_t rule)
{
    sval_u local_10 = {0};
    sval_u local_14 = {0};
    sval_u local_18 = {0};
    sval_u local_1c = {0};
    sval_u local_20 = {0};
    sval_u local_24 = {0};
    sval_u local_28 = {0};
    sval_u local_2c = {0};
    sval_u local_30 = {0};
    sval_u local_34 = {0};
    sval_u local_38 = {0};
    sval_u local_3c = {0};
    sval_u local_40 = {0};
    sval_u local_44 = {0};
    sval_u local_48 = {0};
    sval_u local_4c = {0};
    sval_u local_50 = {0};
    sval_u local_54 = {0};
    sval_u local_58 = {0};
    sval_u local_5c = {0};
    sval_u local_60 = {0};
    sval_u local_64 = {0};
    sval_u local_68 = {0};
    sval_u local_6c = {0};
    sval_u local_70 = {0};
    sval_u local_74 = {0};
    sval_u local_78 = {0};
    sval_u local_7c = {0};
    sval_u local_80 = {0};
    sval_u local_84 = {0};
    sval_u local_88 = {0};
    sval_u local_8c = {0};
    sval_u local_90 = {0};
    sval_u local_94 = {0};
    sval_u local_98 = {0};
    sval_u local_9c = {0};
    sval_u local_a0 = {0};
    sval_u local_a4 = {0};
    sval_u local_a8 = {0};
    sval_u local_ac = {0};
    sval_u local_b0 = {0};
    sval_u local_b4 = {0};
    sval_u local_b8 = {0};
    sval_u local_bc = {0};
    sval_u local_c0 = {0};
    sval_u local_c4 = {0};
    sval_u local_c8 = {0};
    sval_u local_cc = {0};
    sval_u local_d0 = {0};
    sval_u local_d4 = {0};
    sval_u local_d8 = {0};
    sval_u local_dc = {0};
    sval_u local_e0 = {0};
    sval_u local_e4 = {0};
    sval_u local_e8 = {0};
    sval_u local_ec = {0};
    sval_u local_f0 = {0};
    sval_u local_f4 = {0};
    sval_u local_f8 = {0};
    sval_u local_fc = {0};
    sval_u local_100 = {0};
    sval_u local_104 = {0};
    sval_u local_108 = {0};
    sval_u local_10c = {0};
    sval_u local_110 = {0};
    sval_u local_114 = {0};
    sval_u local_118 = {0};
    sval_u local_11c = {0};
    sval_u local_120 = {0};
    sval_u local_124 = {0};
    sval_u local_128 = {0};
    sval_u local_12c = {0};
    sval_u local_130 = {0};
    sval_u local_134 = {0};
    sval_u local_138 = {0};
    sval_u local_13c = {0};
    sval_u local_140 = {0};
    sval_u local_144 = {0};
    sval_u local_148 = {0};
    sval_u local_14c = {0};
    sval_u local_150 = {0};
    sval_u local_154 = {0};
    sval_u local_158 = {0};
    sval_u local_15c = {0};
    sval_u local_160 = {0};
    sval_u local_164 = {0};
    sval_u local_168 = {0};
    sval_u local_16c = {0};
    sval_u local_170 = {0};
    sval_u local_174 = {0};
    sval_u local_178 = {0};
    sval_u local_17c = {0};
    sval_u local_180 = {0};
    sval_u local_184 = {0};
    sval_u local_188 = {0};
    sval_u local_18c = {0};
    sval_u local_190 = {0};
    sval_u local_194 = {0};
    sval_u local_198 = {0};
    sval_u local_19c = {0};
    sval_u local_1a0 = {0};
    sval_u local_1a4 = {0};
    sval_u local_1a8 = {0};
    sval_u local_1ac = {0};
    sval_u local_1b0 = {0};
    sval_u local_1b4 = {0};
    sval_u local_1b8 = {0};
    sval_u local_1bc = {0};
    sval_u local_1c0 = {0};
    sval_u local_1c4 = {0};
    sval_u local_1c8 = {0};
    sval_u local_1cc = {0};
    sval_u local_1d0 = {0};
    sval_u local_1d4 = {0};
    sval_u local_1d8 = {0};
    sval_u local_1dc = {0};
    sval_u local_1e0 = {0};
    sval_u local_1e4 = {0};
    sval_u local_1e8 = {0};
    sval_u local_1ec = {0};
    sval_u local_1f0 = {0};
    sval_u local_1f4 = {0};
    sval_u local_1f8 = {0};
    sval_u local_1fc = {0};
    sval_u local_200 = {0};
    sval_u local_204 = {0};
    sval_u local_208 = {0};
    sval_u local_20c = {0};
    sval_u local_210 = {0};
    sval_u local_214 = {0};
    sval_u local_218 = {0};
    sval_u local_21c = {0};
    sval_u local_220 = {0};
    sval_u local_224 = {0};
    sval_u local_228 = {0};
    sval_u local_22c = {0};
    sval_u local_230 = {0};
    sval_u local_234 = {0};
    sval_u local_238 = {0};
    sval_u local_23c = {0};
    sval_u local_240 = {0};
    sval_u local_244 = {0};
    sval_u local_248 = {0};
    sval_u local_24c = {0};
    sval_u local_250 = {0};
    sval_u local_254 = {0};
    sval_u local_258 = {0};
    sval_u local_25c = {0};
    sval_u local_260 = {0};
    sval_u local_264 = {0};
    sval_u local_268 = {0};
    sval_u local_26c = {0};
    sval_u local_270 = {0};
    sval_u local_274 = {0};
    sval_u local_278 = {0};
    sval_u local_27c = {0};
    sval_u local_280 = {0};
    sval_u local_284 = {0};
    sval_u local_288 = {0};
    sval_u local_28c = {0};
    sval_u local_290 = {0};
    sval_u local_294 = {0};
    sval_u local_298 = {0};
    sval_u local_29c = {0};
    sval_u local_2a0 = {0};
    sval_u local_2a4 = {0};
    sval_u local_2a8 = {0};
    sval_u local_2ac = {0};
    sval_u script_parseRootValue = {0};

    switch (rule) {
    case 1:
        script_parseRootValue.source.value =
            (uintptr_t)coduomp_script_ast_new_script_root(script_yyval.source.value, script_yyvsp[0].source.value);
        script_parseRoot = (scr_ast_node_t *)script_parseRootValue.source.value;
        break;
    case 2:
        node_pos(&local_10, script_yyval.source.sourcePos);
        script_yyval.source.value = (uintptr_t)node2(SCR_AST_KIND_PRIMITIVE_EXPRESSION, script_yyval.source.value, local_10.source.value);
        break;
    case 3:
        node_pos(&local_14, script_yyvsp[-1].source.sourcePos);
        node_pos(&local_18, script_yyvsp[0].source.sourcePos);
        node_pos(&local_1c, script_yyval.source.sourcePos);
        script_yyval.source.value = (uintptr_t)node5(SCR_AST_KIND_BOOL_OR, script_yyval.source.value, local_1c.source.value,
                                                     script_yyvsp[0].source.value, local_18.source.value, local_14.source.value);
        script_yyval.source.sourcePos = script_yyvsp[-1].source.sourcePos;
        break;
    case 4:
        node_pos(&local_20, script_yyvsp[-1].source.sourcePos);
        node_pos(&local_24, script_yyvsp[0].source.sourcePos);
        node_pos(&local_28, script_yyval.source.sourcePos);
        script_yyval.source.value = (uintptr_t)node5(SCR_AST_KIND_BOOL_AND, script_yyval.source.value, local_28.source.value,
                                                     script_yyvsp[0].source.value, local_24.source.value, local_20.source.value);
        script_yyval.source.sourcePos = script_yyvsp[-1].source.sourcePos;
        break;
    case 5:
        node_pos(&local_2c, script_yyvsp[-1].source.sourcePos);
        node1_(&local_30, 0x3d);
        script_yyval.source.value = (uintptr_t)node4(SCR_AST_KIND_BINARY_OPERATOR, script_yyval.source.value, script_yyvsp[0].source.value,
                                                     local_30.source.value, local_2c.source.value);
        script_yyval.source.sourcePos = script_yyvsp[-1].source.sourcePos;
        break;
    case 6:
        node_pos(&local_34, script_yyvsp[-1].source.sourcePos);
        node1_(&local_38, 0x3e);
        script_yyval.source.value = (uintptr_t)node4(SCR_AST_KIND_BINARY_OPERATOR, script_yyval.source.value, script_yyvsp[0].source.value,
                                                     local_38.source.value, local_34.source.value);
        script_yyval.source.sourcePos = script_yyvsp[-1].source.sourcePos;
        break;
    case 7:
        node_pos(&local_3c, script_yyvsp[-1].source.sourcePos);
        node1_(&local_40, 0x3f);
        script_yyval.source.value = (uintptr_t)node4(SCR_AST_KIND_BINARY_OPERATOR, script_yyval.source.value, script_yyvsp[0].source.value,
                                                     local_40.source.value, local_3c.source.value);
        script_yyval.source.sourcePos = script_yyvsp[-1].source.sourcePos;
        break;
    case 8:
        node_pos(&local_44, script_yyvsp[-1].source.sourcePos);
        node1_(&local_48, 0x40);
        script_yyval.source.value = (uintptr_t)node4(SCR_AST_KIND_BINARY_OPERATOR, script_yyval.source.value, script_yyvsp[0].source.value,
                                                     local_48.source.value, local_44.source.value);
        script_yyval.source.sourcePos = script_yyvsp[-1].source.sourcePos;
        break;
    case 9:
        node_pos(&local_4c, script_yyvsp[-1].source.sourcePos);
        node1_(&local_50, 0x41);
        script_yyval.source.value = (uintptr_t)node4(SCR_AST_KIND_BINARY_OPERATOR, script_yyval.source.value, script_yyvsp[0].source.value,
                                                     local_50.source.value, local_4c.source.value);
        script_yyval.source.sourcePos = script_yyvsp[-1].source.sourcePos;
        break;
    case 10:
        node_pos(&local_54, script_yyvsp[-1].source.sourcePos);
        node1_(&local_58, 0x42);
        script_yyval.source.value = (uintptr_t)node4(SCR_AST_KIND_BINARY_OPERATOR, script_yyval.source.value, script_yyvsp[0].source.value,
                                                     local_58.source.value, local_54.source.value);
        script_yyval.source.sourcePos = script_yyvsp[-1].source.sourcePos;
        break;
    case 0xb:
        node_pos(&local_5c, script_yyvsp[-1].source.sourcePos);
        node1_(&local_60, 0x43);
        script_yyval.source.value = (uintptr_t)node4(SCR_AST_KIND_BINARY_OPERATOR, script_yyval.source.value, script_yyvsp[0].source.value,
                                                     local_60.source.value, local_5c.source.value);
        script_yyval.source.sourcePos = script_yyvsp[-1].source.sourcePos;
        break;
    case 0xc:
        node_pos(&local_64, script_yyvsp[-1].source.sourcePos);
        node1_(&local_68, 0x44);
        script_yyval.source.value = (uintptr_t)node4(SCR_AST_KIND_BINARY_OPERATOR, script_yyval.source.value, script_yyvsp[0].source.value,
                                                     local_68.source.value, local_64.source.value);
        script_yyval.source.sourcePos = script_yyvsp[-1].source.sourcePos;
        break;
    case 0xd:
        node_pos(&local_6c, script_yyvsp[-1].source.sourcePos);
        node1_(&local_70, 0x45);
        script_yyval.source.value = (uintptr_t)node4(SCR_AST_KIND_BINARY_OPERATOR, script_yyval.source.value, script_yyvsp[0].source.value,
                                                     local_70.source.value, local_6c.source.value);
        script_yyval.source.sourcePos = script_yyvsp[-1].source.sourcePos;
        break;
    case 0xe:
        node_pos(&local_74, script_yyvsp[-1].source.sourcePos);
        node1_(&local_78, 0x46);
        script_yyval.source.value = (uintptr_t)node4(SCR_AST_KIND_BINARY_OPERATOR, script_yyval.source.value, script_yyvsp[0].source.value,
                                                     local_78.source.value, local_74.source.value);
        script_yyval.source.sourcePos = script_yyvsp[-1].source.sourcePos;
        break;
    case 0xf:
        node_pos(&local_7c, script_yyvsp[-1].source.sourcePos);
        node1_(&local_80, 0x47);
        script_yyval.source.value = (uintptr_t)node4(SCR_AST_KIND_BINARY_OPERATOR, script_yyval.source.value, script_yyvsp[0].source.value,
                                                     local_80.source.value, local_7c.source.value);
        script_yyval.source.sourcePos = script_yyvsp[-1].source.sourcePos;
        break;
    case 0x10:
        node_pos(&local_84, script_yyvsp[-1].source.sourcePos);
        node1_(&local_88, 0x48);
        script_yyval.source.value = (uintptr_t)node4(SCR_AST_KIND_BINARY_OPERATOR, script_yyval.source.value, script_yyvsp[0].source.value,
                                                     local_88.source.value, local_84.source.value);
        script_yyval.source.sourcePos = script_yyvsp[-1].source.sourcePos;
        break;
    case 0x11:
        node_pos(&local_8c, script_yyvsp[-1].source.sourcePos);
        node1_(&local_90, 0x49);
        script_yyval.source.value = (uintptr_t)node4(SCR_AST_KIND_BINARY_OPERATOR, script_yyval.source.value, script_yyvsp[0].source.value,
                                                     local_90.source.value, local_8c.source.value);
        script_yyval.source.sourcePos = script_yyvsp[-1].source.sourcePos;
        break;
    case 0x12:
        node_pos(&local_94, script_yyvsp[-1].source.sourcePos);
        node1_(&local_98, 0x4a);
        script_yyval.source.value = (uintptr_t)node4(SCR_AST_KIND_BINARY_OPERATOR, script_yyval.source.value, script_yyvsp[0].source.value,
                                                     local_98.source.value, local_94.source.value);
        script_yyval.source.sourcePos = script_yyvsp[-1].source.sourcePos;
        break;
    case 0x13:
        node_pos(&local_9c, script_yyvsp[-1].source.sourcePos);
        node1_(&local_a0, 0x4b);
        script_yyval.source.value = (uintptr_t)node4(SCR_AST_KIND_BINARY_OPERATOR, script_yyval.source.value, script_yyvsp[0].source.value,
                                                     local_a0.source.value, local_9c.source.value);
        script_yyval.source.sourcePos = script_yyvsp[-1].source.sourcePos;
        break;
    case 0x14:
        node_pos(&local_a4, script_yyvsp[-1].source.sourcePos);
        node1_(&local_a8, 0x4c);
        script_yyval.source.value = (uintptr_t)node4(SCR_AST_KIND_BINARY_OPERATOR, script_yyval.source.value, script_yyvsp[0].source.value,
                                                     local_a8.source.value, local_a4.source.value);
        script_yyval.source.sourcePos = script_yyvsp[-1].source.sourcePos;
        break;
    case 0x15:
        node_pos(&local_ac, script_yyvsp[0].source.sourcePos);
        script_yyval.source.value = (uintptr_t)node2(SCR_AST_KIND_CAST_BOOL, script_yyvsp[0].source.value, local_ac.source.value);
        break;
    case 0x16:
        node_pos(&local_b0, script_yyvsp[0].source.sourcePos);
        script_yyval.source.value = (uintptr_t)node2(SCR_AST_KIND_CAST_INT, script_yyvsp[0].source.value, local_b0.source.value);
        break;
    case 0x17:
        node_pos(&local_b4, script_yyvsp[0].source.sourcePos);
        script_yyval.source.value = (uintptr_t)node2(SCR_AST_KIND_CAST_FLOAT, script_yyvsp[0].source.value, local_b4.source.value);
        break;
    case 0x18:
        node_pos(&local_b8, script_yyvsp[0].source.sourcePos);
        script_yyval.source.value = (uintptr_t)node2(SCR_AST_KIND_CAST_STRING, script_yyvsp[0].source.value, local_b8.source.value);
        break;
    case 0x19:
        node_pos(&local_bc, script_yyvsp[0].source.sourcePos);
        script_yyval.source.value = (uintptr_t)node2(SCR_AST_KIND_BOOL_NOT, script_yyvsp[0].source.value, local_bc.source.value);
        break;
    case 0x1a:
        node_pos(&local_c0, script_yyvsp[0].source.sourcePos);
        script_yyval.source.value = (uintptr_t)node2(SCR_AST_KIND_BOOL_COMPLEMENT, script_yyvsp[0].source.value, local_c0.source.value);
        break;
    case 0x1b:
        script_yyval.source.value = (uintptr_t)node1(SCR_AST_KIND_FOR_CONDITION, script_yyval.source.value);
        break;
    case 0x1c:
        script_yyval.source.value = (uintptr_t)node0(0);
        break;
    case 0x1f:
        node_pos(&local_c4, script_yyval.source.sourcePos);
        script_yyval.source.value = (uintptr_t)node3(SCR_AST_KIND_SCRIPT_FUNCTION_REF, script_yyval.source.value,
                                                     script_yyvsp[0].source.value, local_c4.source.value);
        script_pendingScriptLoadCount++;
        break;
    case 0x20:
        node_pos(&local_c8, script_yyval.source.sourcePos);
        script_yyval.source.value = (uintptr_t)node2(SCR_AST_KIND_FUNCTION_REF, script_yyval.source.value, local_c8.source.value);
        break;
    case 0x21:
        node_pos(&local_cc, script_yyval.source.sourcePos);
        script_yyval.source.value = (uintptr_t)node3(SCR_AST_KIND_SCRIPT_FUNCTION_REF, script_yyval.source.value,
                                                     script_yyvsp[0].source.value, local_cc.source.value);
        script_yyval.source.sourcePos = script_yyvsp[-1].source.sourcePos;
        script_pendingScriptLoadCount++;
        break;
    case 0x22:
        node_pos(&local_d0, script_yyval.source.sourcePos);
        script_yyval.source.value = (uintptr_t)node2(SCR_AST_KIND_FUNCTION_REF, script_yyvsp[0].source.value, local_d0.source.value);
        break;
    case 0x23:
        node_pos(&local_d4, script_yyval.source.sourcePos);
        script_yyval.source.value = (uintptr_t)node2(SCR_AST_KIND_SCRIPT_FUNCTION_NAME, script_yyval.source.value, local_d4.source.value);
        break;
    case 0x24:
        node_pos(&local_d8, script_yyvsp[-2].source.sourcePos);
        script_yyval.source.value =
            (uintptr_t)node2(SCR_AST_KIND_FUNCTION_POINTER_CALL, script_yyvsp[-2].source.value, local_d8.source.value);
        break;
    case 0x25:
        node_pos(&local_dc, script_yyval.source.sourcePos);
        script_yyval.source.value = (uintptr_t)node2(SCR_AST_KIND_FUNCTION_CALL, script_yyval.source.value, local_dc.source.value);
        break;
    case 0x26:
        node_pos(&local_e0, script_yyvsp[0].source.sourcePos);
        node_pos(&local_e4, script_yyval.source.sourcePos);
        script_yyval.source.value =
            (uintptr_t)node3(SCR_AST_KIND_METHOD_CALL, script_yyvsp[0].source.value, local_e4.source.value, local_e0.source.value);
        script_yyval.source.sourcePos = script_yyvsp[0].source.sourcePos;
        break;
    case 0x27:
        node_pos(&local_e8, script_yyvsp[-2].source.sourcePos);
        script_yyval.source.value = (uintptr_t)node3(SCR_AST_KIND_FUNCTION_CALL_VALUE, script_yyval.source.value,
                                                     script_yyvsp[-1].source.value, local_e8.source.value);
        script_yyval.source.sourcePos = script_yyvsp[-2].source.sourcePos;
        break;
    case 0x28:
        node_pos(&local_ec, script_yyvsp[-2].source.sourcePos);
        node_pos(&local_f0, script_yyval.source.sourcePos);
        script_yyval.source.value =
            (uintptr_t)node5(SCR_AST_KIND_METHOD_CALL_VALUE, script_yyval.source.value, script_yyvsp[-3].source.value,
                             script_yyvsp[-1].source.value, local_f0.source.value, local_ec.source.value);
        script_yyval.source.sourcePos = script_yyvsp[-2].source.sourcePos;
        break;
    case 0x29:
        node_pos(&local_f4, script_yyval.source.sourcePos);
        script_yyval.source.value = (uintptr_t)node2(SCR_AST_KIND_EXPRESSION_LIST, script_yyvsp[-1].source.value, local_f4.source.value);
        break;
    case 0x2a:
        node_pos(&local_f8, script_yyval.source.sourcePos);
        script_yyval.source.value = (uintptr_t)node2(SCR_AST_KIND_INTEGER_LITERAL, script_yyval.source.value, local_f8.source.value);
        break;
    case 0x2b:
        node_pos(&local_fc, script_yyval.source.sourcePos);
        script_yyval.source.value = (uintptr_t)node2(SCR_AST_KIND_FLOAT_LITERAL, script_yyval.source.value, local_fc.source.value);
        break;
    case 0x2c:
        node_pos(&local_100, script_yyval.source.sourcePos);
        script_yyval.source.value =
            (uintptr_t)node2(SCR_AST_KIND_NEGATED_INTEGER_LITERAL, script_yyvsp[0].source.value, local_100.source.value);
        break;
    case 0x2d:
        node_pos(&local_104, script_yyval.source.sourcePos);
        script_yyval.source.value =
            (uintptr_t)node2(SCR_AST_KIND_NEGATED_FLOAT_LITERAL, script_yyvsp[0].source.value, local_104.source.value);
        break;
    case 0x2e:
        node_pos(&local_108, script_yyval.source.sourcePos);
        script_yyval.source.value = (uintptr_t)node2(SCR_AST_KIND_STRING, script_yyval.source.value, local_108.source.value);
        break;
    case 0x2f:
        node_pos(&local_10c, script_yyval.source.sourcePos);
        script_yyval.source.value = (uintptr_t)node2(SCR_AST_KIND_ISTRING, script_yyval.source.value, local_10c.source.value);
        break;
    case 0x30:
        script_yyval.source.value = (uintptr_t)node1(SCR_AST_KIND_CALL_VALUE, script_yyval.source.value);
        break;
    case 0x31:
        node_pos(&local_110, script_yyval.source.sourcePos);
        script_yyval.source.value = (uintptr_t)node2(SCR_AST_KIND_REFERENCE_EXPRESSION, script_yyval.source.value, local_110.source.value);
        break;
    case 0x32:
        node_pos(&local_114, script_yyval.source.sourcePos);
        script_yyval.source.value = (uintptr_t)node1(SCR_AST_KIND_UNDEFINED, local_114.source.value);
        break;
    case 0x33:
        node_pos(&local_118, script_yyval.source.sourcePos);
        script_yyval.source.value = (uintptr_t)node1(SCR_AST_KIND_SELF, local_118.source.value);
        break;
    case 0x34:
        node_pos(&local_11c, script_yyval.source.sourcePos);
        script_yyval.source.value = (uintptr_t)node1(SCR_AST_KIND_LEVEL, local_11c.source.value);
        break;
    case 0x35:
        node_pos(&local_120, script_yyval.source.sourcePos);
        script_yyval.source.value = (uintptr_t)node1(SCR_AST_KIND_ANIM, local_120.source.value);
        break;
    case 0x36:
        node_pos(&local_124, script_yyval.source.sourcePos);
        script_yyval.source.value = (uintptr_t)node1(SCR_AST_KIND_GAME, local_124.source.value);
        break;
    case 0x37:
        node_pos(&local_128, script_yyval.source.sourcePos);
        script_yyval.source.value = (uintptr_t)node2(SCR_AST_KIND_SIZE, script_yyval.source.value, local_128.source.value);
        script_yyval.source.sourcePos = script_yyvsp[0].source.sourcePos;
        break;
    case 0x38:
        node_pos(&local_12c, script_yyval.source.sourcePos);
        script_yyval.source.value = (uintptr_t)node2(SCR_AST_KIND_SCRIPT_FUNCTION_NAME, script_yyval.source.value, local_12c.source.value);
        break;
    case 0x39:
        script_yyval.source.value = (uintptr_t)node0(0x40);
        break;
    case 0x3a:
        node_pos(&local_130, script_yyvsp[0].source.sourcePos);
        script_yyval.source.value = (uintptr_t)node2(SCR_AST_KIND_ANIMATION, script_yyvsp[0].source.value, local_130.source.value);
        break;
    case 0x3b:
        node_pos(&local_134, script_yyval.source.sourcePos);
        script_yyval.source.value = (uintptr_t)node2(SCR_AST_KIND_FALSE, script_yyval.source.value, local_134.source.value);
        break;
    case 0x3c:
        node_pos(&local_138, script_yyval.source.sourcePos);
        script_yyval.source.value = (uintptr_t)node2(SCR_AST_KIND_TRUE, script_yyval.source.value, local_138.source.value);
        break;
    case 0x3d:
        node_pos(&local_13c, script_yyval.source.sourcePos);
        script_yyval.source.value = (uintptr_t)node1(SCR_AST_KIND_ANIMTREE, local_13c.source.value);
        break;
    case 0x3e:
        node_pos(&local_140, script_yyvsp[0].source.sourcePos);
        node_pos(&local_144, script_yyval.source.sourcePos);
        script_yyval.source.value = (uintptr_t)node4(SCR_AST_KIND_OBJECT_STRING_REF, script_yyval.source.value,
                                                     script_yyvsp[0].source.value, local_144.source.value, local_140.source.value);
        script_yyval.source.sourcePos = script_yyvsp[0].source.sourcePos;
        break;
    case 0x3f:
        node_pos(&local_148, script_yyvsp[-1].source.sourcePos);
        node_pos(&local_14c, script_yyval.source.sourcePos);
        script_yyval.source.value = (uintptr_t)node4(SCR_AST_KIND_OBJECT_INDEX_OBJECT_REF, script_yyval.source.value,
                                                     script_yyvsp[-1].source.value, local_14c.source.value, local_148.source.value);
        script_yyval.source.sourcePos = script_yyvsp[-2].source.sourcePos;
        break;
    case 0x40:
        node_pos(&local_150, script_yyval.source.sourcePos);
        script_yyval.source.value = (uintptr_t)node2(SCR_AST_KIND_STRING_REF, script_yyval.source.value, local_150.source.value);
        break;
    case 0x41:
        script_yyval.source.value = (uintptr_t)node1(SCR_AST_KIND_CALL_STATEMENT, script_yyval.source.value);
        break;
    case 0x42:
        node_pos(&local_154, script_yyvsp[-1].source.sourcePos);
        script_yyval.source.value = (uintptr_t)node3(SCR_AST_KIND_ASSIGNMENT_STATEMENT, script_yyval.source.value,
                                                     script_yyvsp[0].source.value, local_154.source.value);
        break;
    case 0x43:
        node_pos(&local_158, script_yyval.source.sourcePos);
        script_yyval.source.value =
            (uintptr_t)node2(SCR_AST_KIND_RETURN_VALUE_STATEMENT, script_yyvsp[0].source.value, local_158.source.value);
        break;
    case 0x44:
        node_pos(&local_15c, script_yyval.source.sourcePos);
        script_yyval.source.value = (uintptr_t)node1(SCR_AST_KIND_RETURN_STATEMENT, local_15c.source.value);
        break;
    case 0x45:
        node_pos(&local_160, script_yyval.source.sourcePos);
        node_pos(&local_164, script_yyvsp[0].source.sourcePos);
        script_yyval.source.value =
            (uintptr_t)node3(SCR_AST_KIND_WAIT_STATEMENT, script_yyvsp[0].source.value, local_164.source.value, local_160.source.value);
        break;
    case 0x46:
        node_pos(&local_168, script_yyval.source.sourcePos);
        script_yyval.source.value = (uintptr_t)node2(SCR_AST_KIND_INC_STATEMENT, script_yyval.source.value, local_168.source.value);
        break;
    case 0x47:
        node_pos(&local_16c, script_yyval.source.sourcePos);
        script_yyval.source.value = (uintptr_t)node2(SCR_AST_KIND_DEC_STATEMENT, script_yyval.source.value, local_16c.source.value);
        break;
    case 0x48:
        node_pos(&local_170, script_yyvsp[-1].source.sourcePos);
        node1_(&local_174, 0x3d);
        script_yyval.source.value = (uintptr_t)node4(SCR_AST_KIND_REF_ASSIGNMENT_STATEMENT, script_yyval.source.value,
                                                     script_yyvsp[0].source.value, local_174.source.value, local_170.source.value);
        break;
    case 0x49:
        node_pos(&local_178, script_yyvsp[-1].source.sourcePos);
        node1_(&local_17c, 0x3e);
        script_yyval.source.value = (uintptr_t)node4(SCR_AST_KIND_REF_ASSIGNMENT_STATEMENT, script_yyval.source.value,
                                                     script_yyvsp[0].source.value, local_17c.source.value, local_178.source.value);
        break;
    case 0x4a:
        node_pos(&local_180, script_yyvsp[-1].source.sourcePos);
        node1_(&local_184, 0x3f);
        script_yyval.source.value = (uintptr_t)node4(SCR_AST_KIND_REF_ASSIGNMENT_STATEMENT, script_yyval.source.value,
                                                     script_yyvsp[0].source.value, local_184.source.value, local_180.source.value);
        break;
    case 0x4b:
        node_pos(&local_188, script_yyvsp[-1].source.sourcePos);
        node1_(&local_18c, 0x46);
        script_yyval.source.value = (uintptr_t)node4(SCR_AST_KIND_REF_ASSIGNMENT_STATEMENT, script_yyval.source.value,
                                                     script_yyvsp[0].source.value, local_18c.source.value, local_188.source.value);
        break;
    case 0x4c:
        node_pos(&local_190, script_yyvsp[-1].source.sourcePos);
        node1_(&local_194, 0x47);
        script_yyval.source.value = (uintptr_t)node4(SCR_AST_KIND_REF_ASSIGNMENT_STATEMENT, script_yyval.source.value,
                                                     script_yyvsp[0].source.value, local_194.source.value, local_190.source.value);
        break;
    case 0x4d:
        node_pos(&local_198, script_yyvsp[-1].source.sourcePos);
        node1_(&local_19c, 0x48);
        script_yyval.source.value = (uintptr_t)node4(SCR_AST_KIND_REF_ASSIGNMENT_STATEMENT, script_yyval.source.value,
                                                     script_yyvsp[0].source.value, local_19c.source.value, local_198.source.value);
        break;
    case 0x4e:
        node_pos(&local_1a0, script_yyvsp[-1].source.sourcePos);
        node1_(&local_1a4, 0x49);
        script_yyval.source.value = (uintptr_t)node4(SCR_AST_KIND_REF_ASSIGNMENT_STATEMENT, script_yyval.source.value,
                                                     script_yyvsp[0].source.value, local_1a4.source.value, local_1a0.source.value);
        break;
    case 0x4f:
        node_pos(&local_1a8, script_yyvsp[-1].source.sourcePos);
        node1_(&local_1ac, 0x4a);
        script_yyval.source.value = (uintptr_t)node4(SCR_AST_KIND_REF_ASSIGNMENT_STATEMENT, script_yyval.source.value,
                                                     script_yyvsp[0].source.value, local_1ac.source.value, local_1a8.source.value);
        break;
    case 0x50:
        node_pos(&local_1b0, script_yyvsp[-1].source.sourcePos);
        node1_(&local_1b4, 0x4b);
        script_yyval.source.value = (uintptr_t)node4(SCR_AST_KIND_REF_ASSIGNMENT_STATEMENT, script_yyval.source.value,
                                                     script_yyvsp[0].source.value, local_1b4.source.value, local_1b0.source.value);
        break;
    case 0x51:
        node_pos(&local_1b8, script_yyvsp[-1].source.sourcePos);
        node1_(&local_1bc, 0x4c);
        script_yyval.source.value = (uintptr_t)node4(SCR_AST_KIND_REF_ASSIGNMENT_STATEMENT, script_yyval.source.value,
                                                     script_yyvsp[0].source.value, local_1bc.source.value, local_1b8.source.value);
        break;
    case 0x52:
        node_pos(&local_1c0, script_yyvsp[-3].source.sourcePos);
        node_pos(&local_1c4, script_yyval.source.sourcePos);
        script_yyval.source.value = (uintptr_t)node4(SCR_AST_KIND_WAITTILL_STATEMENT, script_yyval.source.value,
                                                     script_yyvsp[-1].source.value, local_1c4.source.value, local_1c0.source.value);
        break;
    case 0x53:
        node_pos(&local_1c8, script_yyvsp[-3].source.sourcePos);
        node_pos(&local_1cc, script_yyval.source.sourcePos);
        script_yyval.source.value = (uintptr_t)node4(SCR_AST_KIND_WAITTILLMATCH_STATEMENT, script_yyval.source.value,
                                                     script_yyvsp[-1].source.value, local_1cc.source.value, local_1c8.source.value);
        break;
    case 0x54:
        node_pos(&local_1d0, script_yyvsp[-3].source.sourcePos);
        node_pos(&local_1d4, script_yyval.source.sourcePos);
        script_yyval.source.value = (uintptr_t)node4(SCR_AST_KIND_NOTIFY_STATEMENT, script_yyval.source.value,
                                                     script_yyvsp[-1].source.value, local_1d4.source.value, local_1d0.source.value);
        break;
    case 0x55:
        node_pos(&local_1d8, script_yyvsp[-1].source.sourcePos);
        node_pos(&local_1dc, script_yyval.source.sourcePos);
        script_yyval.source.value = (uintptr_t)node4(SCR_AST_KIND_ENDON_STATEMENT, script_yyval.source.value, script_yyvsp[-1].source.value,
                                                     local_1dc.source.value, local_1d8.source.value);
        break;
    case 0x56:
        node_pos(&local_1e0, script_yyval.source.sourcePos);
        script_yyval.source.value = (uintptr_t)node1(SCR_AST_KIND_BREAK_STATEMENT, local_1e0.source.value);
        break;
    case 0x57:
        node_pos(&local_1e4, script_yyval.source.sourcePos);
        script_yyval.source.value = (uintptr_t)node1(SCR_AST_KIND_CONTINUE_STATEMENT, local_1e4.source.value);
        break;
    case 0x58:
        node_pos(&local_1e8, script_yyvsp[-3].source.sourcePos);
        node_pos(&local_1ec, script_yyvsp[-1].source.sourcePos);
        script_yyval.source.value = (uintptr_t)node4(SCR_AST_KIND_DO_WHILE_STATEMENT, script_yyvsp[-4].source.value,
                                                     script_yyvsp[-1].source.value, local_1ec.source.value, local_1e8.source.value);
        break;
    case 0x59:
        script_yyval.source.value = (uintptr_t)node0(0);
        break;
    case 0x5c:
        node_pos(&local_1f0, script_yyval.source.sourcePos);
        script_yyval.source.value = (uintptr_t)node2(SCR_AST_KIND_STATEMENT_BLOCK, script_yyvsp[-1].source.value, local_1f0.source.value);
        break;
    case 0x5d:
        node_pos(&local_1f4, script_yyvsp[-2].source.sourcePos);
        script_yyval.source.value = (uintptr_t)node3(SCR_AST_KIND_IF_STATEMENT, script_yyvsp[-2].source.value, script_yyvsp[0].source.value,
                                                     local_1f4.source.value);
        break;
    case 0x5e:
        node_pos(&local_1f8, script_yyvsp[-4].source.sourcePos);
        script_yyval.source.value = (uintptr_t)node4(SCR_AST_KIND_IF_ELSE_STATEMENT, script_yyvsp[-4].source.value,
                                                     script_yyvsp[-2].source.value, script_yyvsp[0].source.value, local_1f8.source.value);
        break;
    case 0x5f:
        node_pos(&local_1fc, script_yyval.source.sourcePos);
        node_pos(&local_200, script_yyvsp[-2].source.sourcePos);
        script_yyval.source.value = (uintptr_t)node4(SCR_AST_KIND_WHILE_STATEMENT, script_yyvsp[-2].source.value,
                                                     script_yyvsp[0].source.value, local_200.source.value, local_1fc.source.value);
        break;
    case 0x60:
        node_pos(&local_204, script_yyval.source.sourcePos);
        node_pos(&local_208, script_yyvsp[-4].source.sourcePos);
        script_yyval.source.value =
            (uintptr_t)node6(SCR_AST_KIND_FOR_STATEMENT, script_yyvsp[-5].source.value, script_yyvsp[-4].source.value,
                             script_yyvsp[-2].source.value, script_yyvsp[0].source.value, local_208.source.value, local_204.source.value);
        break;
    case 0x61:
        node_pos(&local_20c, script_yyvsp[-2].source.sourcePos);
        script_yyval.source.value = (uintptr_t)node3(SCR_AST_KIND_SWITCH_STATEMENT, script_yyvsp[-2].source.value,
                                                     script_yyvsp[0].source.value, local_20c.source.value);
        break;
    case 0x62:
        node_pos(&local_210, script_yyval.source.sourcePos);
        script_yyval.source.value =
            (uintptr_t)node2(SCR_AST_KIND_DEVELOPER_STATEMENT_BLOCK, script_yyvsp[-1].source.value, local_210.source.value);
        break;
    case 99:
        script_yyval.source.value = (uintptr_t)node0(0);
        break;
    case 100:
        node_pos(&local_214, script_yyval.source.sourcePos);
        script_yyval.source.value = (uintptr_t)node2(SCR_AST_KIND_CASE_STATEMENT, script_yyvsp[-1].source.value, local_214.source.value);
        break;
    case 0x65:
        node_pos(&local_218, script_yyval.source.sourcePos);
        script_yyval.source.value = (uintptr_t)node1(SCR_AST_KIND_DEFAULT_STATEMENT, local_218.source.value);
        break;
    case 0x67:
        script_yyval.source.value =
            (uintptr_t)append_node((scr_ast_list_t *)script_yyval.source.value, (void *)script_yyvsp[0].source.value);
        break;
    case 0x68:
        local_21c.source.value = (uintptr_t)node0(0);
        script_yyval.source.value = (uintptr_t)linked_list_end((void *)local_21c.source.value);
        break;
    case 0x69:
        node_pos(&local_224, script_yyvsp[0].source.sourcePos);
        local_220.source.value = (uintptr_t)node2_(script_yyvsp[0].source.value, local_224.source.value);
        script_yyval.source.value =
            (uintptr_t)prepend_node((void *)local_220.source.value, (scr_ast_list_item_t **)script_yyval.source.value);
        break;
    case 0x6a:
        local_228.source.value = (uintptr_t)node0(0);
        node_pos(&local_230, script_yyval.source.sourcePos);
        local_22c.source.value = (uintptr_t)node2_(script_yyval.source.value, local_230.source.value);
        script_yyval.source.value = (uintptr_t)prepend_node((void *)local_22c.source.value, (scr_ast_list_item_t **)local_228.source.value);
        break;
    case 0x6b:
        script_yyval.source.value = (uintptr_t)node0(0);
        break;
    case 0x6c:
        node_pos(&local_238, script_yyvsp[0].source.sourcePos);
        local_234.source.value = (uintptr_t)node2_(script_yyvsp[0].source.value, local_238.source.value);
        script_yyval.source.value = (uintptr_t)append_node((scr_ast_list_t *)script_yyval.source.value, (void *)local_234.source.value);
        break;
    case 0x6d:
        node_pos(&local_240, script_yyval.source.sourcePos);
        local_23c.source.value = (uintptr_t)node2_(script_yyval.source.value, local_240.source.value);
        local_248.source.value = (uintptr_t)node0(0);
        local_244.source.value = (uintptr_t)linked_list_end((void *)local_248.source.value);
        script_yyval.source.value = (uintptr_t)append_node((scr_ast_list_t *)local_244.source.value, (void *)local_23c.source.value);
        break;
    case 0x6e:
        local_24c.source.value = (uintptr_t)node0(0);
        script_yyval.source.value = (uintptr_t)linked_list_end((void *)local_24c.source.value);
        break;
    case 0x6f:
        node_pos(&local_254, script_yyvsp[0].source.sourcePos);
        local_250.source.value = (uintptr_t)node2_(script_yyvsp[0].source.value, local_254.source.value);
        script_yyval.source.value = (uintptr_t)append_node((scr_ast_list_t *)script_yyval.source.value, (void *)local_250.source.value);
        break;
    case 0x70:
        node_pos(&local_25c, script_yyval.source.sourcePos);
        local_258.source.value = (uintptr_t)node2_(script_yyval.source.value, local_25c.source.value);
        local_264.source.value = (uintptr_t)node0(0);
        local_260.source.value = (uintptr_t)linked_list_end((void *)local_264.source.value);
        script_yyval.source.value = (uintptr_t)append_node((scr_ast_list_t *)local_260.source.value, (void *)local_258.source.value);
        break;
    case 0x71:
        node_pos(&local_26c, script_yyvsp[0].source.sourcePos);
        local_268.source.value = (uintptr_t)node2_(script_yyvsp[0].source.value, local_26c.source.value);
        script_yyval.source.value = (uintptr_t)append_node((scr_ast_list_t *)script_yyval.source.value, (void *)local_268.source.value);
        break;
    case 0x72:
        node_pos(&local_274, script_yyval.source.sourcePos);
        local_270.source.value = (uintptr_t)node2_(script_yyval.source.value, local_274.source.value);
        local_27c.source.value = (uintptr_t)node0(0);
        local_278.source.value = (uintptr_t)linked_list_end((void *)local_27c.source.value);
        script_yyval.source.value = (uintptr_t)append_node((scr_ast_list_t *)local_278.source.value, (void *)local_270.source.value);
        break;
    case 0x73:
        node_pos(&local_284, script_yyvsp[0].source.sourcePos);
        local_280.source.value = (uintptr_t)node2_(script_yyvsp[0].source.value, local_284.source.value);
        script_yyval.source.value =
            (uintptr_t)prepend_node((void *)local_280.source.value, (scr_ast_list_item_t **)script_yyval.source.value);
        break;
    case 0x74:
        local_288.source.value = (uintptr_t)node0(0);
        node_pos(&local_290, script_yyval.source.sourcePos);
        local_28c.source.value = (uintptr_t)node2_(script_yyval.source.value, local_290.source.value);
        script_yyval.source.value = (uintptr_t)prepend_node((void *)local_28c.source.value, (scr_ast_list_item_t **)local_288.source.value);
        break;
    case 0x75:
        node_pos(&local_294, script_yyval.source.sourcePos);
        script_yyval.source.value = (uintptr_t)node4(SCR_AST_KIND_FUNCTION_DEFINITION, script_yyval.source.value,
                                                     script_yyvsp[-4].source.value, script_yyvsp[-1].source.value, local_294.source.value);
        break;
    case 0x76:
        node_pos(&local_298, script_yyvsp[-7].source.sourcePos);
        script_yyval.source.value = (uintptr_t)node4(SCR_AST_KIND_DEVELOPER_FUNCTION_DEFINITION, script_yyvsp[-7].source.value,
                                                     script_yyvsp[-5].source.value, script_yyvsp[-2].source.value, local_298.source.value);
        break;
    case 0x77:
        node_pos(&local_29c, script_yyvsp[-2].source.sourcePos);
        script_yyval.source.value = (uintptr_t)node2(SCR_AST_KIND_USING_ANIMTREE, script_yyvsp[-2].source.value, local_29c.source.value);
        break;
    case 0x78:
        node_pos(&local_2a4, script_yyvsp[0].source.sourcePos);
        local_2a0.source.value = (uintptr_t)node2_(script_yyvsp[0].source.value, local_2a4.source.value);
        script_yyval.source.value = (uintptr_t)append_node((scr_ast_list_t *)script_yyval.source.value, (void *)local_2a0.source.value);
        break;
    case 0x79:
        local_2a8.source.value = (uintptr_t)node0(0);
        script_yyval.source.value = (uintptr_t)linked_list_end((void *)local_2a8.source.value);
        break;
    case 0x7a:
        script_yyval.source.value = (uintptr_t)node0(0);
        break;
    case 0x7b:
        script_yyval.source.value =
            (uintptr_t)append_node((scr_ast_list_t *)script_yyval.source.value, (void *)script_yyvsp[-1].source.value);
        break;
    case 0x7c:
        local_2ac.source.value = (uintptr_t)node0(0);
        script_yyval.source.value = (uintptr_t)linked_list_end((void *)local_2ac.source.value);
    }
}

int32_t yyparse(void)
{
    int16_t initialStateStack[SCRIPT_YYSTACK_INITIAL_COUNT];
    sval_u initialValueStack[SCRIPT_YYSTACK_INITIAL_COUNT];
    int16_t *stateStackBase = initialStateStack;
    sval_u *valueStackBase = initialValueStack;
    int32_t stackCapacity = SCRIPT_YYSTACK_INITIAL_COUNT;
    int32_t state = SCRIPT_YY_INITIAL_STATE;
    int32_t errorStatus = 0;

    script_yynerrs = 0;
    script_yychar = SCRIPT_YY_EMPTY_LOOKAHEAD;
    script_yyssp = stateStackBase;
    script_yyvsp = valueStackBase;
    stateStackBase[0] = SCRIPT_YY_INITIAL_STATE;

#define SCRIPT_YY_ENSURE_PUSHABLE() \
    do { \
        if (script_yyssp >= &stateStackBase[stackCapacity - 1]) { \
            if (stackCapacity >= SCRIPT_YYSTACK_MAX_COUNT) { \
                yyerror(); \
                return SCRIPT_YY_PARSE_OVERFLOW; \
            } \
            int32_t used = (int32_t)(script_yyssp - stateStackBase) + 1; \
            int32_t newCapacity = stackCapacity * 2; \
            if (newCapacity > SCRIPT_YYSTACK_MAX_COUNT) { \
                newCapacity = SCRIPT_YYSTACK_MAX_COUNT; \
            } \
            int16_t *newStateStack = CODUOMP_ALLOCA((size_t)newCapacity * sizeof(*newStateStack)); \
            sval_u *newValueStack = CODUOMP_ALLOCA((size_t)newCapacity * sizeof(*newValueStack)); \
            __yy_memcpy(newStateStack, stateStackBase, used *(int32_t)sizeof(*newStateStack)); \
            __yy_memcpy(newValueStack, valueStackBase, used *(int32_t)sizeof(*newValueStack)); \
            stateStackBase = newStateStack; \
            valueStackBase = newValueStack; \
            stackCapacity = newCapacity; \
            script_yyssp = stateStackBase + used - 1; \
            script_yyvsp = valueStackBase + used - 1; \
        } \
    } while (0)

    for (;;) {
        int32_t action = script_yypact[state];

        if (action != SCRIPT_YYPACT_NINF) {
            if (script_yychar == SCRIPT_YY_EMPTY_LOOKAHEAD) {
                script_yychar = yylex();
            }

            int32_t token;
            if (script_yychar <= SCRIPT_YY_EOF_TOKEN) {
                script_yychar = SCRIPT_YY_EOF_TOKEN;
                token = SCRIPT_YY_EOF_TOKEN;
            } else if (script_yychar <= SCRIPT_YY_MAX_EXTERNAL_TOKEN) {
                token = script_yytranslate[script_yychar];
            } else {
                token = SCRIPT_YY_UNDEFINED_TRANSLATED_TOKEN;
            }

            int32_t tableIndex = action + token;
            if (ScriptYyCheckMatches(tableIndex, token) != qfalse) {
                action = script_yytable[tableIndex];
                if (action > 0) {
                    if (action == SCRIPT_YY_FINAL_STATE) {
                        return SCRIPT_YY_PARSE_ACCEPT;
                    }

                    SCRIPT_YY_ENSURE_PUSHABLE();
                    state = action;
                    ScriptYyPushValue((int16_t)state, script_yylval, script_yylval.source.sourcePos);
                    script_yychar = SCRIPT_YY_EMPTY_LOOKAHEAD;
                    if (errorStatus > 0) {
                        errorStatus--;
                    }
                    continue;
                }

                if (action < 0 && action != SCRIPT_YYTABLE_NINF) {
                    action = -action;
                    goto reduce;
                }
            }
        }

        action = script_yydefact[state];
        if (action != 0) {
            goto reduce;
        }

        if (errorStatus == 0) {
            yyerror();
            script_yynerrs++;
        }

        if (errorStatus == 3) {
            if (script_yychar == SCRIPT_YY_EOF_TOKEN) {
                return SCRIPT_YY_PARSE_ERROR;
            }
            script_yychar = SCRIPT_YY_EMPTY_LOOKAHEAD;
            continue;
        }

        errorStatus = 3;
        for (;;) {
            int32_t errorIndex = script_yypact[*script_yyssp] + SCRIPT_YY_ERROR_TRANSLATED_TOKEN;
            if (ScriptYyCheckMatches(errorIndex, SCRIPT_YY_ERROR_TRANSLATED_TOKEN) != qfalse) {
                int32_t errorState = script_yytable[errorIndex];
                if (errorState > 0) {
                    SCRIPT_YY_ENSURE_PUSHABLE();
                    state = errorState;
                    ScriptYyPushValue((int16_t)state, script_yylval, script_yylval.source.sourcePos);
                    break;
                }
            }

            if (script_yyssp <= stateStackBase) {
                return SCRIPT_YY_PARSE_ERROR;
            }
            script_yyssp--;
            script_yyvsp--;
        }
        continue;

    reduce: {
        int32_t rule = action;
        int32_t ruleLength = script_yyr2[rule];
        script_yyval = script_yyvsp[1 - ruleLength];
        ScriptYyRunReduction(rule);

        script_yyssp -= ruleLength;
        script_yyvsp -= ruleLength;

        int32_t lhs = script_yyr1[rule];
        int32_t nonterminal = lhs - SCRIPT_YY_FIRST_NONTERMINAL;
        state = *script_yyssp;
        int32_t gotoIndex = script_yypgoto[nonterminal] + state;
        if (ScriptYyCheckMatches(gotoIndex, state) != qfalse) {
            state = script_yytable[gotoIndex];
        } else {
            state = script_yydefgoto[nonterminal];
        }

        SCRIPT_YY_ENSURE_PUSHABLE();
        ScriptYyPushValue((int16_t)state, script_yyval, script_yyval.source.sourcePos);
    }
    }

#undef SCRIPT_YY_ENSURE_PUSHABLE
}
