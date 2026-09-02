#include "ui_runtime.h"

#include "compat/coduo_int32_bits.h"
#include "qcommon/q_string.h"
#include "ui_menu_globals.h"

#include <string.h>

enum {
    UI_CHAR_BACKSPACE = 8,
    UI_EDIT_INSERT_MAX_CHARS = 255,
};

/* Original instruction twins: uo_cgame_mp_x86.dll 0x30053ea0 and
 * uo_ui_mp_x86.dll 0x40015a00. */
extern displayContextDef_t *DC;
void Com_Printf(const char *format, ...);
qboolean Item_TextField_HandleKey(itemDef_t *item, int32_t key)
{
    editFieldDef_t *editField;
    char buffer[MAX_STRING_CHARS];
    int32_t length;

    switch (item->typeValidated) {
    case ITEM_TYPE_TEXT:
    case ITEM_TYPE_EDITFIELD:
    case ITEM_TYPE_NUMERICFIELD:
    case ITEM_TYPE_SLIDER:
    case ITEM_TYPE_YESNO:
    case ITEM_TYPE_BIND:
    case ITEM_TYPE_UPREDITFIELD:
        break;
    default:
        Com_Printf("^1Menu Error: Expecting type: ITEM_TYPE_EDITFIELD, "
                   "ITEM_TYPE_NUMERICFIELD, ITEM_TYPE_UPREDITFIELD, "
                   "ITEM_TYPE_YESNO, ITEM_TYPE_BIND, ITEM_TYPE_SLIDER, or "
                   "ITEM_TYPE_TEXT\n");
        return qfalse;
    }

    editField = (editFieldDef_t *)item->typeData;
    if (editField == NULL || item->cvar == NULL)
        return qfalse;

    memset(buffer, 0, sizeof(buffer));
    DC->getCVarString(item->cvar, buffer, sizeof(buffer));
    length = (int32_t)strlen(buffer);
    if (editField->maxChars != 0 && length > editField->maxChars) {
        length = editField->maxChars;
    }

    if ((key & K_CHAR_FLAG) != 0) {
        key &= ~K_CHAR_FLAG;
        if (key == UI_CHAR_BACKSPACE) {
            if (item->cursorPos > 0) {
                memmove(buffer + item->cursorPos - 1, buffer + item->cursorPos, (size_t)(length - item->cursorPos + 1));
                --item->cursorPos;
                if (item->cursorPos < editField->paintOffset) {
                    --editField->paintOffset;
                }
            }
            DC->setCVar(item->cvar, buffer);
            return qtrue;
        }

        if (key < ' ' || item->cvar == NULL)
            return qtrue;
        if (item->type == ITEM_TYPE_NUMERICFIELD && !Q_isnumeric(key)) {
            return qfalse;
        }
        if (item->type == ITEM_TYPE_UPREDITFIELD && key >= 'a' && key <= 'z') {
            key -= 'a' - 'A';
        }

        if (!DC->getOverstrikeMode()) {
            if (length == UI_EDIT_INSERT_MAX_CHARS || (editField->maxChars != 0 && length >= editField->maxChars)) {
                return qtrue;
            }
            memmove(buffer + item->cursorPos + 1, buffer + item->cursorPos, (size_t)(length - item->cursorPos + 1));
        } else if (editField->maxChars != 0 && item->cursorPos >= editField->maxChars) {
            if (editField->maxCharsGotoNext != 0) {
                itemDef_t *next = Menu_SetNextCursorItem(item->parent);

                if (next != NULL &&
                    (next->type == ITEM_TYPE_EDITFIELD || next->type == ITEM_TYPE_NUMERICFIELD || next->type == ITEM_TYPE_UPREDITFIELD)) {
                    g_editItem = next;
                }
            }
            return qtrue;
        }

        buffer[item->cursorPos] = (char)key;
        DC->setCVar(item->cvar, buffer);
        ++length;
        if (item->cursorPos < length) {
            ++item->cursorPos;
            if (editField->maxPaintChars != 0 && item->cursorPos > editField->maxPaintChars) {
                ++editField->paintOffset;
            }
        }
        if (editField->maxChars != 0 && item->cursorPos >= editField->maxChars && editField->maxCharsGotoNext != 0) {
            itemDef_t *next = Menu_SetNextCursorItem(item->parent);

            if (next != NULL &&
                (next->type == ITEM_TYPE_EDITFIELD || next->type == ITEM_TYPE_NUMERICFIELD || next->type == ITEM_TYPE_UPREDITFIELD)) {
                g_editItem = next;
            }
        }
    } else {
        switch (key) {
        case K_DEL:
        case K_KP_DEL:
            if (item->cursorPos < length) {
                memmove(buffer + item->cursorPos, buffer + item->cursorPos + 1, (size_t)(length - item->cursorPos));
                DC->setCVar(item->cvar, buffer);
            }
            return qtrue;
        case K_RIGHTARROW:
        case K_KP_RIGHTARROW:
            if (editField->maxPaintChars != 0 && item->cursorPos >= editField->maxPaintChars && item->cursorPos < length) {
                ++item->cursorPos;
                ++editField->paintOffset;
            } else if (item->cursorPos < length) {
                ++item->cursorPos;
            }
            return qtrue;
        case K_LEFTARROW:
        case K_KP_LEFTARROW:
            if (item->cursorPos > 0)
                --item->cursorPos;
            if (item->cursorPos < editField->paintOffset) {
                --editField->paintOffset;
            }
            return qtrue;
        case K_HOME:
        case K_KP_HOME:
            item->cursorPos = 0;
            editField->paintOffset = 0;
            return qtrue;
        case K_END:
        case K_KP_END:
            item->cursorPos = length;
            if (length > editField->maxPaintChars) {
                editField->paintOffset = length - editField->maxPaintChars;
            }
            return qtrue;
        case K_INS:
        case K_KP_INS:
            DC->setOverstrikeMode(!DC->getOverstrikeMode());
            return qtrue;
        default:
            break;
        }
    }

    if (key == K_TAB || key == K_DOWNARROW || key == K_KP_DOWNARROW) {
        itemDef_t *next = Menu_SetNextCursorItem(item->parent);

        if (next != NULL &&
            (next->type == ITEM_TYPE_EDITFIELD || next->type == ITEM_TYPE_NUMERICFIELD || next->type == ITEM_TYPE_UPREDITFIELD)) {
            g_editItem = next;
        }
    }
    if (key == K_UPARROW || key == K_KP_UPARROW) {
        itemDef_t *previous = Menu_SetPrevCursorItem(item->parent);

        if (previous != NULL && (previous->type == ITEM_TYPE_EDITFIELD || previous->type == ITEM_TYPE_NUMERICFIELD ||
                                 previous->type == ITEM_TYPE_UPREDITFIELD)) {
            g_editItem = previous;
        }
    }
    if ((key == K_ENTER || key == K_KP_ENTER) && item->accept != NULL) {
        Item_RunScript(item, item->accept);
    }
    if (key == K_ENTER || key == K_KP_ENTER || key == K_ESCAPE) {
        return qfalse;
    }
    return qtrue;
}
