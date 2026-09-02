#ifndef CLIENT_UI_MENU_TYPES_H
#define CLIENT_UI_MENU_TYPES_H

#include <stddef.h>
#include <stdint.h>

#include "qcommon/q_key_types.h"
#include "qcommon/q_shared_types.h"
#include "qcommon/q_vector_types.h"

/*
 * Shared Quake III UI rectangle.  Both client DLLs use its four binary32
 * lanes as the leading geometry of windowDef_t.
 */
typedef struct rectDef_s {
    float x;   /* +0x00 */
    float y;   /* +0x04 */
    float w;   /* +0x08 */
    float h;   /* +0x0c */
} rectDef_t;

/*
 * Shared client-DLL UI window record.  The retained Windows Window_Init
 * bodies are instruction-for-instruction identical after rebasing:
 * uo_cgame_mp_x86.dll 0x30050a30 and uo_ui_mp_x86.dll 0x40012550 each clear
 * 0x2e dwords, then write the same defaults at +0x44, +0x74..+0x80, and
 * +0x30.  The supporting Mac cgame and UI bodies at file offsets 0x25800 and
 * 0x1b750 independently clear 184 bytes and store to the same fields. Pointer
 * members are native-width source fields; only the original 32-bit builds have
 * the annotated 0xb8-byte layout.
 */
typedef struct windowDef_s {
    rectDef_t rect;                 /* +0x00: current window bounds */
    rectDef_t rectClient;           /* +0x10: original/client-space bounds;
                                     * ItemParse_origin adds parsed x/y here */
    const char *name;               /* +0x20: window name/group-match key */
    /* CoD-only +0x24 lane. Neither recovered cgame nor UI binary consumes it. */
    uint8_t abiGap_024[4];
    const char *group;              /* +0x28: window group-match key */
    const char *cinematicName;      /* +0x2c: style-5 cinematic filename */
    int32_t cinematic;              /* +0x30: cinematic handle; -1 means none */
    int32_t style;                  /* +0x34: windowStyle_e */
    int32_t border;                 /* +0x38: border style/gate */
    int32_t ownerDraw;              /* +0x3c: owner-draw id */
    int32_t ownerDrawFlags;         /* +0x40: owner-draw visibility/input flags */
    float borderSize;               /* +0x44: border thickness and child inset */
    int32_t flags;                  /* +0x48: WINDOW_* bitfield */
    rectDef_t rectEffects;          /* +0x4c: transition target/orbit center */
    rectDef_t rectEffects2;         /* +0x5c: per-component transition velocity */
    int32_t offsetTime;             /* +0x6c: transition/orbit update interval */
    int32_t nextTime;               /* +0x70: next fade/transition update time */
    vec4_t foreColor;               /* +0x74: foreground/text RGBA */
    vec4_t backColor;               /* +0x84: background RGBA */
    vec4_t borderColor;             /* +0x94: border RGBA */
    vec4_t outlineColor;            /* +0xa4: outline RGBA */
    qhandle_t background;           /* +0xb4: registered background shader */
} windowDef_t;

struct itemDef_s;

enum {
    MAX_MENUITEMS = 192,
    MAX_MENUS = 100,
    MAX_OPEN_MENUS = 16,
    CONTROL_BINDING_COUNT = 55,
    UI_STRING_HASH_SIZE = 2048,
    UI_KEY_NAME_BUFFER_SIZE = 128
};

/* Shared ui_shared.c menu-script dispatch entry.  The Windows cgame and UI
 * dispatch loops both walk 0x08-byte PE32 records, compare the name at +0x00,
 * and call the handler at +0x04 with (item, &parsePosition). */
typedef void (*scriptCommand_t)(struct itemDef_s *item, char **args);

typedef struct commandDef_s {
    const char *name;
    scriptCommand_t handler;
} commandDef_t;

/* Shared ui_controls binding row.  Both Windows client DLLs walk the same
 * 55-row retail domain with a 0x18-byte PE32 stride and key caches at
 * +0x10/+0x14. */
typedef struct bind_s {
    const char *command;
    int32_t defaultKeys[3];
    int32_t bind1;
    int32_t bind2;
} bind_t;

/* Shared ui_shared.c string-intern hash node.  Both client DLLs allocate an
 * 0x08-byte PE32 node with the chain link at +0x00 and pooled string at +0x04.
 * Native pointers widen naturally on 64-bit hosts. */
typedef struct stringDef_s {
    struct stringDef_s *next;
    const char *str;
} stringDef_t;

