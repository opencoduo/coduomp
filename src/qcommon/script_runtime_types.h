#ifndef QCOMMON_SCRIPT_RUNTIME_TYPES_H
#define QCOMMON_SCRIPT_RUNTIME_TYPES_H

#include "q_shared_types.h"
#include "script_types.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#if defined(_MSC_VER) && defined(_M_IX86)
#define CODUO_SCRIPT_CDECL __cdecl
#elif defined(__i386__) && (defined(__GNUC__) || defined(__clang__))
#define CODUO_SCRIPT_CDECL __attribute__((cdecl))
#else
#define CODUO_SCRIPT_CDECL
#endif

enum {
    SCRIPT_SOURCE_POS_TABLE_COUNT = 2,
    SCRIPT_MEMORY_BLOCK_COUNT = 65536,
    SCRIPT_MEMORY_BLOCK_SIZE = 8,
    SCRIPT_STRING_HASH_SLOT_COUNT = 16384,
    SCRIPT_VARIABLE_NODE_COUNT = 65536,
    /* Physical ceiling of the retained 16-bit string, vector, and object
     * reference-count fields. Changing it requires an ABI/layout revision. */
    SCRIPT_REFERENCE_COUNT_MAX = UINT16_MAX,
    SCRIPT_VARIABLE_NODE_TYPE_MASK = 31,
    SCRIPT_VARIABLE_NODE_PACKED_INDEX_SHIFT = 8,
    SCRIPT_OBJECT_ID_MAP_COUNT = 65536,
    SCRIPT_CALL_STACK_COUNT = 32,
    SCRIPT_ANIM_PROPERTY_NAME_COUNT = 3,
    SCRIPT_ANIM_SLOT_COUNT = 2,
    SCRIPT_ANIM_TREE_SLOT_COUNT = 128,
    SCRIPT_ANIM_TREE_REGISTERED_CAPACITY = SCRIPT_ANIM_TREE_SLOT_COUNT - 1
};

typedef enum script_anim_property_flag_e {
    SCRIPT_ANIM_PROPERTY_LOOPSYNC = 1u << 0,
    SCRIPT_ANIM_PROPERTY_NONLOOPSYNC = 1u << 1,
    SCRIPT_ANIM_PROPERTY_COMPLETE = 1u << 3
} script_anim_property_flag_t;

typedef uint8_t *script_codepos_t;

/* These two pointer-width cells are maintained-host compatibility carriers.
 * The original i386 VM stores each as one 32-bit word. */
typedef uintptr_t coduo_script_value_payload_t;
typedef uintptr_t coduo_script_yystype_word_t;

#define SCRIPT_VALUE_INT_PAYLOAD(value) ((coduo_script_value_payload_t)(uint32_t)(int32_t)(value))
#define SCRIPT_VALUE_U32_PAYLOAD(value) ((coduo_script_value_payload_t)(uint32_t)(value))

typedef struct script_source_pos_record_s {
    uint8_t *codePos;
    uint32_t sourcePosIndex;
    uint32_t reserved08;
    uint32_t reserved0c;
} script_source_pos_record_t;

typedef struct script_source_file_record_s {
    uint8_t *normalCodeStart;
    uint8_t *relocatedCodeStart;
    char *filename;
    char *source;
    int32_t sourceLen;
} script_source_file_record_t;

typedef struct script_saved_source_file_s {
    char *source;
    int32_t sourceLen;
} script_saved_source_file_t;

/* Script operands are byte-packed and may begin at arbitrary byte offsets.
 * The pointer members remain full-width host addresses. */
typedef struct script_code_string_fixup_s {
    char *codePos;
    struct script_code_string_fixup_s *next;
} script_code_string_fixup_t;

typedef struct script_code_offset_patch_s {
    char *patch;
    struct script_code_offset_patch_s *next;
} script_code_offset_patch_t;

typedef struct script_switch_case_record_s {
    uint32_t value;
    uint8_t *codePos;
    uint32_t sourcePos;
    struct script_switch_case_record_s *next;
} script_switch_case_record_t;

typedef struct scr_script_load_record_s {
    uint16_t filenameHandle;
    uint8_t padding02[2];
    uint32_t sourcePos;
} scr_script_load_record_t;

/* The Mac traceback names and assignment operators expose these exact stock
 * spellings. Windows and Linux independently use payload/type at +0/+4 and
 * a twelve-byte variable-node stride on i386. */
typedef union VariableUnion {
    coduo_script_value_payload_t payload;
    coduo_script_value_payload_t valuePayload;
    struct {
        uint16_t valueOrRefCount;
        uint16_t parentHandle;
    } halves;
} VariableUnion;

