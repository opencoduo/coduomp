// Sources: uo_cgame_mp_x86.dll 0x300576d0..0x30057e19 and
//          uo_ui_mp_x86.dll    0x40019230..0x40019989.
// The operation graphs agree after instruction scheduling, alignment, and
// rebasing. UI additionally calls UI_SyncServerListSelection on the vertical
// path; the target adapter preserves that original module-specific edge.
//
// Item_ListBox_Paint — paint a menu listbox item: its scrollbar (two end arrows,
// the track, and the drag thumb, all via DC->drawHandlePic) and the visible rows
// of its data feeder.
//
// Resolved name: the .mcode header's size-matched guess "PM_ViewHeightAdjust" is
// REJECTED. The body Com_Printf's "^1Menu Error: Expecting type: ITEM_TYPE_LISTBOX"
// (rdata 0x3007b864) when item->typeValidated (+0xcc) != ITEM_TYPE_LISTBOX and draws
// the ui/assets/scrollbar_* shaders (DC Assets +0x1e2c4..+0x1e2d8) — it is the UI
// listbox painter, not a player-movement routine. Behavior matches Q3 ui_shared.c
// Item_ListBox_Paint; the same-module PPC bank carries the same name.
//
// ABI: item arrives in EAX (MOV ESI,EAX at entry); no stack arguments; the frame is
// a plain SUB ESP,0x1c local block (EBX/EBP/EDI/ESI callee-saved). Two nearly-
// parallel layouts are selected by window.flags & WINDOW_HORIZONTAL (0x400, tested
// as TEST AH,0x4 on the dword [ESI+0x48]).
//
// Register mapping used below:
//   ESI = item (itemDef_t*)          EDI = listBox (listBoxDef_t*, item->typeData)
//   EBX = count (feeder element count / current row bound)
//   EBP = current element index i    DC = *(displayContextDef_t**)0x30134d2c
//
// Constants proven from .rdata / immediates (dumped via objdump):
//   0x41800000 imm  = 16.0f  (== SCROLLBAR_SIZE, also 0x3007bf00)
//   0x3007bce0 = 1.0f   0x3007bce4 = 2.0f   0x3007bdd0 = 32.0f (SCROLLBAR_SIZE*2)
//   0x3007c05c = 15.0f  0x3007be40 = 4.0f   0x3007be5c = 3.0f
//
// The element-drawing loops write listBox->endPos (+0x04) each iteration and
// listBox->drawPadding (+0x08) at the break; these +0x04/+0x08 stores are named by
// the established listBoxDef_t offsets even though canonical Q3 uses them to publish
// the visible-element run — the machine writes exactly these offsets, so they are
// preserved as-is (see per-store comments).

#include "ui_runtime.h"

#include "compat/coduo_fp_conversion.h"
#include "compat/coduo_int32_bits.h"
#include "ui_menu_globals.h"

#include <stdint.h>
#include <string.h>

extern displayContextDef_t *DC;
void Com_Printf(const char *format, ...);

/* Feeder-row image inset/highlight math reuses SCROLLBAR_SIZE-adjacent float
 * constants; there are no additional magic numbers in this function. */