/* Per-frame held-control callback used by both copies of ui_shared.c. */
typedef void (*ui_captureFunc_t)(void *captureData);

typedef enum uiCursorType_e {
    UI_CURSOR_ARROW = 2,
    UI_CURSOR_SIZER = 4
} uiCursorType_t;

/*
 * Shared client-DLL menu record.  The paired Menu_Init bodies at cgame
 * 0x30058750 and UI 0x4001a2c0 are likewise identical after rebasing: both
 * clear 0x204 dwords, access the fields below at the same offsets, then apply
 * the common embedded-window initialization above. The supporting Mac cgame
 * and UI bodies at file offsets 0x1d940 and 0x108f0 likewise clear 2064 bytes,
 * use the same offsets, and call their matching Window_Init bodies.
 */
typedef struct menuDef_s {
    windowDef_t window;             /* +0x000: same complete block as itemDef_t */
    const char *font;               /* +0x0b8: menu font name */
    int32_t fullScreen;             /* +0x0bc: force 640x480 window rectangle */
    int32_t itemCount;              /* +0x0c0: populated entries in items[] */
    int32_t fontIndex;              /* +0x0c4: inherited Q3 lane; dormant here */
    int32_t cursorItem;             /* +0x0c8: focused item index; -1 means none */
    int32_t fadeCycle;              /* +0x0cc: milliseconds between fade steps */
    float fadeClamp;                /* +0x0d0: maximum faded-in alpha */
    float fadeAmount;               /* +0x0d4: alpha subtracted per fade-out step */
    float fadeInAmount;             /* +0x0d8: alpha added per fade-in step */
    const char *onOpen;             /* +0x0dc: menu-open script */
    const char *onClose;            /* +0x0e0: menu-close script */
    const char *onESC;              /* +0x0e4: escape-key script */
    const char *onKey[MAX_KEYS];    /* +0x0e8: key scripts; [255] is any-key */
    const char *soundName;          /* +0x4e8: source keyword is "soundLoop" */
    int32_t loadMode;               /* +0x4ec: child-resource asset load mode */
    vec4_t focusColor;              /* +0x4f0: focused-child pulse color */
    vec4_t disableColor;            /* +0x500: disabled-item paint color */
    struct itemDef_s *items[MAX_MENUITEMS]; /* +0x510: 192-pointer table */
} menuDef_t;

/* The cgame and UI item-definition families are the same retained Quake UI
 * subsystem.  The Windows Item_Init bodies at 0x300589c0 and 0x4001a530 are
 * instruction-for-instruction identical after rebasing, as are the complete
 * Item_ValidateTypeData bodies at 0x30058d20 and 0x4001a890.  They prove the
 * 0x25c item extent, discriminants, payload extents, and member offsets below;
 * the supporting Mac client modules corroborate the same pointer-bearing
 * layouts.  Pointer fields widen naturally outside the original i386 ABI. */
enum itemType_e {
    ITEM_TYPE_TEXT = 0,
    ITEM_TYPE_BUTTON = 1,
    ITEM_TYPE_RADIOBUTTON = 2,
    ITEM_TYPE_CHECKBOX = 3,
    ITEM_TYPE_EDITFIELD = 4,
    ITEM_TYPE_COMBO = 5,
    ITEM_TYPE_LISTBOX = 6,
    ITEM_TYPE_MODEL = 7,
    ITEM_TYPE_OWNERDRAW = 8,
    ITEM_TYPE_NUMERICFIELD = 9,
    ITEM_TYPE_SLIDER = 10,
    ITEM_TYPE_YESNO = 11,
    ITEM_TYPE_MULTI = 12,
    ITEM_TYPE_BIND = 13,
    ITEM_TYPE_MENUMODEL = 14,
    ITEM_TYPE_UPREDITFIELD = 15
};

typedef enum windowStyle_e {
    WINDOW_STYLE_EMPTY = 0,
    WINDOW_STYLE_FILLED = 1,
    WINDOW_STYLE_GRADIENT = 2,
    WINDOW_STYLE_SHADER = 3,
    WINDOW_STYLE_TEAMCOLOR = 4,
    WINDOW_STYLE_CINEMATIC = 5,
    WINDOW_STYLE_SHADER_NO_TINT = 6
} windowStyle_t;

typedef enum windowBorder_e {
    WINDOW_BORDER_NONE = 0,
    WINDOW_BORDER_FULL = 1,
    WINDOW_BORDER_HORIZONTAL = 2,
    WINDOW_BORDER_VERTICAL = 3,
    WINDOW_BORDER_KCGRADIENT = 4
} windowBorder_t;

