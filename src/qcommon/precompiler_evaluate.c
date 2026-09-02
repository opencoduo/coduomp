#include "precompiler.h"

#include "compat/coduo_int32_bits.h"
#include "precompiler_float.h"

#include <stddef.h>
#include <string.h>

enum {
    PC_EVAL_MAX_VALUES = 64,
    PC_EVAL_MAX_OPERATORS = 64
};

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "precompiler_evaluate.c requires WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

/* NOT_FROM_ORIGINAL_SOURCE: release copied expression-token lists through one
 * ownership path on every parser result. */
static void coduo_pc_free_evaluation_tokens(token_t **tokens)
{
    while (*tokens != NULL) {
        token_t *const token = *tokens;
        *tokens = token->next;
        PC_FreeToken(token);
    }
}

#if defined(WINDOWS_BEHAVIOR) && defined(__cplusplus)
#define PC_EVAL_ALIGNAS_EIGHT alignas(8)
#elif defined(WINDOWS_BEHAVIOR)
#define PC_EVAL_ALIGNAS_EIGHT _Alignas(8)
#else
#define PC_EVAL_ALIGNAS_EIGHT
#endif

typedef struct value_s value_t;
typedef struct operator_s operator_t;

#if defined(LINUX_BEHAVIOR) && UINTPTR_MAX == UINT32_MAX
/* NOT_FROM_ORIGINAL_SOURCE: MinGW naturally aligns binary64 fields to eight
 * bytes on i386, unlike the original Linux ABI.  Constrain this internal row
 * to the machine-code-proven four-byte alignment in every Linux-behavior
 * i386 build; native Linux already has the same layout. */
#pragma pack(push, 4)
#endif

struct value_s {
    int32_t intValue;
    /* Windows aligns this binary64 slot to +0x08; Linux places it at +0x04.
     * coduo_lnxded 0x0807a9d1..0x0807a9e8 proves the 0x18 Linux stride, and
     * 0x0807ab96..0x0807abbd proves the TBYTE-load/QWORD-store boundary. */
    PC_EVAL_ALIGNAS_EIGHT double floatValue;
    int32_t parentheses;
    value_t *prev;
    value_t *next;
};

#if defined(LINUX_BEHAVIOR) && UINTPTR_MAX == UINT32_MAX
#pragma pack(pop)
#endif

struct operator_s {
    int32_t operator;
    int32_t priority;
    int32_t parentheses;
    operator_t *prev;
    operator_t *next;
};

#undef PC_EVAL_ALIGNAS_EIGHT