#if defined(__GNUC__) || defined(__clang__)
#define SCRIPT_RUNTIME_MAY_ALIAS __attribute__((__may_alias__))
#else
#define SCRIPT_RUNTIME_MAY_ALIAS
#endif
typedef struct SCRIPT_RUNTIME_MAY_ALIAS VariableValue {
    union {
        VariableUnion u;
        coduo_script_value_payload_t payload;
    };
    script_variable_type_t type;
} VariableValue;
#undef SCRIPT_RUNTIME_MAY_ALIAS

#pragma pack(push, 1)
typedef struct VariableStackBufferEntry {
    uint8_t type;
    coduo_script_value_payload_t payload;
} VariableStackBufferEntry;
#pragma pack(pop)

/* The original header contains a live code pointer. Pointer-bearing fields
 * widen on native 64-bit builds; packed value lanes widen from five to nine
 * bytes so they can retain host pointers. */
typedef struct VariableStackBuffer {
    uint32_t time;
    uint8_t *pos;
    uint16_t size;
    uint16_t localId;
    VariableStackBufferEntry entries[];
} VariableStackBuffer;

typedef struct VariableValueInternal {
    VariableUnion u;
    uint32_t status;
} VariableValueInternal;

typedef struct script_variable_node_s {
    VariableUnion payload;
    uint32_t packedTypeIndex;
    uint16_t hashOrFreeNext;
    uint16_t nextSibling;
} script_variable_node_t;

typedef struct Variable {
    uint16_t valueIndex;
    uint16_t previousSibling;
} Variable;

typedef union script_memory_block_u {
    struct {
        uint16_t left;
        uint16_t right;
        uint32_t payload;
    } freeNode;
    uint8_t bytes[SCRIPT_MEMORY_BLOCK_SIZE];
} script_memory_block_t;

/* The stock i386 allocation is 14 bytes with the vector at +2. Native 64-bit
 * builds insert two alignment bytes and use a 16-byte allocation. */
#if UINTPTR_MAX == UINT32_MAX
#pragma pack(push, 2)
#endif
typedef struct script_vector_storage_s {
    int16_t refCount;
#if UINTPTR_MAX > UINT32_MAX
    uint16_t alignmentPadding;
#endif
    vec3_t value;
} script_vector_storage_t;
#if UINTPTR_MAX == UINT32_MAX
#pragma pack(pop)
#endif

#define SCRIPT_VECTOR_STORAGE_FROM_VALUE(vector) \
    ((script_vector_storage_t *)((uint8_t *)(void *)(vector) - offsetof(script_vector_storage_t, value)))
#define SCRIPT_VECTOR_STORAGE_FROM_PAYLOAD(payload) \
    ((script_vector_storage_t *)(uintptr_t)((payload) - offsetof(script_vector_storage_t, value)))

#pragma pack(push, 1)
typedef struct script_switch_case_table_entry_s {
    uint32_t value;
    coduo_script_value_payload_t codePos;
} script_switch_case_table_entry_t;
#pragma pack(pop)

typedef struct script_string_hash_slot_s {
    uint16_t linkAndFlags;
    uint16_t stringHandle;
} script_string_hash_slot_t;

typedef struct script_yy_buffer_s {
    FILE *inputFile;
    char *chBuf;
    char *bufPos;
    int32_t bufSize;
    int32_t nChars;
    qboolean isOurBuffer;
    qboolean isInteractive;
    qboolean atBol;
    qboolean fillBuffer;
    int32_t bufferStatus;
} script_yy_buffer_t;

/* Exact Mac traceback spelling. The original yacc parser copies two words. */
typedef union sval_u {
    struct {
        coduo_script_yystype_word_t value;
        uint32_t sourcePos;
    } source;
    coduo_script_yystype_word_t words[2];
} sval_u;

#define SCRIPT_RUNTIME_LAYOUT_ASSERT(name_, expression_) typedef char name_[(expression_) ? 1 : -1]

#if defined(__cplusplus)
#define SCRIPT_RUNTIME_ALIGNOF(type_) alignof(type_)
#elif defined(_MSC_VER)
#define SCRIPT_RUNTIME_ALIGNOF(type_) __alignof(type_)
#else
#define SCRIPT_RUNTIME_ALIGNOF(type_) _Alignof(type_)
#endif