enum {
    WINDOW_MOUSEOVER = 0x00000001,
    WINDOW_HASFOCUS = 0x00000002,
    WINDOW_VISIBLE = 0x00000004,
    WINDOW_DECORATION = 0x00000010,
    WINDOW_FADINGOUT = 0x00000020,
    WINDOW_FADINGIN = 0x00000040,
    WINDOW_MOUSEOVERTEXT = 0x00000080,
    WINDOW_INTRANSITION = 0x00000100,
    WINDOW_FORECOLORSET = 0x00000200,
    WINDOW_HORIZONTAL = 0x00000400,
    WINDOW_LB_LEFTARROW = 0x00000800,
    WINDOW_LB_RIGHTARROW = 0x00001000,
    WINDOW_LB_THUMB = 0x00002000,
    WINDOW_LB_PGUP = 0x00004000,
    WINDOW_LB_PGDN = 0x00008000,
    WINDOW_LB_REGION_MASK = 0x0000f800,
    WINDOW_ORBITING = 0x00010000,
    WINDOW_OOB_CLICK = 0x00020000,
    WINDOW_WRAPPED = 0x00040000,
    WINDOW_AUTOWRAPPED = 0x00080000,
    WINDOW_MOUSE_INTERACTIVE = 0x00100000,
    /* The shared physical bit is menu-modal state on menuDef_t and shortened
     * cvar-text display state on itemDef_t. */
    WINDOW_MODAL = 0x00200000,
    WINDOW_TEXTCVARSHORT = 0x00200000,
    WINDOW_BACKCOLOR_SET = 0x00400000,
    WINDOW_NO_HUD_ALPHA = 0x01000000
};

#define WINDOW_MOUSE_ACTIVE (WINDOW_VISIBLE | WINDOW_MOUSE_INTERACTIVE)
#define WINDOW_TRANSITION_START (WINDOW_INTRANSITION | WINDOW_VISIBLE)

enum {
    LISTBOX_ELEMENT_IMAGE = 1,
    MAX_LB_COLUMNS = 16,
    MAX_MULTI_CVARS = 32,
    MAX_COLOR_RANGES = 10,
    ITEM_CVAR_ENABLE = 1,
    ITEM_CVAR_DISABLE = 2,
    ITEM_CVAR_SHOW = 4,
    ITEM_CVAR_HIDE = 8,
    COLOR_RANGE_ABSOLUTE = 0,
    COLOR_RANGE_RELATIVE = 1
};

#define ITEM_CVAR_ENABLE_MASK (ITEM_CVAR_ENABLE | ITEM_CVAR_DISABLE)
#define ITEM_CVAR_SHOW_MASK (ITEM_CVAR_SHOW | ITEM_CVAR_HIDE)
#define SCROLLBAR_SIZE 16.0f

typedef struct columnInfo_s {
    int32_t pos;
    int32_t width;
    int32_t maxChars;
} columnInfo_t;

typedef struct listBoxDef_s {
    int32_t startPos;
    int32_t endPos;
    int32_t drawPadding;
    int32_t cursorPos;
    float elementWidth;
    float elementHeight;
    int32_t elementStyle;
    int32_t numColumns;
    columnInfo_t columnInfo[MAX_LB_COLUMNS];
    const char *doubleClick;
    qboolean notselectable;
} listBoxDef_t;

typedef struct scrollInfo_s {
    int32_t nextScrollTime;
    int32_t nextAdjustTime;
    int32_t adjustValue;
    int32_t scrollKey;
    float xStart;
    float yStart;
    struct itemDef_s *item;
    int32_t scrollDir;
} scrollInfo_t;

typedef struct editFieldDef_s {
    float minVal;
    float maxVal;
    float defVal;
    float range;
    int32_t maxChars;
    int32_t maxCharsGotoNext;
    int32_t maxPaintChars;
    int32_t paintOffset;
} editFieldDef_t;

typedef struct multiDef_s {
    const char *cvarList[MAX_MULTI_CVARS];
    const char *cvarStr[MAX_MULTI_CVARS];
    float cvarValue[MAX_MULTI_CVARS];
    int32_t count;
    int32_t strDef;
} multiDef_t;

typedef struct colorRangeDef_s {
    vec4_t color;
    /* Retained CoD lane copied with the record but otherwise unconsumed by
     * either Windows client module. */
    uint8_t reserved10[4];
    float low;
    float high;
} colorRangeDef_t;

