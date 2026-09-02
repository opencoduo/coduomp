#include <stdint.h>

#include "server/standalone/scripting/script_compile_private.h"

enum {
    SCRIPT_YY_EMPTY_LOOKAHEAD = -1,
    SCRIPT_YY_EOF_TOKEN = 0,
    SCRIPT_YY_ERROR_TOKEN = 0x100,
    SCRIPT_YY_INITIAL_STATE = 0,
    SCRIPT_YY_ACCEPT_STATE = 1,
    SCRIPT_YY_PARSE_ACCEPT = 0,
    SCRIPT_YY_PARSE_ERROR = 1,
};

/* NOT_FROM_ORIGINAL_SOURCE: readable bounds check for generated yytable probes. */
static qboolean coduomp_script_yy_table_index_is_valid(int32_t tableIndex)
{
    return tableIndex >= 0 && tableIndex <= SCRIPT_YY_PARSE_TABLE_INDEX_MAX;
}

/* NOT_FROM_ORIGINAL_SOURCE: names the generated yycheck table probe. */
static qboolean coduomp_script_yy_check_matches(int32_t tableIndex, int32_t value)
{
    return coduomp_script_yy_table_index_is_valid(tableIndex) != qfalse && script_yycheck[tableIndex] == value;
}

/* NOT_FROM_ORIGINAL_SOURCE: names the generated parser stack overflow check. */
static qboolean coduomp_script_yy_stack_can_push(void)
{
    return script_yyssp < &script_yyss[SCRIPT_YY_STACK_COUNT - 1];
}

/* NOT_FROM_ORIGINAL_SOURCE: names the generated yacc lookahead load path. */
static int32_t coduomp_script_yy_load_lookahead(void)
{
    if (script_yychar < 0) {
        script_yychar = yylex();
        if (script_yychar < 0) {
            script_yychar = SCRIPT_YY_EOF_TOKEN;
        }
    }

    return script_yychar;
}

/* NOT_FROM_ORIGINAL_SOURCE: names the generated state/value stack push. */
static void coduomp_script_yy_push_value(int16_t state, sval_u value)
{
    script_yyssp++;
    *script_yyssp = state;
    script_yyvsp++;
    *script_yyvsp = value;
}