void Item_ListBox_Paint(itemDef_t *item)
{
    listBoxDef_t *listBox;
    int32_t count;   /* EBX: DC->feederCount(item->special) */
    int32_t i;       /* EBP: current element index */
    float   thumb;   /* clamped scrollbar-thumb draw coordinate */
    float   x;       /* running paint x (rect.x + 1, advanced by elementWidth) */
    float   y;       /* running paint y (rect.y + 1, advanced by elementHeight) */
    float   remaining; /* remaining scroll-axis length budget for elements */
    long double thumbMax;

    /* 0x300576d6: only validated listbox items paint here; anything else is an
     * error and prints the shared ITEM_TYPE_LISTBOX diagnostic (0x3005780f). */
    if (item->typeValidated != ITEM_TYPE_LISTBOX) {
        Com_Printf("^1Menu Error: Expecting type: ITEM_TYPE_LISTBOX\n");
        return;
    }

    /* 0x300576e4: typeData is the listBoxDef; nothing to draw without it. */
    listBox = (listBoxDef_t *)item->typeData;
    if (listBox == NULL) {
        return;   /* 0x300576ec JZ 0x30057e13 -> shared epilogue */
    }

    /* 0x30057701: element count of the item's data feeder. */
    {
        float special = item->special;
        displayContextDef_t *display = DC;
        count = display->feederCount(special);
    }

    if (item->window.flags & WINDOW_HORIZONTAL) {
        /* ================= Horizontal listbox ================= */
        /* Scroll axis is X; arrows sit at the bottom-left/right of the item. */

        float leftX = (float)((long double)item->window.rect.x + 1.0f);
        float barY =
            (float)((long double)item->window.rect.h + item->window.rect.y -
                    SCROLLBAR_SIZE - 1.0f);
        displayContextDef_t *display = DC;
        qhandle_t shader = display->scrollBarArrowLeft;

        /* 0x3005775e: left arrow; display was reloaded after feederCount. */
        display->drawHandlePic(leftX, barY, SCROLLBAR_SIZE, SCROLLBAR_SIZE,
                               shader);

        /* 0x300577a4: the track, spanning (rect.w - 32) + 1 between the two
         * arrows. Its y is the SAME EBP value computed once at 0x3005773e-
         * 0x3005774e (rect.y + rect.h - 16 - 1) and re-pushed at 0x300577a2;
         * the width slot gets size + 1.0f (0x30057795 FLD; FADD [0x3007bce0];
         * FSTP [ESP]). */
        float trackX = (float)((long double)leftX + 15.0f);
        display = DC;
        shader = display->scrollBar;
        float trackWidthBase =
            (float)((long double)item->window.rect.w - 32.0f);
        float trackWidth = (float)((long double)trackWidthBase + 1.0f);
        display->drawHandlePic(trackX, barY, trackWidth, SCROLLBAR_SIZE,
                               shader);

        /* 0x300577d5: right arrow at the far end of the track; y is the same
         * shared EBP again (0x300577cb PUSH EBP). */
        float rightX =
            (float)((long double)trackWidthBase - 1.0f + trackX);
        display = DC;
        shader = display->scrollBarArrowRight;
        display->drawHandlePic(rightX, barY, SCROLLBAR_SIZE, SCROLLBAR_SIZE,
                               shader);

        /* 0x300577db: thumb draw position, clamped so the thumb never runs past
         * the far end of the track (0x30057800 FCOMP/clamp). The clamp base is
         * the right-arrow x saved into the [ESP+0x14] slot at 0x300577cc
         * (rect.x + rect.w - 17), minus another 16 and 1 (0x300577ec-0x300577f6). */
        thumb = (float)Item_ListBox_ThumbDrawPosition(item);
        thumbMax = (long double)rightX - SCROLLBAR_SIZE - 1.0f;
        if ((long double)thumb > thumbMax) {
            thumb = (float)thumbMax;
        }
        /* 0x3005783f: draw the thumb. */
        display = DC;
        shader = display->scrollBarThumb;
        display->drawHandlePic(thumb, barY, SCROLLBAR_SIZE, SCROLLBAR_SIZE,
                               shader);

        /* 0x30057842: begin painting elements at the current scroll top. */
        i = listBox->startPos;
        listBox->endPos = i;                 /* 0x30057847: MOV [EDI+4],EBP */
        remaining = (float)((long double)item->window.rect.w - 2.0f);
        /* 0x30057856: the horizontal painter only runs for a selectable list. */
        if (listBox->elementStyle != LISTBOX_ELEMENT_IMAGE) {
            return;                          /* 0x3005785d JNE -> epilogue */
        }
        x = (float)((long double)item->window.rect.x + 1.0f); /* 0x30057865 */
        y = (float)((long double)item->window.rect.y + 1.0f); /* 0x30057871 */
        if (i >= count) {                        /* 0x3005787e JGE -> epilogue */
            return;
        }

        for (;;) {
            /* 0x30057891: image for this row (image feeder), or 0 for none. */
            int32_t image = DC->feederItemImage(item->special, i);
            if (image != 0) {
                /* 0x300578dd: draw the row image inset by 1px, sized to the
                 * element cell minus 2px. */
                DC->drawHandlePic(x + 1.0f, y + 1.0f,
                                    listBox->elementWidth - 2.0f,
                                    listBox->elementHeight - 2.0f,
                                    image);
            }

            /* 0x300578e3: the selected element gets a border outline
             * (CMP EBP,[ESI+0x250] == item->cursorPos). */
            if (i == item->cursorPos) {
                /* 0x30057921: drawRect(x, y, elementWidth-1, elementHeight-1,
                 *             size=item->window.borderSize, &borderColor). */
                DC->drawRect(x, y,
                               listBox->elementWidth - 1.0f,
                               listBox->elementHeight - 1.0f,
                               item->window.borderSize,
                               item->window.borderColor);
            }

            /* 0x30057927-0x30057932: consume one element width from the budget.
             * FSUB then FST (keep) then FCOMP -- the DLL stores the ROUNDED remaining
             * but tests the UNROUNDED difference against elementWidth (Class 8). The
             * rounded remaining is what drawPadding and the next iteration read
             * (0x3005795f/0x30057927 FLD [ESP+0x18]). */
            {
                long double rem = (long double)remaining - listBox->elementWidth;
                remaining = (float)rem;
                if (rem < listBox->elementWidth) {
                    /* 0x3005795f: no room for another whole element -- publish the
                     * leftover run length (rounded) and stop. */
                    listBox->drawPadding = coduo_fp_to_i32_extended(remaining);
                    return;
                }
            }
            /* 0x3005793c: advance to the next element. */
            x = (float)((long double)x + listBox->elementWidth);
            listBox->endPos =
                coduo_int32_from_bits((uint32_t)listBox->endPos + 1u);
            i = coduo_int32_from_bits((uint32_t)i + 1u);
            if (i >= count) {                        /* 0x30057951 JL loop */
                return;
            }
        }
    } else {
        /* ================= Vertical listbox ================= */
        /* Scroll axis is Y; arrows sit at the top-left/right of the item. */

        client_ui_compat_sync_server_list_selection(item);

        float barX =
            (float)((long double)item->window.rect.w + item->window.rect.x -
                    SCROLLBAR_SIZE - 1.0f);
        float topY = (float)((long double)item->window.rect.y + 1.0f);
        displayContextDef_t *display = DC;
        qhandle_t shader = display->scrollBarArrowUp;

        /* 0x300579b0: up arrow. */
        display->drawHandlePic(barX, topY, SCROLLBAR_SIZE, SCROLLBAR_SIZE,
                               shader);

        /* 0x300579bd/0x300579bf: MOV EAX,[EDI]; MOV [EDI+4],EAX -- reset endPos to
         * startPos at the top of the vertical path, mirroring the horizontal path
         * (line 120). A prior pass omitted this; since the element loops below
         * read-modify-increment endPos (0x30057b96 / 0x30057dd7 MOV [EDI+4];INC), the
         * missing reset let endPos accumulate across frames instead of re-basing to
         * startPos each paint. */
        listBox->endPos = listBox->startPos;

        /* 0x300579fb: the track spanning (rect.h - 32) + 1 between the arrows.
         * Its x is the SAME EBP value computed once at 0x30057973-0x3005798f
         * (rect.x + rect.w - 16 - 1) and re-pushed at 0x300579fa; the height
         * slot gets size + 1.0f (0x300579e7 FLD; FADD [0x3007bce0]; FSTP [ESP]). */
        float trackY = (float)((long double)topY + 15.0f);
        display = DC;
        shader = display->scrollBar;
        float trackHeightBase =
            (float)((long double)item->window.rect.h - 32.0f);
        float trackHeight =
            (float)((long double)trackHeightBase + 1.0f);
        display->drawHandlePic(barX, trackY, SCROLLBAR_SIZE, trackHeight,
                               shader);

        /* 0x30057a2c: down arrow at the far (bottom) end of the track; x is the
         * same shared EBP again (0x30057a2b PUSH EBP). */
        float bottomY =
            (float)((long double)trackHeightBase - 1.0f + trackY);
        display = DC;
        shader = display->scrollBarArrowDown;
        display->drawHandlePic(barX, bottomY, SCROLLBAR_SIZE, SCROLLBAR_SIZE,
                               shader);

        /* 0x30057a32: thumb draw position, clamped to the track end. The clamp
         * base is the down-arrow y saved into the [ESP+0x10] slot at 0x30057a22
         * (rect.y + rect.h - 17), minus another 16 and 1 (0x30057a43-0x30057a4d). */
        thumb = (float)Item_ListBox_ThumbDrawPosition(item);
        thumbMax = (long double)bottomY - SCROLLBAR_SIZE - 1.0f;
        if ((long double)thumb > thumbMax) {
            thumb = (float)thumbMax;
        }
        /* 0x30057a84: draw the thumb (x = the shared EBP, 0x30057a83 PUSH EBP). */
        display = DC;
        shader = display->scrollBarThumb;
        display->drawHandlePic(barX, thumb, SCROLLBAR_SIZE, SCROLLBAR_SIZE,
                               shader);

        remaining = (float)((long double)item->window.rect.h - 2.0f);
        x = (float)((long double)item->window.rect.x + 1.0f);
        y = (float)((long double)item->window.rect.y + 1.0f);

        /* 0x30057a90/0x30057a96: MOV EAX,[EDI+0x18]; CMP EAX,0x1 — the vertical
         * image-vs-column split keys on elementStyle (+0x18), the same field the
         * horizontal gate tests at 0x30057856 (NOT notselectable at +0xe4). */
        if (listBox->elementStyle == LISTBOX_ELEMENT_IMAGE) {
            /* ---- Simple (single-item-per-row) vertical list ---- */
            /* 0x30057abc: clamp startPos so the last page cannot scroll past the
             * bottom: startPos = min(startPos, count - round(remaining/elementH) + 1). */
            if (count != 0) {
                int32_t visibleRows = coduo_fp_to_i32_extended(
                    (long double)remaining / listBox->elementHeight);
                int32_t maxTop = coduo_int32_from_bits(
                    (uint32_t)count - (uint32_t)visibleRows + 1u);
                if (listBox->startPos > maxTop) {
                    listBox->startPos = maxTop;
                }
            }
            if (listBox->startPos < 0) {          /* 0x30057ad9 */
                listBox->startPos = 0;
            }
            i = listBox->startPos;
            if (i >= count) {                     /* 0x30057ae6 JGE -> epilogue */
                return;
            }

            for (;;) {
                /* 0x30057aff: image for this row, or 0 for none. */
                int32_t image = DC->feederItemImage(item->special, i);
                if (image != 0) {
                    /* 0x30057b4b: draw the row image inset by 1px. */
                    DC->drawHandlePic(x + 1.0f, y + 1.0f,
                                        listBox->elementWidth - 2.0f,
                                        listBox->elementHeight - 2.0f,
                                        image);
                }
                /* 0x30057b51: selected element outline. */
                if (i == item->cursorPos) {
                    /* 0x30057b8c: drawRect(x, y, elementWidth-1, elementHeight-1,
                     *             size=borderSize, &borderColor). */
                    DC->drawRect(x, y,
                                   listBox->elementWidth - 1.0f,
                                   listBox->elementHeight - 1.0f,
                                   item->window.borderSize,
                                   item->window.borderColor);
                }
                /* 0x30057b92-0x30057ba4: endPos++, then FSUB [EDI+0x10]
                 * (elementWIDTH, NOT Height -- the DLL decrements the vertical budget
                 * by elementWidth here; +0x10=width/+0x14=height is proven by the
                 * drawHandlePic args and the maxTop FDIV [EDI+0x14]). Then FST (keep)
                 * and FCOMP [EDI+0x14] (elementHeight) on the UNROUNDED difference
                 * (Class 8). This width/height asymmetry is a preserved original
                 * quirk -- see the report. */
                listBox->endPos =
                    coduo_int32_from_bits((uint32_t)listBox->endPos + 1u);
                {
                    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
                    long double rem = (long double)remaining - listBox->elementWidth;
                    remaining = (float)rem;
                    if (rem < listBox->elementHeight) {
                        /* 0x30057e02: publish the leftover run (elementHeight -
                         * remaining, rounded) and stop. */
                        listBox->drawPadding =
                            coduo_fp_to_i32_extended(
                                (long double)listBox->elementHeight - remaining);
                        return;
                    }
                }
                /* 0x30057bb2: next row. The loop bound is the feeder count held
                 * in [esp+0x24] (stored once at entry, 0x30057717). */
                y = (float)((long double)y + listBox->elementHeight);
                i = coduo_int32_from_bits((uint32_t)i + 1u);
                if (i >= count) {             /* 0x30057bbe CMP EBP,[esp+0x24] */
                    return;
                }
            }
        } else {
            /* ---- Column (multi-cell) vertical list ---- */
            /* 0x30057bd2: same startPos clamp as the simple path. */
            if (count != 0) {
                int32_t visibleRows = coduo_fp_to_i32_extended(
                    (long double)remaining / listBox->elementHeight);
                int32_t maxTop = coduo_int32_from_bits(
                    (uint32_t)count - (uint32_t)visibleRows + 1u);
                if (listBox->startPos > maxTop) {
                    listBox->startPos = maxTop;
                }
            }
            if (listBox->startPos < 0) {          /* 0x30057bef */
                listBox->startPos = 0;
            }
            i = listBox->startPos;                /* 0x30057bfa EBX = row index */
            if (i >= count) {                     /* 0x30057bfc CMP; JGE epilogue */
                return;
            }

            /* The feeder callback owns this output. UI_FeederItemText writes
             * the -1 text sentinel at entry; the repaired CG_FeederItemText
             * now honors the same contract. */
            int32_t handle;

            for (;;) {
                if (listBox->numColumns > 0) {
                    /* 0x30057c25: walk the column descriptors for this row. */
                    int32_t column;
                    const columnInfo_t *col = &listBox->columnInfo[0];
                    for (column = 0; column < listBox->numColumns; column++) {
                        /* 0x30057c47: fetch this cell's text (and optional image
                         * handle) from the feeder. */
                        const char *text = DC->feederItemText(item->special, i,
                                                                column, &handle);
                        if (handle >= 0) {
                            /* 0x30057c8c: cell drawn as an image, a col->width
                             * square at (col->pos + x + 2, rowY + 3). Proven
                             * stack slots: x-arg = col->pos + x + 2.0f
                             * (0x30057c7c FILD [EBX-4]; FADD slot(x); FADD
                             * [0x3007bce4]=2.0f), y-arg = y + 3.0f (0x30057c6d
                             * FADD 3.0f); both size args are (float)col->width. */
                            float cellSize = (float)col->width;
                            float cellY = (float)((long double)y + 3.0f);
                            float cellX =
                                (float)((long double)col->pos + x + 2.0f);
                            DC->drawHandlePic(cellX, cellY, cellSize,
                                                cellSize, handle);
                        } else if (text != NULL) {
                            /* 0x30057ce8: cell drawn as text via drawText. Proven:
                             *   x = col->pos + item->textalignx + x + 4.0f
                             *       (0x30057cd2 FILD[EBX-4] + FADD[ESI+0xdc]
                             *        + slot(x) + FADD 4.0f),
                             *   y = item->textaligny + elementHeight + y
                             *       (0x30057c9e FLD[ESI+0xe0] + FADD[EDI+0x14]
                             *        + FADD slot(y)),
                             *   adjust=item->font, scale=item->textscale,
                             *   color=&item->window.foreColor, style=0,
                             *   limit=col->maxChars (0x30057ca4 MOV ECX,[EBX+0x4];
                             *   0x30057cab PUSH ECX), font=item->textStyle. */
                            float textY =
                                (float)((long double)item->textaligny +
                                        listBox->elementHeight + y);
                            float textX =
                                (float)((long double)col->pos +
                                        item->textalignx + x + 4.0f);
                            DC->drawText(
                                textX, textY,
                                item->font,
                                item->textscale,
                                item->window.foreColor,
                                text,
                                0, col->maxChars,
                                item->textStyle);

                            /* NOT_FROM_ORIGINAL_SOURCE: improved server rows
                             * may attach a second string for a subdued bot
                             * count without changing ordinary feeder text. */
                            if (handle == UI_FEEDER_TEXT_GREY_SUFFIX) {
                                const int32_t prefixLength =
                                    (int32_t)strlen(text);
                                const char *const suffix =
                                    text + prefixLength + 1;
                                if (suffix[0] != '\0' &&
                                    (col->maxChars <= 0 ||
                                     prefixLength < col->maxChars)) {
                                    const int32_t suffixLimit =
                                        col->maxChars > 0
                                            ? col->maxChars - prefixLength : 0;
                                    const float suffixX =
                                        textX + (float)DC->textWidth(
                                            text, item->font,
                                            item->textscale, prefixLength);
                                    const vec4_t suffixColor = {
                                        0.5f, 0.5f, 0.5f,
                                        item->window.foreColor[3]
                                    };
                                    DC->drawText(
                                        suffixX, textY,
                                        item->font,
                                        item->textscale,
                                        suffixColor,
                                        suffix,
                                        0, suffixLimit,
                                        item->textStyle);
                                }
                            }
                        }
                        col++;  /* advance one columnInfo_t (0xc bytes); 0x30057cf9 ADD EBX,0xc */
                    }
                } else {
                    /* 0x30057d03: single-column row — one feeder text cell. */
                    const char *text = DC->feederItemText(item->special, i,
                                                            0, &handle);
                    if (handle < 0 && text != NULL) {
                        /* 0x30057d6c: drawText(rowX + 4.0f, y + elementHeight,
                         *             adjust=font, scale=textscale,
                         *             &foreColor, text, 0, 0, font). */
                        float textY =
                            (float)((long double)y + listBox->elementHeight);
                        float textX = (float)((long double)x + 4.0f);
                        DC->drawText(
                            textX, textY,
                            item->font,
                            item->textscale,
                            item->window.foreColor,
                            text,
                            0, 0,
                            item->textStyle);
                    }
                }

                /* 0x30057d72: selected-row outline for the full-width row. */
                if (i == item->cursorPos) {
                    /* 0x30057dbc: DC->fillRect(x + 2.0f, y + 2.0f,
                     *             rect.w - 16 - 4, elementHeight(+0x14),
                     *             &item->window.outlineColor(+0xa4)). */
                    float fillWidth =
                        (float)((long double)item->window.rect.w -
                                SCROLLBAR_SIZE - 4.0f);
                    float fillX = (float)((long double)x + 2.0f);
                    float fillY = (float)((long double)y + 2.0f);
                    DC->fillRect(fillX, fillY, fillWidth,
                                   listBox->elementHeight,
                                   item->window.outlineColor);
                }

                /* 0x30057dc2: advance the vertical budget by one element height.
                 * Unlike the simple vertical loop (which increments endPos at
                 * 0x30057b9c BEFORE its compare), this loop breaks FIRST: the
                 * JNP at 0x30057dd5 reaches the drawPadding store without any
                 * endPos increment; only the continue path (0x30057dd7 MOV
                 * EDX,[EDI+0x4]; INC; ... MOV [EDI+0x4],EDX) increments it. */
                {
                    /* 0x30057dc2-0x30057dcd: FSUB elementHeight, FST (keep), FCOMP
                     * [EDI+0x14] on the UNROUNDED difference (Class 8). */
                    long double rem = (long double)remaining - listBox->elementHeight;
                    remaining = (float)rem;
                    if (rem < listBox->elementHeight) {
                        /* 0x30057e02: publish the leftover run and stop. */
                        listBox->drawPadding =
                            coduo_fp_to_i32_extended(
                                (long double)listBox->elementHeight - remaining);
                        return;
                    }
                }
                listBox->endPos =
                    coduo_int32_from_bits((uint32_t)listBox->endPos + 1u);
                /* 0x30057dd7: next row. The loop bound is the feeder count held
                 * in a stack slot ([esp+0x24]); the row index is [esp+0x20]. */
                y = (float)((long double)y + listBox->elementHeight);
                i = coduo_int32_from_bits((uint32_t)i + 1u);
                if (i >= count) {                 /* 0x30057de7 CMP; JL 0x30057c10 */
                    return;
                }
            }
        }
    }
}