typedef struct modelDef_s {
    int32_t angle;
    vec3_t origin;
    float fovX;
    float fovY;
    int32_t rotationSpeed;
    int32_t animated;
    int32_t startFrame;
    int32_t numFrames;
    int32_t loopFrames;
    int32_t fps;
    int32_t frame;
    int32_t oldFrame;
    float backlerp;
    int32_t frameTime;
} modelDef_t;

typedef struct itemDef_s {
    windowDef_t window;             /* +0x000 */
    rectDef_t textRect;             /* +0x0b8 */
    int32_t type;                   /* +0x0c8: declared ITEM_TYPE_* */
    int32_t typeValidated;          /* +0x0cc: committed typeData domain */
    int32_t alignment;
    int32_t font;
    int32_t textalignment;
    float textalignx;
    float textaligny;
    float textscale;
    int32_t textStyle;
    const char *text;
    menuDef_t *parent;
    qhandle_t asset;
    const char *mouseEnterText;
    const char *mouseExitText;
    const char *mouseEnter;
    const char *mouseExit;
    const char *action;
    const char *accept;
    const char *onFocus;
    const char *leaveFocus;
    const char *cvar;
    const char *cvarTest;
    const char *enableCvar;
    int32_t cvarFlags;
    const char *focusSound;
    int32_t numColors;
    colorRangeDef_t colorRanges[MAX_COLOR_RANGES];
    int32_t colorRangeType;
    float special;
    int32_t cursorPos;
    void *typeData;
    int32_t loadMode;
} itemDef_t;

/* Shared ui_shared keyword-chain records.  The Windows cgame setup bodies at
 * 0x3005a300/0x3005aba0 and UI setup bodies at 0x4001be70/0x4001c7f0 all walk
 * 0x0c-byte i386 entries and link collision chains through +0x08; their parse
 * dispatchers call the handler stored at +0x04.  These retain the inherited
 * Quake III type and member spellings instead of the UI reconstruction's
 * duplicate *KeywordDef names. */
enum { KEYWORDHASH_SIZE = 512 };

typedef struct keywordHash_s {
    char *keyword;
    qboolean (*func)(itemDef_t *item, int32_t sourceHandle);
    struct keywordHash_s *next;
} keywordHash_t;

typedef struct menuKeywordHash_s {
    char *keyword;
    qboolean (*func)(menuDef_t *menu, int32_t sourceHandle);
    struct menuKeywordHash_s *next;
} menuKeywordHash_t;

typedef char ui_column_info_size[
    sizeof(columnInfo_t) == 0x0c ? 1 : -1];
typedef char ui_list_box_element_style_offset[
    offsetof(listBoxDef_t, elementStyle) == 0x18 ? 1 : -1];
typedef char ui_list_box_columns_offset[
    offsetof(listBoxDef_t, columnInfo) == 0x20 ? 1 : -1];
typedef char ui_edit_field_paint_offset_offset[
    offsetof(editFieldDef_t, paintOffset) == 0x1c ? 1 : -1];
typedef char ui_color_range_size[
    sizeof(colorRangeDef_t) == 0x1c ? 1 : -1];
typedef char ui_color_range_low_offset[
    offsetof(colorRangeDef_t, low) == 0x14 ? 1 : -1];
typedef char ui_model_def_size[
    sizeof(modelDef_t) == 0x40 ? 1 : -1];

typedef char ui_rect_def_size[sizeof(rectDef_t) == 0x10 ? 1 : -1];

#if UINTPTR_MAX == UINT32_MAX
typedef char ui_command_def_handler_offset[
    offsetof(commandDef_t, handler) == 0x04 ? 1 : -1];
typedef char ui_command_def_size[
    sizeof(commandDef_t) == 0x08 ? 1 : -1];
typedef char ui_bind_first_key_offset[
    offsetof(bind_t, bind1) == 0x10 ? 1 : -1];
typedef char ui_bind_second_key_offset[
    offsetof(bind_t, bind2) == 0x14 ? 1 : -1];
typedef char ui_bind_size[
    sizeof(bind_t) == 0x18 ? 1 : -1];
typedef char ui_string_def_string_offset[
    offsetof(stringDef_t, str) == 0x04 ? 1 : -1];
typedef char ui_string_def_size[
    sizeof(stringDef_t) == 0x08 ? 1 : -1];

typedef char ui_keyword_hash_func_offset[
    offsetof(keywordHash_t, func) == 0x04 ? 1 : -1];