/* NOT_FROM_ORIGINAL_SOURCE: factors generated yacc reduction actions from yyparse. */
static void coduomp_script_yy_run_reduction(int32_t rule)
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
        coduomp_script_ast_new_script_root(&script_parseRootValue, script_yyvsp[0].words[0]);
        script_parseRoot = (scr_ast_node_t *)script_parseRootValue.words[0];
        break;
    case 2:
        node_pos(&local_10, script_yyval.words[1]);
        node2(&script_yyval, SCR_AST_KIND_PRIMITIVE_EXPRESSION, script_yyval.words[0], local_10.words[0]);
        break;
    case 3:
        node_pos(&local_14, script_yyvsp[-1].words[1]);
        node_pos(&local_18, script_yyvsp[0].words[1]);
        node_pos(&local_1c, script_yyval.words[1]);
        node5(&script_yyval, SCR_AST_KIND_BOOL_OR, script_yyval.words[0], local_1c.words[0], script_yyvsp[0].words[0], local_18.words[0],
              local_14.words[0]);
        script_yyval.words[1] = script_yyvsp[-1].words[1];
        break;
    case 4:
        node_pos(&local_20, script_yyvsp[-1].words[1]);
        node_pos(&local_24, script_yyvsp[0].words[1]);
        node_pos(&local_28, script_yyval.words[1]);
        node5(&script_yyval, SCR_AST_KIND_BOOL_AND, script_yyval.words[0], local_28.words[0], script_yyvsp[0].words[0], local_24.words[0],
              local_20.words[0]);
        script_yyval.words[1] = script_yyvsp[-1].words[1];
        break;
    case 5:
        node_pos(&local_2c, script_yyvsp[-1].words[1]);
        node1_(&local_30, 0x3d);
        node4(&script_yyval, SCR_AST_KIND_BINARY_OPERATOR, script_yyval.words[0], script_yyvsp[0].words[0], local_30.words[0],
              local_2c.words[0]);
        script_yyval.words[1] = script_yyvsp[-1].words[1];
        break;
    case 6:
        node_pos(&local_34, script_yyvsp[-1].words[1]);
        node1_(&local_38, 0x3e);
        node4(&script_yyval, SCR_AST_KIND_BINARY_OPERATOR, script_yyval.words[0], script_yyvsp[0].words[0], local_38.words[0],
              local_34.words[0]);
        script_yyval.words[1] = script_yyvsp[-1].words[1];
        break;
    case 7:
        node_pos(&local_3c, script_yyvsp[-1].words[1]);
        node1_(&local_40, 0x3f);
        node4(&script_yyval, SCR_AST_KIND_BINARY_OPERATOR, script_yyval.words[0], script_yyvsp[0].words[0], local_40.words[0],
              local_3c.words[0]);
        script_yyval.words[1] = script_yyvsp[-1].words[1];
        break;
    case 8:
        node_pos(&local_44, script_yyvsp[-1].words[1]);
        node1_(&local_48, 0x40);
        node4(&script_yyval, SCR_AST_KIND_BINARY_OPERATOR, script_yyval.words[0], script_yyvsp[0].words[0], local_48.words[0],
              local_44.words[0]);
        script_yyval.words[1] = script_yyvsp[-1].words[1];
        break;
    case 9:
        node_pos(&local_4c, script_yyvsp[-1].words[1]);
        node1_(&local_50, 0x41);
        node4(&script_yyval, SCR_AST_KIND_BINARY_OPERATOR, script_yyval.words[0], script_yyvsp[0].words[0], local_50.words[0],
              local_4c.words[0]);
        script_yyval.words[1] = script_yyvsp[-1].words[1];
        break;
    case 10:
        node_pos(&local_54, script_yyvsp[-1].words[1]);
        node1_(&local_58, 0x42);
        node4(&script_yyval, SCR_AST_KIND_BINARY_OPERATOR, script_yyval.words[0], script_yyvsp[0].words[0], local_58.words[0],
              local_54.words[0]);
        script_yyval.words[1] = script_yyvsp[-1].words[1];
        break;
    case 0xb:
        node_pos(&local_5c, script_yyvsp[-1].words[1]);
        node1_(&local_60, 0x43);
        node4(&script_yyval, SCR_AST_KIND_BINARY_OPERATOR, script_yyval.words[0], script_yyvsp[0].words[0], local_60.words[0],
              local_5c.words[0]);
        script_yyval.words[1] = script_yyvsp[-1].words[1];
        break;
    case 0xc:
        node_pos(&local_64, script_yyvsp[-1].words[1]);
        node1_(&local_68, 0x44);
        node4(&script_yyval, SCR_AST_KIND_BINARY_OPERATOR, script_yyval.words[0], script_yyvsp[0].words[0], local_68.words[0],
              local_64.words[0]);
        script_yyval.words[1] = script_yyvsp[-1].words[1];
        break;
    case 0xd:
        node_pos(&local_6c, script_yyvsp[-1].words[1]);
        node1_(&local_70, 0x45);
        node4(&script_yyval, SCR_AST_KIND_BINARY_OPERATOR, script_yyval.words[0], script_yyvsp[0].words[0], local_70.words[0],
              local_6c.words[0]);
        script_yyval.words[1] = script_yyvsp[-1].words[1];
        break;
    case 0xe:
        node_pos(&local_74, script_yyvsp[-1].words[1]);
        node1_(&local_78, 0x46);
        node4(&script_yyval, SCR_AST_KIND_BINARY_OPERATOR, script_yyval.words[0], script_yyvsp[0].words[0], local_78.words[0],
              local_74.words[0]);
        script_yyval.words[1] = script_yyvsp[-1].words[1];
        break;
    case 0xf:
        node_pos(&local_7c, script_yyvsp[-1].words[1]);
        node1_(&local_80, 0x47);
        node4(&script_yyval, SCR_AST_KIND_BINARY_OPERATOR, script_yyval.words[0], script_yyvsp[0].words[0], local_80.words[0],
              local_7c.words[0]);
        script_yyval.words[1] = script_yyvsp[-1].words[1];
        break;
    case 0x10:
        node_pos(&local_84, script_yyvsp[-1].words[1]);
        node1_(&local_88, 0x48);
        node4(&script_yyval, SCR_AST_KIND_BINARY_OPERATOR, script_yyval.words[0], script_yyvsp[0].words[0], local_88.words[0],
              local_84.words[0]);
        script_yyval.words[1] = script_yyvsp[-1].words[1];
        break;
    case 0x11:
        node_pos(&local_8c, script_yyvsp[-1].words[1]);
        node1_(&local_90, 0x49);
        node4(&script_yyval, SCR_AST_KIND_BINARY_OPERATOR, script_yyval.words[0], script_yyvsp[0].words[0], local_90.words[0],
              local_8c.words[0]);
        script_yyval.words[1] = script_yyvsp[-1].words[1];
        break;
    case 0x12:
        node_pos(&local_94, script_yyvsp[-1].words[1]);
        node1_(&local_98, 0x4a);
        node4(&script_yyval, SCR_AST_KIND_BINARY_OPERATOR, script_yyval.words[0], script_yyvsp[0].words[0], local_98.words[0],
              local_94.words[0]);
        script_yyval.words[1] = script_yyvsp[-1].words[1];
        break;
    case 0x13:
        node_pos(&local_9c, script_yyvsp[-1].words[1]);
        node1_(&local_a0, 0x4b);
        node4(&script_yyval, SCR_AST_KIND_BINARY_OPERATOR, script_yyval.words[0], script_yyvsp[0].words[0], local_a0.words[0],
              local_9c.words[0]);
        script_yyval.words[1] = script_yyvsp[-1].words[1];
        break;
    case 0x14:
        node_pos(&local_a4, script_yyvsp[-1].words[1]);
        node1_(&local_a8, 0x4c);
        node4(&script_yyval, SCR_AST_KIND_BINARY_OPERATOR, script_yyval.words[0], script_yyvsp[0].words[0], local_a8.words[0],
              local_a4.words[0]);
        script_yyval.words[1] = script_yyvsp[-1].words[1];
        break;
    case 0x15:
        node_pos(&local_ac, script_yyvsp[0].words[1]);
        node2(&script_yyval, SCR_AST_KIND_CAST_BOOL, script_yyvsp[0].words[0], local_ac.words[0]);
        break;
    case 0x16:
        node_pos(&local_b0, script_yyvsp[0].words[1]);
        node2(&script_yyval, SCR_AST_KIND_CAST_INT, script_yyvsp[0].words[0], local_b0.words[0]);
        break;
    case 0x17:
        node_pos(&local_b4, script_yyvsp[0].words[1]);
        node2(&script_yyval, SCR_AST_KIND_CAST_FLOAT, script_yyvsp[0].words[0], local_b4.words[0]);
        break;
    case 0x18:
        node_pos(&local_b8, script_yyvsp[0].words[1]);
        node2(&script_yyval, SCR_AST_KIND_CAST_STRING, script_yyvsp[0].words[0], local_b8.words[0]);
        break;
    case 0x19:
        node_pos(&local_bc, script_yyvsp[0].words[1]);
        node2(&script_yyval, SCR_AST_KIND_BOOL_NOT, script_yyvsp[0].words[0], local_bc.words[0]);
        break;
    case 0x1a:
        node_pos(&local_c0, script_yyvsp[0].words[1]);
        node2(&script_yyval, SCR_AST_KIND_BOOL_COMPLEMENT, script_yyvsp[0].words[0], local_c0.words[0]);
        break;
    case 0x1b:
        node1(&script_yyval, SCR_AST_KIND_FOR_CONDITION, script_yyval.words[0]);
        break;
    case 0x1c:
        node0(&script_yyval, 0);
        break;
    case 0x1f:
        node_pos(&local_c4, script_yyval.words[1]);
        node3(&script_yyval, SCR_AST_KIND_SCRIPT_FUNCTION_REF, script_yyval.words[0], script_yyvsp[0].words[0], local_c4.words[0]);
        script_pendingScriptLoadCount++;
        break;
    case 0x20:
        node_pos(&local_c8, script_yyval.words[1]);
        node2(&script_yyval, SCR_AST_KIND_FUNCTION_REF, script_yyval.words[0], local_c8.words[0]);
        break;
    case 0x21:
        node_pos(&local_cc, script_yyval.words[1]);
        node3(&script_yyval, SCR_AST_KIND_SCRIPT_FUNCTION_REF, script_yyval.words[0], script_yyvsp[0].words[0], local_cc.words[0]);
        script_yyval.words[1] = script_yyvsp[-1].words[1];
        script_pendingScriptLoadCount++;
        break;
    case 0x22:
        node_pos(&local_d0, script_yyval.words[1]);
        node2(&script_yyval, SCR_AST_KIND_FUNCTION_REF, script_yyvsp[0].words[0], local_d0.words[0]);
        break;
    case 0x23:
        node_pos(&local_d4, script_yyval.words[1]);
        node2(&script_yyval, SCR_AST_KIND_SCRIPT_FUNCTION_NAME, script_yyval.words[0], local_d4.words[0]);
        break;
    case 0x24:
        node_pos(&local_d8, script_yyvsp[-2].words[1]);
        node2(&script_yyval, SCR_AST_KIND_FUNCTION_POINTER_CALL, script_yyvsp[-2].words[0], local_d8.words[0]);
        break;
    case 0x25:
        node_pos(&local_dc, script_yyval.words[1]);
        node2(&script_yyval, SCR_AST_KIND_FUNCTION_CALL, script_yyval.words[0], local_dc.words[0]);
        break;
    case 0x26:
        node_pos(&local_e0, script_yyvsp[0].words[1]);
        node_pos(&local_e4, script_yyval.words[1]);
        node3(&script_yyval, SCR_AST_KIND_METHOD_CALL, script_yyvsp[0].words[0], local_e4.words[0], local_e0.words[0]);
        script_yyval.words[1] = script_yyvsp[0].words[1];
        break;
    case 0x27:
        node_pos(&local_e8, script_yyvsp[-2].words[1]);
        node3(&script_yyval, SCR_AST_KIND_FUNCTION_CALL_VALUE, script_yyval.words[0], script_yyvsp[-1].words[0], local_e8.words[0]);
        script_yyval.words[1] = script_yyvsp[-2].words[1];
        break;
    case 0x28:
        node_pos(&local_ec, script_yyvsp[-2].words[1]);
        node_pos(&local_f0, script_yyval.words[1]);
        node5(&script_yyval, SCR_AST_KIND_METHOD_CALL_VALUE, script_yyval.words[0], script_yyvsp[-3].words[0], script_yyvsp[-1].words[0],
              local_f0.words[0], local_ec.words[0]);
        script_yyval.words[1] = script_yyvsp[-2].words[1];
        break;
    case 0x29:
        node_pos(&local_f4, script_yyval.words[1]);
        node2(&script_yyval, SCR_AST_KIND_EXPRESSION_LIST, script_yyvsp[-1].words[0], local_f4.words[0]);
        break;
    case 0x2a:
        node_pos(&local_f8, script_yyval.words[1]);
        node2(&script_yyval, SCR_AST_KIND_INTEGER_LITERAL, script_yyval.words[0], local_f8.words[0]);
        break;
    case 0x2b:
        node_pos(&local_fc, script_yyval.words[1]);
        node2(&script_yyval, SCR_AST_KIND_FLOAT_LITERAL, script_yyval.words[0], local_fc.words[0]);
        break;
    case 0x2c:
        node_pos(&local_100, script_yyval.words[1]);
        node2(&script_yyval, SCR_AST_KIND_NEGATED_INTEGER_LITERAL, script_yyvsp[0].words[0], local_100.words[0]);
        break;
    case 0x2d:
        node_pos(&local_104, script_yyval.words[1]);
        node2(&script_yyval, SCR_AST_KIND_NEGATED_FLOAT_LITERAL, script_yyvsp[0].words[0], local_104.words[0]);
        break;
    case 0x2e:
        node_pos(&local_108, script_yyval.words[1]);
        node2(&script_yyval, SCR_AST_KIND_STRING, script_yyval.words[0], local_108.words[0]);
        break;
    case 0x2f:
        node_pos(&local_10c, script_yyval.words[1]);
        node2(&script_yyval, SCR_AST_KIND_ISTRING, script_yyval.words[0], local_10c.words[0]);
        break;
    case 0x30:
        node1(&script_yyval, SCR_AST_KIND_CALL_VALUE, script_yyval.words[0]);
        break;
    case 0x31:
        node_pos(&local_110, script_yyval.words[1]);
        node2(&script_yyval, SCR_AST_KIND_REFERENCE_EXPRESSION, script_yyval.words[0], local_110.words[0]);
        break;
    case 0x32:
        node_pos(&local_114, script_yyval.words[1]);
        node1(&script_yyval, SCR_AST_KIND_UNDEFINED, local_114.words[0]);
        break;
    case 0x33:
        node_pos(&local_118, script_yyval.words[1]);
        node1(&script_yyval, SCR_AST_KIND_SELF, local_118.words[0]);
        break;
    case 0x34:
        node_pos(&local_11c, script_yyval.words[1]);
        node1(&script_yyval, SCR_AST_KIND_LEVEL, local_11c.words[0]);
        break;
    case 0x35:
        node_pos(&local_120, script_yyval.words[1]);
        node1(&script_yyval, SCR_AST_KIND_ANIM, local_120.words[0]);
        break;
    case 0x36:
        node_pos(&local_124, script_yyval.words[1]);
        node1(&script_yyval, SCR_AST_KIND_GAME, local_124.words[0]);
        break;
    case 0x37:
        node_pos(&local_128, script_yyval.words[1]);
        node2(&script_yyval, SCR_AST_KIND_SIZE, script_yyval.words[0], local_128.words[0]);
        script_yyval.words[1] = script_yyvsp[0].words[1];
        break;
    case 0x38:
        node_pos(&local_12c, script_yyval.words[1]);
        node2(&script_yyval, SCR_AST_KIND_SCRIPT_FUNCTION_NAME, script_yyval.words[0], local_12c.words[0]);
        break;
    case 0x39:
        node0(&script_yyval, 0x40);
        break;
    case 0x3a:
        node_pos(&local_130, script_yyvsp[0].words[1]);
        node2(&script_yyval, SCR_AST_KIND_ANIMATION, script_yyvsp[0].words[0], local_130.words[0]);
        break;
    case 0x3b:
        node_pos(&local_134, script_yyval.words[1]);
        node2(&script_yyval, SCR_AST_KIND_FALSE, script_yyval.words[0], local_134.words[0]);
        break;
    case 0x3c:
        node_pos(&local_138, script_yyval.words[1]);
        node2(&script_yyval, SCR_AST_KIND_TRUE, script_yyval.words[0], local_138.words[0]);
        break;
    case 0x3d:
        node_pos(&local_13c, script_yyval.words[1]);
        node1(&script_yyval, SCR_AST_KIND_ANIMTREE, local_13c.words[0]);
        break;
    case 0x3e:
        node_pos(&local_140, script_yyvsp[0].words[1]);
        node_pos(&local_144, script_yyval.words[1]);
        node4(&script_yyval, SCR_AST_KIND_OBJECT_STRING_REF, script_yyval.words[0], script_yyvsp[0].words[0], local_144.words[0],
              local_140.words[0]);
        script_yyval.words[1] = script_yyvsp[0].words[1];
        break;
    case 0x3f:
        node_pos(&local_148, script_yyvsp[-1].words[1]);
        node_pos(&local_14c, script_yyval.words[1]);
        node4(&script_yyval, SCR_AST_KIND_OBJECT_INDEX_OBJECT_REF, script_yyval.words[0], script_yyvsp[-1].words[0], local_14c.words[0],
              local_148.words[0]);
        script_yyval.words[1] = script_yyvsp[-2].words[1];
        break;
    case 0x40:
        node_pos(&local_150, script_yyval.words[1]);
        node2(&script_yyval, SCR_AST_KIND_STRING_REF, script_yyval.words[0], local_150.words[0]);
        break;
    case 0x41:
        node1(&script_yyval, SCR_AST_KIND_CALL_STATEMENT, script_yyval.words[0]);
        break;
    case 0x42:
        node_pos(&local_154, script_yyvsp[-1].words[1]);
        node3(&script_yyval, SCR_AST_KIND_ASSIGNMENT_STATEMENT, script_yyval.words[0], script_yyvsp[0].words[0], local_154.words[0]);
        break;
    case 0x43:
        node_pos(&local_158, script_yyval.words[1]);
        node2(&script_yyval, SCR_AST_KIND_RETURN_VALUE_STATEMENT, script_yyvsp[0].words[0], local_158.words[0]);
        break;
    case 0x44:
        node_pos(&local_15c, script_yyval.words[1]);
        node1(&script_yyval, SCR_AST_KIND_RETURN_STATEMENT, local_15c.words[0]);
        break;
    case 0x45:
        node_pos(&local_160, script_yyval.words[1]);
        node_pos(&local_164, script_yyvsp[0].words[1]);
        node3(&script_yyval, SCR_AST_KIND_WAIT_STATEMENT, script_yyvsp[0].words[0], local_164.words[0], local_160.words[0]);
        break;
    case 0x46:
        node_pos(&local_168, script_yyval.words[1]);
        node2(&script_yyval, SCR_AST_KIND_INC_STATEMENT, script_yyval.words[0], local_168.words[0]);
        break;
    case 0x47:
        node_pos(&local_16c, script_yyval.words[1]);
        node2(&script_yyval, SCR_AST_KIND_DEC_STATEMENT, script_yyval.words[0], local_16c.words[0]);
        break;
    case 0x48:
        node_pos(&local_170, script_yyvsp[-1].words[1]);
        node1_(&local_174, 0x3d);
        node4(&script_yyval, SCR_AST_KIND_REF_ASSIGNMENT_STATEMENT, script_yyval.words[0], script_yyvsp[0].words[0], local_174.words[0],
              local_170.words[0]);
        break;
    case 0x49:
        node_pos(&local_178, script_yyvsp[-1].words[1]);
        node1_(&local_17c, 0x3e);
        node4(&script_yyval, SCR_AST_KIND_REF_ASSIGNMENT_STATEMENT, script_yyval.words[0], script_yyvsp[0].words[0], local_17c.words[0],
              local_178.words[0]);
        break;
    case 0x4a:
        node_pos(&local_180, script_yyvsp[-1].words[1]);
        node1_(&local_184, 0x3f);
        node4(&script_yyval, SCR_AST_KIND_REF_ASSIGNMENT_STATEMENT, script_yyval.words[0], script_yyvsp[0].words[0], local_184.words[0],
              local_180.words[0]);
        break;
    case 0x4b:
        node_pos(&local_188, script_yyvsp[-1].words[1]);
        node1_(&local_18c, 0x46);
        node4(&script_yyval, SCR_AST_KIND_REF_ASSIGNMENT_STATEMENT, script_yyval.words[0], script_yyvsp[0].words[0], local_18c.words[0],
              local_188.words[0]);
        break;
    case 0x4c:
        node_pos(&local_190, script_yyvsp[-1].words[1]);
        node1_(&local_194, 0x47);
        node4(&script_yyval, SCR_AST_KIND_REF_ASSIGNMENT_STATEMENT, script_yyval.words[0], script_yyvsp[0].words[0], local_194.words[0],
              local_190.words[0]);
        break;
    case 0x4d:
        node_pos(&local_198, script_yyvsp[-1].words[1]);
        node1_(&local_19c, 0x48);
        node4(&script_yyval, SCR_AST_KIND_REF_ASSIGNMENT_STATEMENT, script_yyval.words[0], script_yyvsp[0].words[0], local_19c.words[0],
              local_198.words[0]);
        break;
    case 0x4e:
        node_pos(&local_1a0, script_yyvsp[-1].words[1]);
        node1_(&local_1a4, 0x49);
        node4(&script_yyval, SCR_AST_KIND_REF_ASSIGNMENT_STATEMENT, script_yyval.words[0], script_yyvsp[0].words[0], local_1a4.words[0],
              local_1a0.words[0]);
        break;
    case 0x4f:
        node_pos(&local_1a8, script_yyvsp[-1].words[1]);
        node1_(&local_1ac, 0x4a);
        node4(&script_yyval, SCR_AST_KIND_REF_ASSIGNMENT_STATEMENT, script_yyval.words[0], script_yyvsp[0].words[0], local_1ac.words[0],
              local_1a8.words[0]);
        break;
    case 0x50:
        node_pos(&local_1b0, script_yyvsp[-1].words[1]);
        node1_(&local_1b4, 0x4b);
        node4(&script_yyval, SCR_AST_KIND_REF_ASSIGNMENT_STATEMENT, script_yyval.words[0], script_yyvsp[0].words[0], local_1b4.words[0],
              local_1b0.words[0]);
        break;
    case 0x51:
        node_pos(&local_1b8, script_yyvsp[-1].words[1]);
        node1_(&local_1bc, 0x4c);
        node4(&script_yyval, SCR_AST_KIND_REF_ASSIGNMENT_STATEMENT, script_yyval.words[0], script_yyvsp[0].words[0], local_1bc.words[0],
              local_1b8.words[0]);
        break;
    case 0x52:
        node_pos(&local_1c0, script_yyvsp[-3].words[1]);
        node_pos(&local_1c4, script_yyval.words[1]);
        node4(&script_yyval, SCR_AST_KIND_WAITTILL_STATEMENT, script_yyval.words[0], script_yyvsp[-1].words[0], local_1c4.words[0],
              local_1c0.words[0]);
        break;
    case 0x53:
        node_pos(&local_1c8, script_yyvsp[-3].words[1]);
        node_pos(&local_1cc, script_yyval.words[1]);
        node4(&script_yyval, SCR_AST_KIND_WAITTILLMATCH_STATEMENT, script_yyval.words[0], script_yyvsp[-1].words[0], local_1cc.words[0],
              local_1c8.words[0]);
        break;
    case 0x54:
        node_pos(&local_1d0, script_yyvsp[-3].words[1]);
        node_pos(&local_1d4, script_yyval.words[1]);
        node4(&script_yyval, SCR_AST_KIND_NOTIFY_STATEMENT, script_yyval.words[0], script_yyvsp[-1].words[0], local_1d4.words[0],
              local_1d0.words[0]);
        break;
    case 0x55:
        node_pos(&local_1d8, script_yyvsp[-1].words[1]);
        node_pos(&local_1dc, script_yyval.words[1]);
        node4(&script_yyval, SCR_AST_KIND_ENDON_STATEMENT, script_yyval.words[0], script_yyvsp[-1].words[0], local_1dc.words[0],
              local_1d8.words[0]);
        break;
    case 0x56:
        node_pos(&local_1e0, script_yyval.words[1]);
        node1(&script_yyval, SCR_AST_KIND_BREAK_STATEMENT, local_1e0.words[0]);
        break;
    case 0x57:
        node_pos(&local_1e4, script_yyval.words[1]);
        node1(&script_yyval, SCR_AST_KIND_CONTINUE_STATEMENT, local_1e4.words[0]);
        break;
    case 0x58:
        node_pos(&local_1e8, script_yyvsp[-3].words[1]);
        node_pos(&local_1ec, script_yyvsp[-1].words[1]);
        node4(&script_yyval, SCR_AST_KIND_DO_WHILE_STATEMENT, script_yyvsp[-4].words[0], script_yyvsp[-1].words[0], local_1ec.words[0],
              local_1e8.words[0]);
        break;
    case 0x59:
        node0(&script_yyval, 0);
        break;
    case 0x5c:
        node_pos(&local_1f0, script_yyval.words[1]);
        node2(&script_yyval, SCR_AST_KIND_STATEMENT_BLOCK, script_yyvsp[-1].words[0], local_1f0.words[0]);
        break;
    case 0x5d:
        node_pos(&local_1f4, script_yyvsp[-2].words[1]);
        node3(&script_yyval, SCR_AST_KIND_IF_STATEMENT, script_yyvsp[-2].words[0], script_yyvsp[0].words[0], local_1f4.words[0]);
        break;
    case 0x5e:
        node_pos(&local_1f8, script_yyvsp[-4].words[1]);
        node4(&script_yyval, SCR_AST_KIND_IF_ELSE_STATEMENT, script_yyvsp[-4].words[0], script_yyvsp[-2].words[0], script_yyvsp[0].words[0],
              local_1f8.words[0]);
        break;
    case 0x5f:
        node_pos(&local_1fc, script_yyval.words[1]);
        node_pos(&local_200, script_yyvsp[-2].words[1]);
        node4(&script_yyval, SCR_AST_KIND_WHILE_STATEMENT, script_yyvsp[-2].words[0], script_yyvsp[0].words[0], local_200.words[0],
              local_1fc.words[0]);
        break;
    case 0x60:
        node_pos(&local_204, script_yyval.words[1]);
        node_pos(&local_208, script_yyvsp[-4].words[1]);
        node6(&script_yyval, SCR_AST_KIND_FOR_STATEMENT, script_yyvsp[-5].words[0], script_yyvsp[-4].words[0], script_yyvsp[-2].words[0],
              script_yyvsp[0].words[0], local_208.words[0], local_204.words[0]);
        break;
    case 0x61:
        node_pos(&local_20c, script_yyvsp[-2].words[1]);
        node3(&script_yyval, SCR_AST_KIND_SWITCH_STATEMENT, script_yyvsp[-2].words[0], script_yyvsp[0].words[0], local_20c.words[0]);
        break;
    case 0x62:
        node_pos(&local_210, script_yyval.words[1]);
        node2(&script_yyval, SCR_AST_KIND_DEVELOPER_STATEMENT_BLOCK, script_yyvsp[-1].words[0], local_210.words[0]);
        break;
    case 99:
        node0(&script_yyval, 0);
        break;
    case 100:
        node_pos(&local_214, script_yyval.words[1]);
        node2(&script_yyval, SCR_AST_KIND_CASE_STATEMENT, script_yyvsp[-1].words[0], local_214.words[0]);
        break;
    case 0x65:
        node_pos(&local_218, script_yyval.words[1]);
        node1(&script_yyval, SCR_AST_KIND_DEFAULT_STATEMENT, local_218.words[0]);
        break;
    case 0x67:
        append_node(&script_yyval, script_yyval.words[0], script_yyvsp[0].words[0]);
        break;
    case 0x68:
        node0(&local_21c, 0);
        linked_list_end(&script_yyval, local_21c.words[0]);
        break;
    case 0x69:
        node_pos(&local_224, script_yyvsp[0].words[1]);
        node2_(&local_220, script_yyvsp[0].words[0], local_224.words[0]);
        prepend_node(&script_yyval, local_220.words[0], script_yyval.words[0]);
        break;
    case 0x6a:
        node0(&local_228, 0);
        node_pos(&local_230, script_yyval.words[1]);
        node2_(&local_22c, script_yyval.words[0], local_230.words[0]);
        prepend_node(&script_yyval, local_22c.words[0], local_228.words[0]);
        break;
    case 0x6b:
        node0(&script_yyval, 0);
        break;
    case 0x6c:
        node_pos(&local_238, script_yyvsp[0].words[1]);
        node2_(&local_234, script_yyvsp[0].words[0], local_238.words[0]);
        append_node(&script_yyval, script_yyval.words[0], local_234.words[0]);
        break;
    case 0x6d:
        node_pos(&local_240, script_yyval.words[1]);
        node2_(&local_23c, script_yyval.words[0], local_240.words[0]);
        node0(&local_248, 0);
        linked_list_end(&local_244, local_248.words[0]);
        append_node(&script_yyval, local_244.words[0], local_23c.words[0]);
        break;
    case 0x6e:
        node0(&local_24c, 0);
        linked_list_end(&script_yyval, local_24c.words[0]);
        break;
    case 0x6f:
        node_pos(&local_254, script_yyvsp[0].words[1]);
        node2_(&local_250, script_yyvsp[0].words[0], local_254.words[0]);
        append_node(&script_yyval, script_yyval.words[0], local_250.words[0]);
        break;
    case 0x70:
        node_pos(&local_25c, script_yyval.words[1]);
        node2_(&local_258, script_yyval.words[0], local_25c.words[0]);
        node0(&local_264, 0);
        linked_list_end(&local_260, local_264.words[0]);
        append_node(&script_yyval, local_260.words[0], local_258.words[0]);
        break;
    case 0x71:
        node_pos(&local_26c, script_yyvsp[0].words[1]);
        node2_(&local_268, script_yyvsp[0].words[0], local_26c.words[0]);
        append_node(&script_yyval, script_yyval.words[0], local_268.words[0]);
        break;
    case 0x72:
        node_pos(&local_274, script_yyval.words[1]);
        node2_(&local_270, script_yyval.words[0], local_274.words[0]);
        node0(&local_27c, 0);
        linked_list_end(&local_278, local_27c.words[0]);
        append_node(&script_yyval, local_278.words[0], local_270.words[0]);
        break;
    case 0x73:
        node_pos(&local_284, script_yyvsp[0].words[1]);
        node2_(&local_280, script_yyvsp[0].words[0], local_284.words[0]);
        prepend_node(&script_yyval, local_280.words[0], script_yyval.words[0]);
        break;
    case 0x74:
        node0(&local_288, 0);
        node_pos(&local_290, script_yyval.words[1]);
        node2_(&local_28c, script_yyval.words[0], local_290.words[0]);
        prepend_node(&script_yyval, local_28c.words[0], local_288.words[0]);
        break;
    case 0x75:
        node_pos(&local_294, script_yyval.words[1]);
        node4(&script_yyval, SCR_AST_KIND_FUNCTION_DEFINITION, script_yyval.words[0], script_yyvsp[-4].words[0], script_yyvsp[-1].words[0],
              local_294.words[0]);
        break;
    case 0x76:
        node_pos(&local_298, script_yyvsp[-7].words[1]);
        node4(&script_yyval, SCR_AST_KIND_DEVELOPER_FUNCTION_DEFINITION, script_yyvsp[-7].words[0], script_yyvsp[-5].words[0],
              script_yyvsp[-2].words[0], local_298.words[0]);
        break;
    case 0x77:
        node_pos(&local_29c, script_yyvsp[-2].words[1]);
        node2(&script_yyval, SCR_AST_KIND_USING_ANIMTREE, script_yyvsp[-2].words[0], local_29c.words[0]);
        break;
    case 0x78:
        node_pos(&local_2a4, script_yyvsp[0].words[1]);
        node2_(&local_2a0, script_yyvsp[0].words[0], local_2a4.words[0]);
        append_node(&script_yyval, script_yyval.words[0], local_2a0.words[0]);
        break;
    case 0x79:
        node0(&local_2a8, 0);
        linked_list_end(&script_yyval, local_2a8.words[0]);
        break;
    case 0x7a:
        node0(&script_yyval, 0);
        break;
    case 0x7b:
        append_node(&script_yyval, script_yyval.words[0], script_yyvsp[-1].words[0]);
        break;
    case 0x7c:
        node0(&local_2ac, 0);
        linked_list_end(&script_yyval, local_2ac.words[0]);
    }
}

