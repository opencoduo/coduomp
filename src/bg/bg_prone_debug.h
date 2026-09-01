#ifndef BG_PRONE_DEBUG_H
#define BG_PRONE_DEBUG_H

/* Internal color identities used by the shared prone validator.  The original
 * modules own distinct color objects and debug-draw entry points; target
 * service headers map these identities to those original boundaries. */
typedef enum bg_prone_debug_color_e {
    BG_PRONE_DEBUG_RED,
    BG_PRONE_DEBUG_GREEN,
    BG_PRONE_DEBUG_YELLOW,
    BG_PRONE_DEBUG_MAGENTA,
    BG_PRONE_DEBUG_CYAN,
    BG_PRONE_DEBUG_MEDIUM_CYAN
} bg_prone_debug_color_t;

#endif
