#include "xmodel.h"

#include "animation_private.h"

/*
 * Complete XModel asset-release callback cluster.
 *
 * Windows authority: CoDUOMP.exe 0x0049cf70..0x0049d084.
 * Linux authority: coduo_lnxded 0x080c19f4..0x080c1b0e.
 * Both engines retain the same no-op surface callback, release the same part
 * name handles in ascending order, and release three LOD surface-name tables
 * in the same order before clearing each table pointer.
 */

void XModelSurfsFree(fileData_t *fileData)
{
    (void)fileData;
}

void XModelPartsFree(fileData_t *fileData)
{
    XModelPartNameTable *table =
        fileData->data.xmodelParts->partNameTableSlot->partNameTable;

    for (int32_t partIndex = 0; partIndex < table->count; ++partIndex) {
        SL_RemoveRefToString(table->handles[partIndex]);
    }
}

void XModelFree(fileData_t *fileData)
{
    XModelInfo *modelInfo = fileData->data.xmodelInfo;

    for (int32_t lodIndex = 0; lodIndex < XMODEL_LOD_COUNT; ++lodIndex) {
        XModelLodInfo *lod = &modelInfo->lodRecords[lodIndex];

        if (lod->surfaceNameTable == NULL) {
            continue;
        }

        for (int32_t surfaceIndex = 0;
             surfaceIndex < lod->surfaceCount;
             ++surfaceIndex) {
            SL_RemoveRefToString(lod->surfaceNameTable[surfaceIndex]);
        }
        lod->surfaceNameTable = NULL;
    }
}