SCRIPT_RUNTIME_LAYOUT_ASSERT(script_anim_property_flag_size, sizeof(script_anim_property_flag_t) == sizeof(uint32_t));
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_codepos_pointer_size, sizeof(script_codepos_t) == sizeof(uint8_t *));
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_value_payload_pointer_size, sizeof(coduo_script_value_payload_t) == sizeof(uintptr_t));
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_yystype_word_pointer_size, sizeof(coduo_script_yystype_word_t) == sizeof(uintptr_t));
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_variable_indirection_size, sizeof(Variable) == 0x04);
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_variable_value_index_offset, offsetof(Variable, valueIndex) == 0x00);
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_variable_previous_offset, offsetof(Variable, previousSibling) == 0x02);
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_memory_block_size, sizeof(script_memory_block_t) == 0x08);
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_memory_block_payload_offset, offsetof(script_memory_block_t, freeNode.payload) == 0x04);
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_string_hash_slot_size, sizeof(script_string_hash_slot_t) == 0x04);
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_string_hash_handle_offset, offsetof(script_string_hash_slot_t, stringHandle) == 0x02);

#if UINTPTR_MAX == UINT32_MAX
#define SCRIPT_RUNTIME_I386_SIZE_ALIGN(name_, type_, size_, alignment_) \
    SCRIPT_RUNTIME_LAYOUT_ASSERT(name_##_size, sizeof(type_) == (size_)); \
    SCRIPT_RUNTIME_LAYOUT_ASSERT(name_##_alignment, SCRIPT_RUNTIME_ALIGNOF(type_) == (alignment_))

SCRIPT_RUNTIME_I386_SIZE_ALIGN(script_source_pos_record, script_source_pos_record_t, 0x10, 0x04);
SCRIPT_RUNTIME_I386_SIZE_ALIGN(script_source_file_record, script_source_file_record_t, 0x14, 0x04);
SCRIPT_RUNTIME_I386_SIZE_ALIGN(script_saved_source_file, script_saved_source_file_t, 0x08, 0x04);
SCRIPT_RUNTIME_I386_SIZE_ALIGN(script_code_string_fixup, script_code_string_fixup_t, 0x08, 0x04);
SCRIPT_RUNTIME_I386_SIZE_ALIGN(script_code_offset_patch, script_code_offset_patch_t, 0x08, 0x04);
SCRIPT_RUNTIME_I386_SIZE_ALIGN(script_switch_case_record, script_switch_case_record_t, 0x10, 0x04);
SCRIPT_RUNTIME_I386_SIZE_ALIGN(script_load_record, scr_script_load_record_t, 0x08, 0x04);
SCRIPT_RUNTIME_I386_SIZE_ALIGN(script_variable_union, VariableUnion, 0x04, 0x04);
SCRIPT_RUNTIME_I386_SIZE_ALIGN(script_variable_value, VariableValue, 0x08, 0x04);
SCRIPT_RUNTIME_I386_SIZE_ALIGN(script_stack_buffer, VariableStackBuffer, 0x0c, 0x04);
SCRIPT_RUNTIME_I386_SIZE_ALIGN(script_stack_entry, VariableStackBufferEntry, 0x05, 0x01);
SCRIPT_RUNTIME_I386_SIZE_ALIGN(script_vector_storage, script_vector_storage_t, 0x0e, 0x02);
SCRIPT_RUNTIME_I386_SIZE_ALIGN(script_switch_table_entry, script_switch_case_table_entry_t, 0x08, 0x01);
SCRIPT_RUNTIME_I386_SIZE_ALIGN(script_variable_internal, VariableValueInternal, 0x08, 0x04);
SCRIPT_RUNTIME_I386_SIZE_ALIGN(script_variable_node, script_variable_node_t, 0x0c, 0x04);
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_variable_indirection_alignment, SCRIPT_RUNTIME_ALIGNOF(Variable) == 0x02);
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_memory_block_alignment, SCRIPT_RUNTIME_ALIGNOF(script_memory_block_t) == 0x04);
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_string_hash_slot_alignment, SCRIPT_RUNTIME_ALIGNOF(script_string_hash_slot_t) == 0x02);
SCRIPT_RUNTIME_I386_SIZE_ALIGN(script_yy_buffer, script_yy_buffer_t, 0x28, 0x04);
SCRIPT_RUNTIME_I386_SIZE_ALIGN(script_yacc_value, sval_u, 0x08, 0x04);

