#ifndef QCOMMON_FILE_DATA_H
#define QCOMMON_FILE_DATA_H

#include <stddef.h>
#include <stdint.h>

#include "asset_type_names.h"

typedef union fileDataPayload_u fileDataPayload;
typedef void (*fileDataFree_t)(fileData_t *fileData);

/* Exact tag recovered from the Mac XAnimFreeMemory, XModelFree,
 * XModelPartsFree, and XModelSurfsFree signatures.  FS_GetDataForFile returns
 * this common three-word view of either a loose-file record or the basename
 * subobject of a pack-file record.  The Windows client XAnimLoadFile at
 * CoDUOMP.exe 0x00495ed0 and Linux server body at coduo_lnxded 0x080b816e
 * independently use name at +0x00, typed payload at +0x04, and the free
 * callback at +0x08. */
union fileDataPayload_u {
    void *generic;
    XAnimParts *xanimParts;
    XModelInfo *xmodelInfo;
    XModelPartsData *xmodelParts;
    XModelSurfsData *xmodelSurfs;
};

struct fileData_s {
    const char *name;
    fileDataPayload data;
    fileDataFree_t freeData;
};

#if UINTPTR_MAX == UINT32_MAX
typedef char file_data_payload_size[
    sizeof(fileDataPayload) == 0x04 ? 1 : -1];
struct file_data_payload_alignment_probe_s {
    unsigned char byte;
    fileDataPayload value;
};
typedef char file_data_payload_alignment[
    offsetof(struct file_data_payload_alignment_probe_s, value) == 0x04
        ? 1 : -1];
typedef char file_data_name_offset[
    offsetof(fileData_t, name) == 0x00 ? 1 : -1];
typedef char file_data_payload_offset[
    offsetof(fileData_t, data) == 0x04 ? 1 : -1];
typedef char file_data_callback_offset[
    offsetof(fileData_t, freeData) == 0x08 ? 1 : -1];
typedef char file_data_size[sizeof(fileData_t) == 0x0c ? 1 : -1];
struct file_data_alignment_probe_s {
    unsigned char byte;
    fileData_t value;
};
typedef char file_data_alignment[
    offsetof(struct file_data_alignment_probe_s, value) == 0x04 ? 1 : -1];
#endif

#endif