typedef char ui_keyword_hash_next_offset[
    offsetof(keywordHash_t, next) == 0x08 ? 1 : -1];
typedef char ui_keyword_hash_size[
    sizeof(keywordHash_t) == 0x0c ? 1 : -1];
typedef char ui_menu_keyword_hash_func_offset[
    offsetof(menuKeywordHash_t, func) == 0x04 ? 1 : -1];
typedef char ui_menu_keyword_hash_next_offset[
    offsetof(menuKeywordHash_t, next) == 0x08 ? 1 : -1];
typedef char ui_menu_keyword_hash_size[
    sizeof(menuKeywordHash_t) == 0x0c ? 1 : -1];

typedef char ui_window_def_name_offset[
    offsetof(windowDef_t, name) == 0x20 ? 1 : -1];
typedef char ui_window_def_group_offset[
    offsetof(windowDef_t, group) == 0x28 ? 1 : -1];
typedef char ui_window_def_cinematic_name_offset[
    offsetof(windowDef_t, cinematicName) == 0x2c ? 1 : -1];
typedef char ui_window_def_cinematic_offset[
    offsetof(windowDef_t, cinematic) == 0x30 ? 1 : -1];
typedef char ui_window_def_border_size_offset[
    offsetof(windowDef_t, borderSize) == 0x44 ? 1 : -1];
typedef char ui_window_def_flags_offset[
    offsetof(windowDef_t, flags) == 0x48 ? 1 : -1];
typedef char ui_window_def_fore_color_offset[
    offsetof(windowDef_t, foreColor) == 0x74 ? 1 : -1];
typedef char ui_window_def_background_offset[
    offsetof(windowDef_t, background) == 0xb4 ? 1 : -1];
typedef char ui_window_def_size[sizeof(windowDef_t) == 0xb8 ? 1 : -1];

typedef char ui_menu_def_font_offset[
    offsetof(menuDef_t, font) == 0xb8 ? 1 : -1];
typedef char ui_menu_def_cursor_item_offset[
    offsetof(menuDef_t, cursorItem) == 0xc8 ? 1 : -1];
typedef char ui_menu_def_fade_cycle_offset[
    offsetof(menuDef_t, fadeCycle) == 0xcc ? 1 : -1];
typedef char ui_menu_def_on_key_offset[
    offsetof(menuDef_t, onKey) == 0xe8 ? 1 : -1];
typedef char ui_menu_def_load_mode_offset[
    offsetof(menuDef_t, loadMode) == 0x4ec ? 1 : -1];
typedef char ui_menu_def_items_offset[
    offsetof(menuDef_t, items) == 0x510 ? 1 : -1];
typedef char ui_menu_def_size[sizeof(menuDef_t) == 0x810 ? 1 : -1];

typedef char ui_list_box_double_click_offset[
    offsetof(listBoxDef_t, doubleClick) == 0xe0 ? 1 : -1];
typedef char ui_list_box_notselectable_offset[
    offsetof(listBoxDef_t, notselectable) == 0xe4 ? 1 : -1];
typedef char ui_list_box_size[
    sizeof(listBoxDef_t) == 0xe8 ? 1 : -1];
typedef char ui_scroll_info_item_offset[
    offsetof(scrollInfo_t, item) == 0x18 ? 1 : -1];
typedef char ui_scroll_info_size[
    sizeof(scrollInfo_t) == 0x20 ? 1 : -1];
typedef char ui_multi_def_string_offset[
    offsetof(multiDef_t, cvarStr) == 0x80 ? 1 : -1];
typedef char ui_multi_def_value_offset[
    offsetof(multiDef_t, cvarValue) == 0x100 ? 1 : -1];
typedef char ui_multi_def_size[
    sizeof(multiDef_t) == 0x188 ? 1 : -1];
typedef char ui_item_def_type_offset[
    offsetof(itemDef_t, type) == 0xc8 ? 1 : -1];
typedef char ui_item_def_text_offset[
    offsetof(itemDef_t, text) == 0xec ? 1 : -1];
typedef char ui_item_def_color_ranges_offset[
    offsetof(itemDef_t, colorRanges) == 0x130 ? 1 : -1];
typedef char ui_item_def_type_data_offset[
    offsetof(itemDef_t, typeData) == 0x254 ? 1 : -1];
typedef char ui_item_def_load_mode_offset[
    offsetof(itemDef_t, loadMode) == 0x258 ? 1 : -1];
typedef char ui_item_def_size[
    sizeof(itemDef_t) == 0x25c ? 1 : -1];
#endif

#endif