#if UINTPTR_MAX == UINT32_MAX
#define PC_ASSERT_I386_EVAL_FIELD(type, member, offset, extent) \
    _Static_assert(offsetof(type, member) == (offset), "i386 evaluator field " #type "." #member " moved"); \
    _Static_assert(sizeof(((type *)0)->member) == (extent), "i386 evaluator field " #type "." #member " extent " \
                                                            "changed")
#if defined(WINDOWS_BEHAVIOR)
_Static_assert(_Alignof(value_t) == 0x08, "original Windows evaluator-value alignment changed");
PC_ASSERT_I386_EVAL_FIELD(value_t, intValue, 0x00, 0x04);
PC_ASSERT_I386_EVAL_FIELD(value_t, floatValue, 0x08, 0x08);
PC_ASSERT_I386_EVAL_FIELD(value_t, parentheses, 0x10, 0x04);
PC_ASSERT_I386_EVAL_FIELD(value_t, prev, 0x14, 0x04);
PC_ASSERT_I386_EVAL_FIELD(value_t, next, 0x18, 0x04);
_Static_assert(sizeof(value_t) == 0x20, "original Windows evaluator value stride is 0x20");
#else
_Static_assert(_Alignof(value_t) == 0x04, "original Linux evaluator-value alignment changed");
PC_ASSERT_I386_EVAL_FIELD(value_t, intValue, 0x00, 0x04);
PC_ASSERT_I386_EVAL_FIELD(value_t, floatValue, 0x04, 0x08);
PC_ASSERT_I386_EVAL_FIELD(value_t, parentheses, 0x0c, 0x04);
PC_ASSERT_I386_EVAL_FIELD(value_t, prev, 0x10, 0x04);
PC_ASSERT_I386_EVAL_FIELD(value_t, next, 0x14, 0x04);
_Static_assert(sizeof(value_t) == 0x18, "original Linux evaluator value stride is 0x18");
#endif
_Static_assert(_Alignof(operator_t) == 0x04, "original evaluator-operator alignment changed");
PC_ASSERT_I386_EVAL_FIELD(operator_t, operator, 0x00, 0x04);
PC_ASSERT_I386_EVAL_FIELD(operator_t, priority, 0x04, 0x04);
PC_ASSERT_I386_EVAL_FIELD(operator_t, parentheses, 0x08, 0x04);
PC_ASSERT_I386_EVAL_FIELD(operator_t, prev, 0x0c, 0x04);
PC_ASSERT_I386_EVAL_FIELD(operator_t, next, 0x10, 0x04);
_Static_assert(sizeof(operator_t) == 0x14, "original evaluator operator stride is 0x14");
#undef PC_ASSERT_I386_EVAL_FIELD
#endif

/* Source: CoDUOMP.exe 0x00444a90..0x00444af0.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00444a90_00444af1.mcode and its
 * compiler-emitted jump/selector tables at 0x00444af4..0x00444b4f.
 * Name: exact same-module Mac symbol PC_OperatorPriority. */
int32_t PC_OperatorPriority(int32_t operatorSubtype)
{
    switch (operatorSubtype) {
    case PC_OPERATOR_LOGICAL_AND:
        return PC_OPERATOR_PRIORITY_LOGICAL_AND;
    case PC_OPERATOR_LOGICAL_OR:
        return PC_OPERATOR_PRIORITY_LOGICAL_OR;
    case PC_OPERATOR_GREATER_OR_EQUAL:
    case PC_OPERATOR_LESS_OR_EQUAL:
    case PC_OPERATOR_GREATER:
    case PC_OPERATOR_LESS:
        return PC_OPERATOR_PRIORITY_RELATIONAL;
    case PC_OPERATOR_EQUAL:
    case PC_OPERATOR_NOT_EQUAL:
        return PC_OPERATOR_PRIORITY_EQUALITY;
    case PC_OPERATOR_SHIFT_RIGHT:
    case PC_OPERATOR_SHIFT_LEFT:
        return PC_OPERATOR_PRIORITY_SHIFT;
    case PC_OPERATOR_MULTIPLY:
    case PC_OPERATOR_DIVIDE:
    case PC_OPERATOR_MODULO:
        return PC_OPERATOR_PRIORITY_MULTIPLICATIVE;
    case PC_OPERATOR_ADD:
    case PC_OPERATOR_SUBTRACT:
        return PC_OPERATOR_PRIORITY_ADDITIVE;
    case PC_OPERATOR_BITWISE_AND:
        return PC_OPERATOR_PRIORITY_BITWISE_AND;
    case PC_OPERATOR_BITWISE_OR:
        return PC_OPERATOR_PRIORITY_BITWISE_OR;
    case PC_OPERATOR_BITWISE_XOR:
        return PC_OPERATOR_PRIORITY_BITWISE_XOR;
    case PC_OPERATOR_BITWISE_NOT:
    case PC_OPERATOR_LOGICAL_NOT:
        return PC_OPERATOR_PRIORITY_UNARY;
    case PC_OPERATOR_TERNARY_COLON:
    case PC_OPERATOR_TERNARY_QUESTION:
        return PC_OPERATOR_PRIORITY_TERNARY;
    default:
        return PC_OPERATOR_PRIORITY_NONE;
    }
}

/* Source: CoDUOMP.exe 0x00444b50..0x004454a9.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00444b50_004454aa.mcode and the two
 * compiler-emitted dispatch-table pairs at 0x004454ac..0x0044556b.
 * The Linux body at 0x0807a7db..0x0807b789 retains the same evaluator and
 * differs only at the proved local-row layout and token-float load boundary.
 * Name: exact same-module Mac symbol PC_EvaluateTokens. */
qboolean PC_EvaluateTokens(source_t *source, token_t *tokens, int32_t *intValue, double *floatValue, qboolean integerEval)
{
    value_t values[PC_EVAL_MAX_VALUES];
    operator_t operators[PC_EVAL_MAX_OPERATORS];
    int32_t valueCount = 0;
    int32_t operatorCount = 0;
    int32_t parentheses = 0;
    qboolean hasValue = qfalse;
    qboolean negative = qfalse;
    qboolean parseError = qfalse;
    value_t *firstValue = NULL;
    value_t *lastValue = NULL;
    operator_t *firstOperator = NULL;
    operator_t *lastOperator = NULL;
    qboolean ternaryActive = qfalse;
    int32_t ternaryIntValue = 0;
    double ternaryFloatValue = 0.0;

    if (intValue != NULL)
        *intValue = 0;
    if (floatValue != NULL)
        *floatValue = 0.0;

    for (token_t *token = tokens; token != NULL; token = token->next) {
        if (token->type == PC_TOKEN_TYPE_NAME) {
            if (hasValue != qfalse || negative != qfalse) {
                SourceError(source, "syntax error in #if/#elif");
                parseError = qtrue;
                break;
            }

            if (strcmp(token->string, "defined") != 0) {
                SourceError(source, "undefined name %s in #if/#elif", token->string);
                parseError = qtrue;
                break;
            }

            qboolean parenthesized = qfalse;
            token = token->next;
            /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
            if (token == NULL) {
                SourceError(source, "defined without name in #if/#elif");
                parseError = qtrue;
                break;
            }
            if (strcmp(token->string, "(") == 0) {
                parenthesized = qtrue;
                token = token->next;
            }

            if (token == NULL || token->type != PC_TOKEN_TYPE_NAME) {
                SourceError(source, "defined without name in #if/#elif");
                parseError = qtrue;
                break;
            }

            if (valueCount >= PC_EVAL_MAX_VALUES) {
                SourceError(source, "out of value space\n");
                parseError = qtrue;
                break;
            }

            value_t *value = &values[valueCount++];
            memset(value, 0, sizeof(*value));
            value->intValue = PC_FindHashedDefine(source->defineHash, token->string) != NULL ? 1 : 0;
            value->floatValue = value->intValue != 0 ? 1.0 : 0.0;
            value->parentheses = parentheses;
            value->prev = lastValue;
            if (lastValue == NULL)
                firstValue = value;
            else
                lastValue->next = value;
            lastValue = value;

            if (parenthesized != qfalse) {
                token = token->next;
                if (token == NULL || strcmp(token->string, ")") != 0) {
                    SourceError(source, "defined without ) in #if/#elif");
                    parseError = qtrue;
                    break;
                }
            }

            hasValue = qtrue;
            continue;
        }

        if (token->type == PC_TOKEN_TYPE_NUMBER) {
            if (hasValue != qfalse) {
                SourceError(source, "syntax error in #if/#elif");
                parseError = qtrue;
                break;
            }

            if (valueCount >= PC_EVAL_MAX_VALUES) {
                SourceError(source, "out of value space\n");
                parseError = qtrue;
                break;
            }

            value_t *value = &values[valueCount++];
            memset(value, 0, sizeof(*value));
            value->intValue = token->intValue;
#if defined(WINDOWS_BEHAVIOR)
            value->floatValue = token->floatValue;
#elif EMULATE_X87
            value->floatValue = x87f_store_f64(coduo_pc_load_token_float80(token->floatValue));
#else
            {
                long double extendedValue = 0.0L;
                const size_t copySize =
                    sizeof(extendedValue) < PC_X87_EXTENDED_TBYTE_SIZE ? sizeof(extendedValue) : PC_X87_EXTENDED_TBYTE_SIZE;
                memcpy(&extendedValue, token->floatValue, copySize);
                value->floatValue = (double)extendedValue;
            }
#endif
            if (negative != qfalse) {
                value->intValue = -value->intValue;
                value->floatValue = -value->floatValue;
            }
            value->parentheses = parentheses;
            value->prev = lastValue;
            if (lastValue == NULL)
                firstValue = value;
            else
                lastValue->next = value;
            lastValue = value;
            hasValue = qtrue;
            negative = qfalse;
            continue;
        }

        if (token->type != PC_TOKEN_TYPE_PUNCTUATION) {
            SourceError(source, "unknown %s in #if/#elif", token->string);
            parseError = qtrue;
            break;
        }

        if (negative != qfalse) {
            SourceError(source, "misplaced minus sign in #if/#elif");
            parseError = qtrue;
            break;
        }

        if (token->subtype == PC_OPERATOR_OPEN_PARENTHESIS) {
            ++parentheses;
            continue;
        }
        if (token->subtype == PC_OPERATOR_CLOSE_PARENTHESIS) {
            --parentheses;
            if (parentheses < 0) {
                SourceError(source, "too many ) in #if/#elsif");
                parseError = qtrue;
                break;
            }
            continue;
        }

        if (integerEval == qfalse && (token->subtype == PC_OPERATOR_BITWISE_NOT || token->subtype == PC_OPERATOR_MODULO ||
                                      token->subtype == PC_OPERATOR_SHIFT_RIGHT || token->subtype == PC_OPERATOR_SHIFT_LEFT ||
                                      token->subtype == PC_OPERATOR_BITWISE_AND || token->subtype == PC_OPERATOR_BITWISE_OR ||
                                      token->subtype == PC_OPERATOR_BITWISE_XOR)) {
            SourceError(source, "illigal operator %s on floating point operands\n", token->string);
            parseError = qtrue;
            break;
        }

        qboolean binaryOperator = qfalse;
        switch (token->subtype) {
        case PC_OPERATOR_LOGICAL_AND:
        case PC_OPERATOR_LOGICAL_OR:
        case PC_OPERATOR_GREATER_OR_EQUAL:
        case PC_OPERATOR_LESS_OR_EQUAL:
        case PC_OPERATOR_EQUAL:
        case PC_OPERATOR_NOT_EQUAL:
        case PC_OPERATOR_SHIFT_RIGHT:
        case PC_OPERATOR_SHIFT_LEFT:
        case PC_OPERATOR_MULTIPLY:
        case PC_OPERATOR_DIVIDE:
        case PC_OPERATOR_MODULO:
        case PC_OPERATOR_ADD:
        case PC_OPERATOR_SUBTRACT:
        case PC_OPERATOR_BITWISE_AND:
        case PC_OPERATOR_BITWISE_OR:
        case PC_OPERATOR_BITWISE_XOR:
        case PC_OPERATOR_GREATER:
        case PC_OPERATOR_LESS:
        case PC_OPERATOR_TERNARY_COLON:
        case PC_OPERATOR_TERNARY_QUESTION:
            binaryOperator = qtrue;
            break;
        default:
            break;
        }

        if (binaryOperator != qfalse) {
            if (hasValue == qfalse) {
                if (token->subtype == PC_OPERATOR_SUBTRACT) {
                    negative = qtrue;
                    continue;
                }
                SourceError(source, "operator %s after operator in #if/#elif", token->string);
                parseError = qtrue;
                break;
            }
        } else if (token->subtype == PC_OPERATOR_INCREMENT || token->subtype == PC_OPERATOR_DECREMENT) {
            /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
            SourceError(source, "++ or -- used in #if/#elif");
            parseError = qtrue;
            break;
        } else if (token->subtype == PC_OPERATOR_BITWISE_NOT || token->subtype == PC_OPERATOR_LOGICAL_NOT) {
            if (hasValue != qfalse) {
                SourceError(source, "! or ~ after value in #if/#elif");
                parseError = qtrue;
                break;
            }
        } else {
            SourceError(source, "invalid operator %s in #if/#elif", token->string);
            parseError = qtrue;
            break;
        }

        if (operatorCount >= PC_EVAL_MAX_OPERATORS) {
            SourceError(source, "out of operator space\n");
            parseError = qtrue;
            break;
        }

        operator_t *op = &operators[operatorCount++];
        memset(op, 0, sizeof(*op));
        op->operator= token->subtype;
        op->priority = PC_OperatorPriority(token->subtype);
        op->parentheses = parentheses;
        op->prev = lastOperator;
        if (lastOperator == NULL)
            firstOperator = op;
        else
            lastOperator->next = op;
        lastOperator = op;
        hasValue = qfalse;
    }

    if (parseError == qfalse) {
        if (hasValue == qfalse) {
            SourceError(source, "trailing operator in #if/#elif");
            parseError = qtrue;
        } else if (parentheses != 0) {
            SourceError(source, "too many ( in #if/#elif");
            parseError = qtrue;
        }
    }

    while (parseError == qfalse && firstOperator != NULL) {
        value_t *value = firstValue;
        operator_t *op = firstOperator;

        while (op->next != NULL && (op->next->parentheses > op->parentheses ||
                                    (op->next->parentheses == op->parentheses && op->next->priority > op->priority))) {
            if (op->operator!= PC_OPERATOR_LOGICAL_NOT && op->operator!= PC_OPERATOR_BITWISE_NOT)
                value = value->next;
            op = op->next;
            if (value == NULL)
                break;
        }

        if (value == NULL) {
            SourceError(source, "mising values in #if/#elif");
            parseError = qtrue;
            break;
        }

        value_t *rhs = value->next;
        if (rhs == NULL && op->operator!= PC_OPERATOR_LOGICAL_NOT && op->operator!= PC_OPERATOR_BITWISE_NOT) {
            /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
            SourceError(source, "mising values in #if/#elif");
            parseError = qtrue;
            break;
        }
        switch (op->operator) {
        case PC_OPERATOR_LOGICAL_AND:
            value->intValue = value->intValue != 0 && rhs->intValue != 0 ? 1 : 0;
            value->floatValue = value->floatValue != 0.0 && rhs->floatValue != 0.0 ? 1.0 : 0.0;
            break;
        case PC_OPERATOR_LOGICAL_OR:
            value->intValue = value->intValue != 0 || rhs->intValue != 0 ? 1 : 0;
            value->floatValue = value->floatValue != 0.0 || rhs->floatValue != 0.0 ? 1.0 : 0.0;
            break;
        case PC_OPERATOR_GREATER_OR_EQUAL:
            value->intValue = value->intValue >= rhs->intValue ? 1 : 0;
            value->floatValue = value->floatValue >= rhs->floatValue ? 1.0 : 0.0;
            break;
        case PC_OPERATOR_LESS_OR_EQUAL:
            value->intValue = value->intValue <= rhs->intValue ? 1 : 0;
            value->floatValue = value->floatValue <= rhs->floatValue ? 1.0 : 0.0;
            break;
        case PC_OPERATOR_EQUAL:
            value->intValue = value->intValue == rhs->intValue ? 1 : 0;
            value->floatValue = value->floatValue == rhs->floatValue ? 1.0 : 0.0;
            break;
        case PC_OPERATOR_NOT_EQUAL:
            value->intValue = value->intValue != rhs->intValue ? 1 : 0;
            value->floatValue = value->floatValue != rhs->floatValue ? 1.0 : 0.0;
            break;
        case PC_OPERATOR_SHIFT_RIGHT:
            value->intValue = coduo_int32_from_bits(coduo_int32_sar_bits(coduo_int32_bits(value->intValue), (uint32_t)rhs->intValue & 31u));
            break;
        case PC_OPERATOR_SHIFT_LEFT:
            value->intValue = coduo_int32_from_bits(coduo_int32_bits(value->intValue) << ((uint32_t)rhs->intValue & 31u));
            break;
        case PC_OPERATOR_MULTIPLY:
            value->intValue = (int32_t)((uint32_t)value->intValue * (uint32_t)rhs->intValue);
            value->floatValue *= rhs->floatValue;
            break;
        case PC_OPERATOR_DIVIDE:
            if (rhs->intValue == 0 || rhs->floatValue == 0.0) {
                SourceError(source, "divide by zero in #if/#elif\n");
                parseError = qtrue;
                break;
            }
            if (value->intValue == INT32_MIN && rhs->intValue == -1) {
                /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
                SourceError(source, "integer divide overflow in #if/#elif\n");
                parseError = qtrue;
                break;
            }
            value->intValue /= rhs->intValue;
            value->floatValue /= rhs->floatValue;
            break;
        case PC_OPERATOR_MODULO:
            if (rhs->intValue == 0) {
                SourceError(source, "divide by zero in #if/#elif\n");
                parseError = qtrue;
                break;
            }
            if (value->intValue == INT32_MIN && rhs->intValue == -1) {
                /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
                SourceError(source, "integer divide overflow in #if/#elif\n");
                parseError = qtrue;
                break;
            }
            value->intValue %= rhs->intValue;
            break;
        case PC_OPERATOR_ADD:
            value->intValue = (int32_t)((uint32_t)value->intValue + (uint32_t)rhs->intValue);
            value->floatValue += rhs->floatValue;
            break;
        case PC_OPERATOR_SUBTRACT:
            value->intValue = (int32_t)((uint32_t)value->intValue - (uint32_t)rhs->intValue);
            value->floatValue -= rhs->floatValue;
            break;
        case PC_OPERATOR_BITWISE_AND:
            value->intValue &= rhs->intValue;
            break;
        case PC_OPERATOR_BITWISE_OR:
            value->intValue |= rhs->intValue;
            break;
        case PC_OPERATOR_BITWISE_XOR:
            value->intValue ^= rhs->intValue;
            break;
        case PC_OPERATOR_BITWISE_NOT:
            value->intValue = ~value->intValue;
            break;
        case PC_OPERATOR_LOGICAL_NOT:
            value->intValue = value->intValue == 0 ? 1 : 0;
            value->floatValue = value->floatValue == 0.0 ? 1.0 : 0.0;
            break;
        case PC_OPERATOR_GREATER:
            value->intValue = value->intValue > rhs->intValue ? 1 : 0;
            value->floatValue = value->floatValue > rhs->floatValue ? 1.0 : 0.0;
            break;
        case PC_OPERATOR_LESS:
            value->intValue = value->intValue < rhs->intValue ? 1 : 0;
            value->floatValue = value->floatValue < rhs->floatValue ? 1.0 : 0.0;
            break;
        case PC_OPERATOR_TERNARY_COLON:
            if (ternaryActive == qfalse) {
                SourceError(source, ": without ? in #if/#elif");
                parseError = qtrue;
                break;
            }
            if (integerEval == qfalse) {
                if (ternaryFloatValue == 0.0)
                    value->floatValue = rhs->floatValue;
            } else if (ternaryIntValue == 0) {
                value->intValue = rhs->intValue;
            }
            ternaryActive = qfalse;
            break;
        case PC_OPERATOR_TERNARY_QUESTION:
            if (ternaryActive != qfalse) {
                SourceError(source, "? after ? in #if/#elif");
                parseError = qtrue;
                break;
            }
            ternaryIntValue = value->intValue;
            ternaryFloatValue = value->floatValue;
            ternaryActive = qtrue;
            break;
        default:
            break;
        }

        if (parseError != qfalse)
            break;

        if (op->operator!= PC_OPERATOR_LOGICAL_NOT && op->operator!= PC_OPERATOR_BITWISE_NOT) {
            value_t *removeValue = op->operator== PC_OPERATOR_TERNARY_QUESTION ? value : rhs;
            if (removeValue->prev == NULL)
                firstValue = removeValue->next;
            else
                removeValue->prev->next = removeValue->next;
            if (removeValue->next == NULL)
                lastValue = removeValue->prev;
            else
                removeValue->next->prev = removeValue->prev;
        }

        if (op->prev == NULL)
            firstOperator = op->next;
        else
            op->prev->next = op->next;
        if (op->next == NULL)
            lastOperator = op->prev;
        else
            op->next->prev = op->prev;
    }

    (void)lastValue;
    (void)lastOperator;
    if (parseError == qfalse) {
        if (intValue != NULL && firstValue != NULL)
            *intValue = firstValue->intValue;
        if (floatValue != NULL && firstValue != NULL)
            *floatValue = firstValue->floatValue;
        return qtrue;
    }

    if (intValue != NULL)
        *intValue = 0;
    if (floatValue != NULL)
        *floatValue = 0.0;
    return qfalse;
}

/* Source: CoDUOMP.exe 0x00445570..0x004457bb.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00445570_004457bc.mcode.
 * Name: exact same-module Mac symbol PC_Evaluate. */
qboolean PC_Evaluate(source_t *source, int32_t *intValue, double *floatValue, qboolean integerEval)
{
    token_t token;
    token_t *firstToken = NULL;
    token_t *lastToken = NULL;
    qboolean definedName = qfalse;
    qboolean evaluated = qfalse;

    if (intValue != NULL)
        *intValue = 0;
    if (floatValue != NULL)
        *floatValue = 0.0;

    if (PC_ReadLine(source, &token) == qfalse) {
        SourceError(source, "no value after #if/#elif");
        return qfalse;
    }

    do {
        if (token.type == PC_TOKEN_TYPE_NAME) {
            if (definedName != qfalse) {
                definedName = qfalse;
                token_t *copy = PC_CopyToken(&token);
                copy->next = NULL;
                if (lastToken == NULL)
                    firstToken = copy;
                else
                    lastToken->next = copy;
                lastToken = copy;
            } else if (strcmp(token.string, "defined") == 0) {
                definedName = qtrue;
                token_t *copy = PC_CopyToken(&token);
                copy->next = NULL;
                if (lastToken == NULL)
                    firstToken = copy;
                else
                    lastToken->next = copy;
                lastToken = copy;
            } else {
                define_t *define = PC_FindHashedDefine(source->defineHash, token.string);
                if (define == NULL) {
                    SourceError(source, "can't evaluate %s, not defined", token.string);
                    goto cleanup;
                }
                if (PC_ExpandDefineIntoSource(source, &token, define) == qfalse)
                    goto cleanup;
            }
        } else if (token.type == PC_TOKEN_TYPE_NUMBER || token.type == PC_TOKEN_TYPE_PUNCTUATION) {
            token_t *copy = PC_CopyToken(&token);
            copy->next = NULL;
            if (lastToken == NULL)
                firstToken = copy;
            else
                lastToken->next = copy;
            lastToken = copy;
        } else {
            SourceError(source, "can't evaluate %s", token.string);
            goto cleanup;
        }
    } while (PC_ReadLine(source, &token) != qfalse);

    if (PC_EvaluateTokens(source, firstToken, intValue, floatValue, integerEval) == qfalse)
        goto cleanup;
    evaluated = qtrue;

cleanup:
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    coduo_pc_free_evaluation_tokens(&firstToken);
    return evaluated;
}

/* Source: CoDUOMP.exe 0x004457c0..0x00445a57.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004457c0_00445a58.mcode.
 * Name: exact same-module Mac symbol PC_DollarEvaluate. */
qboolean PC_DollarEvaluate(source_t *source, int32_t *intValue, double *floatValue, qboolean integerEval)
{
    token_t token;
    token_t *firstToken = NULL;
    token_t *lastToken = NULL;
    int32_t parentheses = 1;
    qboolean definedName = qfalse;
    qboolean evaluated = qfalse;

    if (intValue != NULL)
        *intValue = 0;
    if (floatValue != NULL)
        *floatValue = 0.0;

    if (PC_ReadSourceToken(source, &token) == qfalse) {
        SourceError(source, "no leading ( after $evalint/$evalfloat");
        return qfalse;
    }
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (strcmp(token.string, "(") != 0) {
        SourceError(source, "no leading ( after $evalint/$evalfloat");
        return qfalse;
    }
    if (PC_ReadSourceToken(source, &token) == qfalse) {
        SourceError(source, "nothing to evaluate");
        return qfalse;
    }

    do {
        if (token.type == PC_TOKEN_TYPE_NAME) {
            if (definedName == qfalse) {
                if (strcmp(token.string, "defined") == 0) {
                    definedName = qtrue;
                    token_t *copy = PC_CopyToken(&token);
                    copy->next = NULL;
                    if (lastToken == NULL)
                        firstToken = copy;
                    else
                        lastToken->next = copy;
                    lastToken = copy;
                } else {
                    define_t *define = PC_FindHashedDefine(source->defineHash, token.string);
                    if (define == NULL) {
                        SourceError(source, "can't evaluate %s, not defined", token.string);
                        goto cleanup;
                    }
                    if (PC_ExpandDefineIntoSource(source, &token, define) == qfalse)
                        goto cleanup;
                }
            } else {
                definedName = qfalse;
                token_t *copy = PC_CopyToken(&token);
                copy->next = NULL;
                if (lastToken == NULL)
                    firstToken = copy;
                else
                    lastToken->next = copy;
                lastToken = copy;
            }
        } else {
            if (token.type != PC_TOKEN_TYPE_NUMBER && token.type != PC_TOKEN_TYPE_PUNCTUATION) {
                SourceError(source, "can't evaluate %s", token.string);
                goto cleanup;
            }

            if (strcmp(token.string, "(") == 0)
                ++parentheses;
            else if (strcmp(token.string, ")") == 0)
                --parentheses;

            if (parentheses < 1)
                break;

            token_t *copy = PC_CopyToken(&token);
            copy->next = NULL;
            if (lastToken == NULL)
                firstToken = copy;
            else
                lastToken->next = copy;
            lastToken = copy;
        }
    } while (PC_ReadSourceToken(source, &token) != qfalse);

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    if (parentheses != 0) {
        SourceError(source, "missing ) after $evalint/$evalfloat");
        goto cleanup;
    }

    if (PC_EvaluateTokens(source, firstToken, intValue, floatValue, integerEval) == qfalse)
        goto cleanup;
    evaluated = qtrue;

cleanup:
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    coduo_pc_free_evaluation_tokens(&firstToken);
    return evaluated;
}
