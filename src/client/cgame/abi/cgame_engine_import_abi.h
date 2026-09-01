#ifndef CODUO_CGAME_ENGINE_IMPORT_ABI_H
#define CODUO_CGAME_ENGINE_IMPORT_ABI_H

#include "cgame_module_abi.h"

enum {
    CGAME_ENGINE_IMPORT_COUNT = 102,
    CGAME_IMPORT_SCR_BEGIN_LOAD_ANIM_TREES = 32,
    CGAME_IMPORT_SCR_END_LOAD_ANIM_TREES = 34,
    CGAME_IMPORT_SCR_PRECACHE_ANIM_TREES = 35,
    CGAME_IMPORT_SCR_FIND_ANIM_TREE = 80,
    CGAME_IMPORT_SCR_FIND_ANIM = 81,
    CGAME_IMPORT_SCR_GET_ANIMS_INDEX = 98
};

typedef struct cgame_engine_import_table32_s {
    cgame_abi_pointer32_t slots[CGAME_ENGINE_IMPORT_COUNT];
} cgame_engine_import_table32_t;

typedef void (CGAME_ABI_CDECL *cgame_import_begin_load_anim_trees32_t)(void);
typedef void (CGAME_ABI_CDECL *cgame_import_end_load_anim_trees32_t)(void);
typedef void (CGAME_ABI_CDECL *cgame_import_precache_anim_trees32_t)(
    cgame_abi_pointer32_t allocator_callback);
typedef cgame_abi_pointer32_t (CGAME_ABI_CDECL *cgame_import_find_anim_tree32_t)(
    cgame_abi_pointer32_t name);
typedef void (CGAME_ABI_CDECL *cgame_import_find_anim32_t)(
    cgame_abi_pointer32_t treeName, cgame_abi_pointer32_t animName,
    cgame_abi_pointer32_t outAnim);
typedef uint32_t (CGAME_ABI_CDECL *cgame_import_get_anims_index32_t)(
    cgame_abi_pointer32_t anims);

_Static_assert(sizeof(cgame_engine_import_table32_t) == 408,
               "original direct-import table is 102 dwords");

#endif