SCRIPT_RUNTIME_LAYOUT_ASSERT(script_source_pos_code_offset, offsetof(script_source_pos_record_t, codePos) == 0x00);
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_source_pos_index_offset, offsetof(script_source_pos_record_t, sourcePosIndex) == 0x04);
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_source_pos_reserved08_offset, offsetof(script_source_pos_record_t, reserved08) == 0x08);
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_source_pos_reserved0c_offset, offsetof(script_source_pos_record_t, reserved0c) == 0x0c);
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_source_file_relocated_offset, offsetof(script_source_file_record_t, relocatedCodeStart) == 0x04);
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_source_file_name_offset, offsetof(script_source_file_record_t, filename) == 0x08);
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_source_file_source_offset, offsetof(script_source_file_record_t, source) == 0x0c);
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_source_file_length_offset, offsetof(script_source_file_record_t, sourceLen) == 0x10);
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_saved_source_length_offset, offsetof(script_saved_source_file_t, sourceLen) == 0x04);
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_fixup_next_offset, offsetof(script_code_string_fixup_t, next) == 0x04);
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_patch_next_offset, offsetof(script_code_offset_patch_t, next) == 0x04);
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_switch_case_code_offset, offsetof(script_switch_case_record_t, codePos) == 0x04);
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_switch_case_source_offset, offsetof(script_switch_case_record_t, sourcePos) == 0x08);
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_switch_case_next_offset, offsetof(script_switch_case_record_t, next) == 0x0c);
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_load_record_padding_offset, offsetof(scr_script_load_record_t, padding02) == 0x02);
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_load_record_source_offset, offsetof(scr_script_load_record_t, sourcePos) == 0x04);
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_variable_union_value_offset, offsetof(VariableUnion, halves.valueOrRefCount) == 0x00);
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_variable_union_parent_offset, offsetof(VariableUnion, halves.parentHandle) == 0x02);
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_variable_value_payload_offset, offsetof(VariableValue, payload) == 0x00);
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_variable_value_type_offset, offsetof(VariableValue, type) == 0x04);
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_variable_internal_status_offset, offsetof(VariableValueInternal, status) == 0x04);
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_stack_time_offset, offsetof(VariableStackBuffer, time) == 0x00);
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_stack_pos_offset, offsetof(VariableStackBuffer, pos) == 0x04);
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_stack_size_offset, offsetof(VariableStackBuffer, size) == 0x08);
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_stack_local_id_offset, offsetof(VariableStackBuffer, localId) == 0x0a);
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_stack_entries_offset, offsetof(VariableStackBuffer, entries) == 0x0c);
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_stack_entry_payload_offset, offsetof(VariableStackBufferEntry, payload) == 0x01);
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_vector_value_offset, offsetof(script_vector_storage_t, value) == 0x02);
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_switch_table_code_offset, offsetof(script_switch_case_table_entry_t, codePos) == 0x04);
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_variable_node_value_offset, offsetof(script_variable_node_t, payload.valuePayload) == 0x00);
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_variable_node_type_offset, offsetof(script_variable_node_t, packedTypeIndex) == 0x04);
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_variable_node_hash_offset, offsetof(script_variable_node_t, hashOrFreeNext) == 0x08);
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_variable_node_sibling_offset, offsetof(script_variable_node_t, nextSibling) == 0x0a);
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_yy_input_file_offset, offsetof(script_yy_buffer_t, inputFile) == 0x00);
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_yy_buffer_start_offset, offsetof(script_yy_buffer_t, chBuf) == 0x04);
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_yy_buffer_cursor_offset, offsetof(script_yy_buffer_t, bufPos) == 0x08);
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_yy_buffer_size_offset, offsetof(script_yy_buffer_t, bufSize) == 0x0c);
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_yy_character_count_offset, offsetof(script_yy_buffer_t, nChars) == 0x10);
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_yy_owned_offset, offsetof(script_yy_buffer_t, isOurBuffer) == 0x14);
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_yy_interactive_offset, offsetof(script_yy_buffer_t, isInteractive) == 0x18);
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_yy_line_start_offset, offsetof(script_yy_buffer_t, atBol) == 0x1c);
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_yy_fill_offset, offsetof(script_yy_buffer_t, fillBuffer) == 0x20);
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_yy_status_offset, offsetof(script_yy_buffer_t, bufferStatus) == 0x24);
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_yacc_words_offset, offsetof(sval_u, words) == 0x00);
SCRIPT_RUNTIME_LAYOUT_ASSERT(script_yacc_source_pos_offset, offsetof(sval_u, source.sourcePos) == 0x04);

#undef SCRIPT_RUNTIME_I386_SIZE_ALIGN
#endif

#undef SCRIPT_RUNTIME_ALIGNOF
#undef SCRIPT_RUNTIME_LAYOUT_ASSERT

#endif
