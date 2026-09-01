#include "client/engine/scripting/script_compile.h"

#include <stddef.h>

enum {
    SCRIPT_PARSE_HUNK_ALIGNMENT = 16,
    SCRIPT_PARSE_HUNK_ALIGNMENT_MASK = SCRIPT_PARSE_HUNK_ALIGNMENT - 1,
};

/* NOT_FROM_ORIGINAL_SOURCE: source factoring for the identical inlined
 * Hunk_AllocateTempMemoryHighInternal sequence in the parser's node constructors. */
void *coduomp_script_parse_allocate(size_t size)
{
    size_t newHighTempBytes =
        (hunk.highTemp + size +
         SCRIPT_PARSE_HUNK_ALIGNMENT_MASK) &
        ~(size_t)SCRIPT_PARSE_HUNK_ALIGNMENT_MASK;
    hunk.highTemp = newHighTempBytes;

    if ((int32_t)(hunk.lowTemp + newHighTempBytes) >
        (int32_t)hunk.totalSize) {
        Com_Meminfo_f();
        Com_Error(ERR_DROP,
                  "\x15Hunk_AllocateTempMemoryHigh: failed on %i",
                  (int32_t)size);
    }

    return hunk_data + hunk.totalSize - newHighTempBytes;
}

/* Source: CoDUOMP.exe 0x00482050..0x004820cc.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00482050_004820cd.mcode. */
scr_ast_node_t *node6(uintptr_t word0, uintptr_t word1, uintptr_t word2,
                      uintptr_t word3, uintptr_t word4, uintptr_t word5,
                      uintptr_t word6)
{
    scr_ast_node_t *node = coduomp_script_parse_allocate(
        offsetof(scr_ast_node_t, payload) +
        sizeof(node->payload.forStatement));

    node->kind = (scr_ast_kind_t)word0;
    node->payload.forStatement.initNode = (scr_ast_node_t *)word1;
    node->payload.forStatement.conditionNode = (scr_ast_node_t *)word2;
    node->payload.forStatement.incrementNode = (scr_ast_node_t *)word3;
    node->payload.forStatement.bodyNode = (scr_ast_node_t *)word4;
    node->payload.forStatement.conditionSourcePos = (uint32_t)word5;
    node->payload.forStatement.loopSourcePos = (uint32_t)word6;
    return node;
}

/* Source: CoDUOMP.exe 0x004820d0..0x0048217a.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004820d0_0048217b.mcode. */
scr_ast_list_t *linked_list_end(void *entry)
{
    scr_ast_list_item_t *item =
        coduomp_script_parse_allocate(sizeof(*item));
    scr_ast_list_t *list;

    item->entry = entry;
    item->next = NULL;

    list = coduomp_script_parse_allocate(sizeof(*list));
    list->head = item;
    list->tail = item;
    return list;
}

/* Source: CoDUOMP.exe 0x00482180..0x004821dc.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00482180_004821dd.mcode. */
scr_ast_list_item_t **prepend_node(void *entry,
                                   scr_ast_list_item_t **headLink)
{
    scr_ast_list_item_t *item =
        coduomp_script_parse_allocate(sizeof(*item));

    item->entry = entry;
    item->next = *headLink;
    *headLink = item;
    return headLink;
}

/* Source: CoDUOMP.exe 0x004821e0..0x00482245.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004821e0_00482246.mcode. */
scr_ast_list_t *append_node(scr_ast_list_t *list, void *entry)
{
    scr_ast_list_item_t *item =
        coduomp_script_parse_allocate(sizeof(*item));

    item->entry = entry;
    item->next = NULL;
    list->tail->next = item;
    list->tail = item;
    return list;
}

/* Source: CoDUOMP.exe 0x00482250..0x00482268.
 * The same-module Mac build does not retain this helper, so the exact original
 * source name is unavailable; ScriptParse_ConcatLists describes its proven
 * operation. Both input lists are assumed to contain at least one item. */
scr_ast_list_t *ScriptParse_ConcatLists(scr_ast_list_t *first,
                                        const scr_ast_list_t *second)
{
    first->tail->next = second->head;
    first->tail = second->tail;
    return first;
}