int32_t yyparse(void)
{
    int32_t state = SCRIPT_YY_INITIAL_STATE;

    script_yynerrs = 0;
    script_yyerrflag = 0;
    script_yychar = SCRIPT_YY_EMPTY_LOOKAHEAD;
    script_yyssp = script_yyss;
    script_yyvsp = script_yyvs;
    script_yyss[0] = SCRIPT_YY_INITIAL_STATE;

    while (qtrue) {
        int32_t rule = script_yydef[state];

        if (rule == 0) {
            int32_t lookahead = coduomp_script_yy_load_lookahead();
            int32_t tableIndex = script_yysindex[state];

            if (tableIndex != 0) {
                tableIndex += lookahead;
                if (coduomp_script_yy_check_matches(tableIndex, lookahead) != qfalse) {
                    int16_t nextState = script_yytable[tableIndex];
                    if (coduomp_script_yy_stack_can_push() == qfalse) {
                        yyerror();
                        return SCRIPT_YY_PARSE_ERROR;
                    }

                    state = nextState;
                    coduomp_script_yy_push_value(nextState, script_yylval);
                    script_yychar = SCRIPT_YY_EMPTY_LOOKAHEAD;
                    if (script_yyerrflag > 0) {
                        script_yyerrflag--;
                    }
                    continue;
                }
            }

            tableIndex = script_yyrindex[state];
            if (tableIndex == 0 || coduomp_script_yy_check_matches(tableIndex + lookahead, lookahead) == qfalse) {
                if (script_yyerrflag == 0) {
                    yyerror();
                    script_yynerrs++;
                }

                if (script_yyerrflag < 3) {
                    script_yyerrflag = 3;
                    while (qtrue) {
                        tableIndex = script_yysindex[*script_yyssp];
                        if (tableIndex != 0 &&
                            coduomp_script_yy_check_matches(tableIndex + SCRIPT_YY_ERROR_TOKEN, SCRIPT_YY_ERROR_TOKEN) != qfalse) {
                            break;
                        }

                        if (script_yyssp <= script_yyss) {
                            script_yyerrflag = 3;
                            return SCRIPT_YY_PARSE_ERROR;
                        }

                        script_yyssp--;
                        script_yyvsp--;
                    }

                    if (coduomp_script_yy_stack_can_push() == qfalse) {
                        yyerror();
                        return SCRIPT_YY_PARSE_ERROR;
                    }

                    tableIndex += SCRIPT_YY_ERROR_TOKEN;
                    state = script_yytable[tableIndex];
                    coduomp_script_yy_push_value((int16_t)state, script_yylval);
                } else {
                    if (script_yychar == SCRIPT_YY_EOF_TOKEN) {
                        return SCRIPT_YY_PARSE_ERROR;
                    }
                    script_yychar = SCRIPT_YY_EMPTY_LOOKAHEAD;
                }
                continue;
            }

            rule = script_yytable[tableIndex + lookahead];
        }

        int32_t ruleLength = script_yylen[rule];
        script_yyval = script_yyvsp[1 - ruleLength];
        coduomp_script_yy_run_reduction(rule);

        script_yyssp -= ruleLength;
        script_yyvsp -= ruleLength;

        int32_t lhs = script_yylhs[rule];
        state = *script_yyssp;
        if (state == SCRIPT_YY_INITIAL_STATE && lhs == 0) {
            if (coduomp_script_yy_stack_can_push() == qfalse) {
                yyerror();
                return SCRIPT_YY_PARSE_ERROR;
            }

            state = SCRIPT_YY_ACCEPT_STATE;
            coduomp_script_yy_push_value(SCRIPT_YY_ACCEPT_STATE, script_yyval);
            coduomp_script_yy_load_lookahead();
            if (script_yychar == SCRIPT_YY_EOF_TOKEN) {
                return SCRIPT_YY_PARSE_ACCEPT;
            }
            continue;
        }

        int32_t tableIndex = script_yygindex[lhs];
        int16_t nextState;
        if (tableIndex == 0 || coduomp_script_yy_check_matches(tableIndex + state, state) == qfalse) {
            nextState = script_yydgoto[lhs];
        } else {
            nextState = script_yytable[tableIndex + state];
        }

        if (coduomp_script_yy_stack_can_push() == qfalse) {
            yyerror();
            return SCRIPT_YY_PARSE_ERROR;
        }

        state = nextState;
        coduomp_script_yy_push_value(nextState, script_yyval);
    }
}
