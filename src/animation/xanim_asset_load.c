#include "xanim_asset_load.h"
#include "animation_private.h"
#include "qcommon/com_sprintf.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

enum {
    XANIM_PART_REMAP_SENTINEL = 127,
    XANIM_PART_REMAP_STRING_TYPE = 11,
    XANIM_FILE_VERSION = 14,
    XANIM_FILE_LOOPED_FLAG = 1,
    XANIM_FILE_DELTA_MOTION_FLAG = 2,
    XANIM_PART_NAME_STRING_TYPE = 9,
    XANIM_NOTETRACK_STRING_USER = 0,
    XANIM_NOTETRACK_STRING_TYPE = 3,
    XANIM_PACKED_UNIT_VECTOR_REMAINDER = 1073676289,
    XANIM_PART_COUNT_LIMIT = DOBJ_MAX_BONES,
    XANIM_INTERNED_NAME_SIZE_LIMIT = UINT8_MAX
};

/* NOT_FROM_ORIGINAL_SOURCE: carry the loaded-file extent with the XAnim
 * cursor so every serialized field is proved to fit before consumption. */
typedef struct xanim_load_cursor_s {
    const uint8_t *position;
    size_t remaining;
    const char *animName;
} xanim_load_cursor_t;

/* NOT_FROM_ORIGINAL_SOURCE: common malformed-file exit for all bounded XAnim
 * cursor operations. ERR_DROP does not return on either engine; the terminal
 * loop only makes that contract explicit to portable C compilers. */
static _Noreturn void xanim_load_fail(const xanim_load_cursor_t *cursor, const char *reason)
{
    Com_Error(ERR_DROP,
              "\x15"
              "malformed xanim '%s': %s",
              cursor->animName, reason);
    for (;;) {
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: consume one proven in-file byte range. */
static const uint8_t *xanim_load_take(xanim_load_cursor_t *cursor, size_t size)
{
    if (size > cursor->remaining) {
        xanim_load_fail(cursor, "truncated data");
    }

    const uint8_t *data = cursor->position;
    cursor->position += size;
    cursor->remaining -= size;
    return data;
}

/* NOT_FROM_ORIGINAL_SOURCE: consume a NUL-terminated string whose terminator
 * is proven to lie inside the retained file extent. */
static const char *xanim_load_take_string(xanim_load_cursor_t *cursor, size_t *size)
{
    const uint8_t *terminator = memchr(cursor->position, '\0', cursor->remaining);
    if (terminator == NULL) {
        xanim_load_fail(cursor, "unterminated string");
    }

    *size = (size_t)(terminator - cursor->position) + 1U;
    return (const char *)xanim_load_take(cursor, *size);
}

/* NOT_FROM_ORIGINAL_SOURCE: XAnim names become script-string entries whose
 * physical byte count is one byte, including the terminator. */
static const char *xanim_load_take_interned_string(xanim_load_cursor_t *cursor, size_t *size)
{
    const char *text = xanim_load_take_string(cursor, size);
    if (*size > XANIM_INTERNED_NAME_SIZE_LIMIT) {
        xanim_load_fail(cursor, "interned name exceeds string-pool capacity");
    }
    return text;
}

/* CoDUOMP.exe 0x00495b90..0x00495bc4 and coduo_lnxded retain the same
 * allocation and assignment. */
void XAnimBeginLoadFiles(void)
{
    xanim_rootTreeHandle = Scr_AllocArray();
}

/* NOT_FROM_ORIGINAL_SOURCE: readable wrapper for the packed part-bit tests in
 * XAnimLoadFile. */
static qboolean xanim_load_part_bit_is_set(const uint8_t *partBits, int32_t partIndex)
{
    return (partBits[partIndex >> 3] & (uint8_t)(1U << (partIndex & 7))) != 0;
}

/* NOT_FROM_ORIGINAL_SOURCE: local factoring of unaligned file-stream reads. */
static uint8_t xanim_load_read_byte(xanim_load_cursor_t *cursor)
{
    return *xanim_load_take(cursor, sizeof(uint8_t));
}

/* NOT_FROM_ORIGINAL_SOURCE: local factoring of bounded unaligned file reads. */
static uint16_t xanim_load_read_unsigned_short(xanim_load_cursor_t *cursor)
{
    uint16_t value;

    memcpy(&value, xanim_load_take(cursor, sizeof(value)), sizeof(value));
    return value;
}

/* NOT_FROM_ORIGINAL_SOURCE: local factoring of bounded unaligned file reads. */
static int16_t xanim_load_read_signed_short(xanim_load_cursor_t *cursor)
{
    int16_t value;

    memcpy(&value, xanim_load_take(cursor, sizeof(value)), sizeof(value));
    return value;
}

/* NOT_FROM_ORIGINAL_SOURCE: local factoring of bounded unaligned file reads. */
static float xanim_load_read_float(xanim_load_cursor_t *cursor)
{
    float value;

    memcpy(&value, xanim_load_take(cursor, sizeof(value)), sizeof(value));
    return value;
}

/* CoDUOMP.exe 0x0049cd80..0x0049cdef and coduo_lnxded
 * 0x080c1876..0x080c1965 implement the same packed quaternion expansion.
 * ReadQuat is the exact supporting Mac symbol.  The authority targets are
 * little-endian; their XModelLittleInt16 calls/identity helpers therefore do
 * not alter the three serialized lanes. */
void ReadQuat(const int16_t packed[3], xanim_int16_vec4_t *out)
{
    out->components[0] = packed[0];
    out->components[1] = packed[1];
    out->components[2] = packed[2];

    uint32_t remainingBits = XANIM_PACKED_UNIT_VECTOR_REMAINDER;
    remainingBits -= (uint32_t)(int32_t)out->components[2] * (uint32_t)(int32_t)out->components[2];
    remainingBits -= (uint32_t)(int32_t)out->components[1] * (uint32_t)(int32_t)out->components[1];
    remainingBits -= (uint32_t)(int32_t)out->components[0] * (uint32_t)(int32_t)out->components[0];
    int32_t remaining;
    memcpy(&remaining, &remainingBits, sizeof(remaining));
    out->components[3] = remaining > 0 ? (int16_t)(int32_t)floor(sqrt((double)remaining) + 0.5) : 0;
}

/* CoDUOMP.exe 0x0049cdf0..0x0049ce38 and coduo_lnxded
 * 0x080c1966..0x080c19f3 agree; ReadQuat2 is the exact Mac symbol. */
void ReadQuat2(const int16_t packed[1], xanim_int16_vec2_t *out)
{
    out->components[0] = packed[0];
    uint32_t remainingBits =
        XANIM_PACKED_UNIT_VECTOR_REMAINDER - (uint32_t)(int32_t)out->components[0] * (uint32_t)(int32_t)out->components[0];
    int32_t remaining;
    memcpy(&remaining, &remainingBits, sizeof(remaining));
    out->components[1] = remaining > 0 ? (int16_t)(int32_t)floor(sqrt((double)remaining) + 0.5) : 0;
}

/* NOT_FROM_ORIGINAL_SOURCE: local factoring of XAnimLoadFile's packed
 * quaternion reads. */
static void xanim_load_read_vec2(xanim_load_cursor_t *cursor, qboolean negate, xanim_int16_vec2_t *out)
{
    int16_t packed;

    memcpy(&packed, xanim_load_take(cursor, sizeof(packed)), sizeof(packed));
    ReadQuat2(&packed, out);
    if (negate) {
        out->components[0] = (int16_t)-out->components[0];
        out->components[1] = (int16_t)-out->components[1];
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: local factoring of XAnimLoadFile's packed
 * quaternion reads. */
static void xanim_load_read_vec4(xanim_load_cursor_t *cursor, qboolean negate, xanim_int16_vec4_t *out)
{
    int16_t packed[3];

    memcpy(packed, xanim_load_take(cursor, sizeof(packed)), sizeof(packed));
    ReadQuat(packed, out);
    if (negate) {
        out->components[0] = (int16_t)-out->components[0];
        out->components[1] = (int16_t)-out->components[1];
        out->components[2] = (int16_t)-out->components[2];
        out->components[3] = (int16_t)-out->components[3];
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: local factoring of the quaternion sign-continuity
 * pass at 0x004961e0 and 0x004968f0. */
static void xanim_load_fix_vec2_continuity(xanim_int16_vec2_t *frames, uint16_t frameCount)
{
    for (int32_t index = 1; index < frameCount; ++index) {
        xanim_int16_vec2_t *current = &frames[index];
        const xanim_int16_vec2_t *previous = &frames[index - 1];
        int32_t dot = (int32_t)previous->components[0] * current->components[0] + (int32_t)previous->components[1] * current->components[1];

        if (dot < 0) {
            current->components[0] = (int16_t)-current->components[0];
            current->components[1] = (int16_t)-current->components[1];
        }
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: local factoring of the quaternion sign-continuity
 * pass at 0x004961e0 and 0x004968f0. */
static void xanim_load_fix_vec4_continuity(xanim_int16_vec4_t *frames, uint16_t frameCount)
{
    for (int32_t index = 1; index < frameCount; ++index) {
        xanim_int16_vec4_t *current = &frames[index];
        const xanim_int16_vec4_t *previous = &frames[index - 1];
        int32_t dot = (int32_t)previous->components[0] * current->components[0] +
                      (int32_t)previous->components[1] * current->components[1] +
                      (int32_t)previous->components[2] * current->components[2] + (int32_t)previous->components[3] * current->components[3];

        if (dot < 0) {
            current->components[0] = (int16_t)-current->components[0];
            current->components[1] = (int16_t)-current->components[1];
            current->components[2] = (int16_t)-current->components[2];
            current->components[3] = (int16_t)-current->components[3];
        }
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of the variable-sized
 * rotation stream allocation visible in the original loader. */
static size_t xanim_load_rotation_stream_size(uint16_t frameCount, uint16_t totalFrameCount, qboolean smallFrameKeys)
{
    size_t size = offsetof(xanim_rotation_stream_t, tail) + sizeof(uint16_t);

    if (frameCount < totalFrameCount) {
        size_t keyedSize =
            offsetof(xanim_rotation_stream_t, tail) + (smallFrameKeys ? (size_t)frameCount + 1U : (size_t)frameCount * sizeof(uint16_t));
        if (keyedSize > size) {
            size = keyedSize;
        }
    }
    return size;
}

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of the variable-sized
 * translation stream allocation visible in the original loader. */
static size_t xanim_load_translation_stream_size(uint16_t frameCount, uint16_t totalFrameCount, qboolean smallFrameKeys)
{
    size_t size = offsetof(xanim_translation_stream_t, key) + sizeof(uint16_t);

    if (frameCount < totalFrameCount) {
        size_t keyedSize =
            offsetof(xanim_translation_stream_t, key) + (smallFrameKeys ? (size_t)frameCount + 1U : (size_t)frameCount * sizeof(uint16_t));
        if (keyedSize > size) {
            size = keyedSize;
        }
    }
    return size;
}

/* NOT_FROM_ORIGINAL_SOURCE: read one key from a bounded key span already
 * consumed from the file. */
static uint16_t xanim_load_key_value(const uint8_t *keys, size_t index, qboolean smallFrameKeys)
{
    if (smallFrameKeys != qfalse) {
        return keys[index];
    }

    uint16_t value;
    memcpy(&value, keys + index * sizeof(value), sizeof(value));
    return value;
}

/* NOT_FROM_ORIGINAL_SOURCE: XAnimFindByteKey/XAnimFindShortKey require sparse
 * keys to start at zero, increase strictly, and cover the last animation
 * frame. All 4,263 stock XAnim files satisfy this serialized invariant. */
static void xanim_load_validate_keys(xanim_load_cursor_t *cursor, const uint8_t *keys, uint16_t frameCount, uint16_t totalFrameCount,
                                     qboolean smallFrameKeys)
{
    uint16_t previous = xanim_load_key_value(keys, 0, smallFrameKeys);
    if (previous != 0) {
        xanim_load_fail(cursor, "first key is not frame zero");
    }

    for (uint16_t index = 1; index < frameCount; ++index) {
        uint16_t current = xanim_load_key_value(keys, index, smallFrameKeys);
        if (current <= previous) {
            xanim_load_fail(cursor, "animation keys are not increasing");
        }
        previous = current;
    }

    if (previous != (uint16_t)(totalFrameCount - 1U)) {
        xanim_load_fail(cursor, "last key is not the final frame");
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of the two repeated key
 * copy branches in XAnimLoadFile. */
static void xanim_load_copy_rotation_keys(xanim_rotation_stream_t *stream, xanim_load_cursor_t *cursor, uint16_t frameCount,
                                          uint16_t totalFrameCount, qboolean smallFrameKeys)
{
    if (frameCount >= totalFrameCount) {
        return;
    }

    size_t bytes = smallFrameKeys ? (size_t)frameCount : (size_t)frameCount * sizeof(uint16_t);
    const uint8_t *keys = xanim_load_take(cursor, bytes);
    xanim_load_validate_keys(cursor, keys, frameCount, totalFrameCount, smallFrameKeys);
    memcpy(stream->tail.byteKeys, keys, bytes);
}

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of the two repeated key
 * copy branches in XAnimLoadFile. */
static void xanim_load_copy_translation_keys(xanim_translation_stream_t *stream, xanim_load_cursor_t *cursor, uint16_t frameCount,
                                             uint16_t totalFrameCount, qboolean smallFrameKeys)
{
    if (frameCount >= totalFrameCount) {
        return;
    }

    size_t bytes = smallFrameKeys ? (size_t)frameCount : (size_t)frameCount * sizeof(uint16_t);
    const uint8_t *keys = xanim_load_take(cursor, bytes);
    xanim_load_validate_keys(cursor, keys, frameCount, totalFrameCount, smallFrameKeys);
    memcpy(stream->key.byteKeys, keys, bytes);
}

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of the rotation-stream
 * parser repeated for delta motion and every animated part. */
static void xanim_load_parse_rotation_stream(xanim_load_cursor_t *cursor, xanim_rotation_stream_t **streamOut, uint16_t totalFrameCount,
                                             qboolean smallFrameKeys, qboolean compressedVec2, qboolean negate, xanim_asset_alloc_fn alloc)
{
    uint16_t frameCount = xanim_load_read_unsigned_short(cursor);
    xanim_rotation_stream_t *stream;

    if (frameCount > totalFrameCount) {
        xanim_load_fail(cursor, "rotation key count exceeds frame count");
    }

    if (frameCount == 0) {
        *streamOut = NULL;
        return;
    }

    if (frameCount == 1) {
        if (compressedVec2) {
            xanim_int16_vec2_t frame;

            xanim_load_read_vec2(cursor, negate, &frame);
            stream = alloc(offsetof(xanim_rotation_stream_t, tail) + sizeof(uint16_t));
            stream->data.inlinePrefix.lane0 = frame.components[0];
            stream->data.inlinePrefix.lane1 = frame.components[1];
        } else {
            xanim_int16_vec4_t frame;

            xanim_load_read_vec4(cursor, negate, &frame);
            stream = alloc(sizeof(*stream));
            stream->data.inlinePrefix.lane0 = frame.components[0];
            stream->data.inlinePrefix.lane1 = frame.components[1];
            stream->tail.inlineFull.lane2 = frame.components[2];
            stream->tail.inlineFull.lane3 = frame.components[3];
        }
        stream->frameIndex = 0;
        *streamOut = stream;
        return;
    }

    stream = alloc(xanim_load_rotation_stream_size(frameCount, totalFrameCount, smallFrameKeys));
    xanim_load_copy_rotation_keys(stream, cursor, frameCount, totalFrameCount, smallFrameKeys);
    if (compressedVec2) {
        xanim_int16_vec2_t *frames = alloc((size_t)frameCount * sizeof(*frames));

        stream->data.frames2 = frames;
        for (int32_t index = 0; index < frameCount; ++index) {
            /* 0x004967a3 applies the serialized sign only to frame zero;
             * 0x00496870 then propagates that choice through continuity. */
            xanim_load_read_vec2(cursor, index == 0 ? negate : qfalse, &frames[index]);
        }
        xanim_load_fix_vec2_continuity(frames, frameCount);
    } else {
        xanim_int16_vec4_t *frames = alloc((size_t)frameCount * sizeof(*frames));

        stream->data.frames4 = frames;
        for (int32_t index = 0; index < frameCount; ++index) {
            /* 0x004968f0 applies the serialized sign only to frame zero;
             * 0x00496a06 then propagates that choice through continuity. */
            xanim_load_read_vec4(cursor, index == 0 ? negate : qfalse, &frames[index]);
        }
        xanim_load_fix_vec4_continuity(frames, frameCount);
    }
    stream->frameIndex = (uint16_t)(frameCount - 1U);
    *streamOut = stream;
}

/* NOT_FROM_ORIGINAL_SOURCE: source-level factoring of the translation-stream
 * parser repeated for delta motion and every animated part. */
static void xanim_load_parse_translation_stream(xanim_load_cursor_t *cursor, xanim_translation_stream_t **streamOut,
                                                uint16_t totalFrameCount, qboolean smallFrameKeys, xanim_asset_alloc_fn alloc)
{
    uint16_t frameCount = xanim_load_read_unsigned_short(cursor);
    xanim_translation_stream_t *stream;

    if (frameCount > totalFrameCount) {
        xanim_load_fail(cursor, "translation key count exceeds frame count");
    }

    if (frameCount == 0) {
        *streamOut = NULL;
        return;
    }

    if (frameCount == 1) {
        stream = alloc(sizeof(*stream));
        stream->data.inlineLane0 = xanim_load_read_float(cursor);
        stream->frameIndex = 0;
        stream->inlineLanes.lane1 = xanim_load_read_float(cursor);
        stream->inlineLanes.lane2 = xanim_load_read_float(cursor);
        *streamOut = stream;
        return;
    }

    stream = alloc(xanim_load_translation_stream_size(frameCount, totalFrameCount, smallFrameKeys));
    xanim_load_copy_translation_keys(stream, cursor, frameCount, totalFrameCount, smallFrameKeys);
    vec3_t *frames = alloc((size_t)frameCount * sizeof(*frames));

    stream->data.frames = frames;
    for (int32_t index = 0; index < frameCount; ++index) {
        frames[index][0] = xanim_load_read_float(cursor);
        frames[index][1] = xanim_load_read_float(cursor);
        frames[index][2] = xanim_load_read_float(cursor);
    }
    stream->frameIndex = (uint16_t)(frameCount - 1U);
    *streamOut = stream;
}

/* Source: CoDUOMP.exe 0x00495e10..0x00495ecb.
 * Name: same-module Mac symbol ReadNoteTracks. */
const uint8_t *ReadNoteTracks(const char *animName, const uint8_t *cursor, size_t remaining, XAnimParts *record, xanim_asset_alloc_fn alloc)
{
    /* NOT_FROM_ORIGINAL_SOURCE: the recovered cursor carries the retained file
     * extent through every note-name and frame-word consumption. */
    xanim_load_cursor_t noteCursor = {cursor, remaining, animName};
    uint8_t noteTrackCount = xanim_load_read_byte(&noteCursor);
    xanim_notetrack_t *noteTracks =
        alloc((size_t)noteTrackCount * sizeof(noteTracks[0]) + sizeof(noteTracks[0]) + sizeof(noteTracks[0].nameHandle));
    record->noteTracks = noteTracks;

    for (int32_t index = 0; index < noteTrackCount; ++index) {
        size_t nameSize;
        const char *name = xanim_load_take_interned_string(&noteCursor, &nameSize);
        noteTracks[index].nameHandle = SL_GetStringOfLen(name, XANIM_NOTETRACK_STRING_USER, nameSize, XANIM_NOTETRACK_STRING_TYPE);
        uint16_t frame = xanim_load_read_unsigned_short(&noteCursor);

        noteTracks[index].time = record->frameCountMinusOne != 0 ? (float)frame / (float)record->frameCountMinusOne : 0.0f;
    }

    noteTracks[noteTrackCount].nameHandle =
        SL_GetStringOfLen("end", XANIM_NOTETRACK_STRING_USER, sizeof("end"), XANIM_NOTETRACK_STRING_TYPE);
    noteTracks[noteTrackCount].time = 1.0f;
    noteTracks[noteTrackCount + 1].nameHandle = 0;
    return noteCursor.position;
}

/* Source: CoDUOMP.exe 0x00495cc0..0x00495e07 and coduo_lnxded's matching
 * body.  Name: exact supporting Mac symbol XAnimFreeMemory. */
void XAnimFreeMemory(fileData_t *entry)
{
    XAnimParts *record = entry->data.xanimParts;

    if (xanim_rootTreeHandle != 0) {
        uint16_t nameHandle = SL_GetLowercaseString_(entry->name, 0, 7);

        (void)GetVariable(xanim_rootTreeHandle, nameHandle);
        SL_RemoveRefToString(nameHandle);
    }

    uint16_t *partNames = record->partNameHandles;
    int32_t partCount = (int16_t)partNames[0];

    for (int32_t index = 0; index < partCount; ++index) {
        SL_RemoveRefToString(partNames[index + 1]);
    }

    xanim_notetrack_t *noteTrack = record->noteTracks;
    if (noteTrack != NULL) {
        while (noteTrack->nameHandle != 0) {
            SL_RemoveRefToString(noteTrack->nameHandle);
            ++noteTrack;
        }
    }
}

/* Source: CoDUOMP.exe 0x00495bd0..0x00495cb9.
 * Name: reconstructed Linux engine XAnimLoadPendingFiles. */
void XAnimLoadPendingFiles(xanim_asset_alloc_fn alloc)
{
    for (uint16_t child = FindNextSibling(xanim_rootTreeHandle); child != 0; child = FindNextSibling(child)) {
        uint16_t nameHandle = (uint16_t)GetVariableName(child);

        XAnimLoadFile(SL_ConvertToString(nameHandle), alloc);
    }

    /* Windows' ScriptVariable_Release and both binaries' RemoveRefToObject
     * have the same object-release behavior; the latter is the shared source
     * identity retained by the supporting Mac binary. */
    RemoveRefToObject(xanim_rootTreeHandle);
    xanim_rootTreeHandle = 0;
}

/* Source: CoDUOMP.exe 0x00495ed0..0x00496cfc.
 * Name: same-module Mac symbol XAnimLoadFile. The repeated stream parsing in
 * the machine function is factored into the marked local helpers above. */
void XAnimLoadFile(const char *animName, xanim_asset_alloc_fn alloc)
{
    fileData_t *entry = FS_GetDataForFile("xanim", animName, "");

    if (entry == NULL) {
        Com_Error(ERR_DROP,
                  "\x15"
                  "Cannot precache 'xanim/%s'.",
                  animName);
        return;
    }
    if (entry->data.xanimParts != NULL) {
        return;
    }

    char path[MAX_STRING_CHARS];
    void *fileBuffer;

    /* NOT_FROM_ORIGINAL_SOURCE: require the complete animation path and NUL to
     * fit; never substitute a truncated asset name. */
    if (strlen(animName) > sizeof(path) - sizeof("xanim/")) {
        Com_Error(ERR_DROP, "\x15"
                            "XAnim path is too long");
        return;
    }
    Com_sprintf(path, sizeof(path), "xanim/%s", animName);
    const int32_t fileLength = FS_ReadFile(path, &fileBuffer);
    if (fileLength < 0) {
        Com_Error(ERR_DROP,
                  "\x15"
                  "Cannot find 'xanim/%s'.",
                  animName);
        return;
    }

    /* NOT_FROM_ORIGINAL_SOURCE: retain the file byte count in the common
     * cursor so every serialized field is proved before reading or copying. */
    xanim_load_cursor_t cursor = {(const uint8_t *)fileBuffer, (size_t)fileLength, animName};
    int32_t version = xanim_load_read_signed_short(&cursor);
    if (version != XANIM_FILE_VERSION) {
        FS_FreeFile(fileBuffer);
        Com_Error(ERR_DROP,
                  "\x15"
                  "xanim '%s' out of date (version %d, expecting %d)",
                  animName, version, XANIM_FILE_VERSION);
        return;
    }

    uint16_t fileFrameCount = xanim_load_read_unsigned_short(&cursor);
    int32_t partCount = xanim_load_read_signed_short(&cursor);
    if (partCount < 0 || partCount > XANIM_PART_COUNT_LIMIT) {
        xanim_load_fail(&cursor, "invalid part count");
    }
    size_t partNameStorageSize = ((size_t)partCount + 1U) * sizeof(uint16_t);
    uint16_t *partNames = alloc(partNameStorageSize);

    partNames[0] = (uint16_t)partCount;

    uint8_t flags = xanim_load_read_byte(&cursor);
    uint8_t looped = flags & XANIM_FILE_LOOPED_FLAG;
    uint8_t hasDeltaMotion = (uint8_t)((flags & XANIM_FILE_DELTA_MOTION_FLAG) >> 1);
    int16_t frameRate = xanim_load_read_signed_short(&cursor);
    XAnimParts *record = alloc(sizeof(*record));

    record->partNameHandles = partNames;
    record->frameRate = (float)frameRate;
    record->looped = looped;
    record->hasDeltaMotion = hasDeltaMotion;

    uint16_t totalFrameCount = fileFrameCount;
    if (looped != 0) {
        if (fileFrameCount == UINT16_MAX) {
            xanim_load_fail(&cursor, "looped frame count overflows");
        }
        ++totalFrameCount;
    } else if (fileFrameCount == 0) {
        xanim_load_fail(&cursor, "invalid frame count");
    }
    qboolean smallFrameKeys = totalFrameCount <= XANIM_SMALL_FRAME_KEY_LIMIT;

    record->frameCountMinusOne = (uint16_t)(totalFrameCount - 1U);
    record->frequency = record->frameCountMinusOne == 0 ? 0.0f : record->frameRate / (float)record->frameCountMinusOne;

    if (hasDeltaMotion != 0) {
        xanim_part_stream_pair_t *deltaMotion = alloc(sizeof(*deltaMotion));

        record->deltaMotion = deltaMotion;
        xanim_load_parse_rotation_stream(&cursor, &deltaMotion->rotation, totalFrameCount, smallFrameKeys, qtrue, qfalse, alloc);
        xanim_load_parse_translation_stream(&cursor, &deltaMotion->translation, totalFrameCount, smallFrameKeys, alloc);
    }

    size_t bitsetSize = ((size_t)partCount + 7U) / 8U;
    const uint8_t *signBits = xanim_load_take(&cursor, bitsetSize);

    uint8_t *compressedRotationBits = alloc(bitsetSize);
    memcpy(compressedRotationBits, xanim_load_take(&cursor, bitsetSize), bitsetSize);
    record->compressedRotationBits = compressedRotationBits;

    for (int32_t index = 0; index < partCount; ++index) {
        size_t nameSize;
        const char *name = xanim_load_take_interned_string(&cursor, &nameSize);

        partNames[index + 1] = SL_GetStringOfLen(name, 0, nameSize, XANIM_PART_NAME_STRING_TYPE);
    }

    size_t partStreamStorageSize = (size_t)partCount * sizeof(record->partStreamPairs[0]);
    record->partStreamPairs = alloc(partStreamStorageSize);
    for (int32_t index = 0; index < partCount; ++index) {
        xanim_part_stream_pair_t *streams = &record->partStreamPairs[index];
        qboolean compressedRotation = xanim_load_part_bit_is_set(compressedRotationBits, index);

        xanim_load_parse_rotation_stream(&cursor, &streams->rotation, totalFrameCount, smallFrameKeys, compressedRotation,
                                         xanim_load_part_bit_is_set(signBits, index), alloc);
        if (compressedRotation == qfalse && streams->rotation == NULL) {
            xanim_load_fail(&cursor, "uncompressed part has no rotation");
        }
        xanim_load_parse_translation_stream(&cursor, &streams->translation, totalFrameCount, smallFrameKeys, alloc);
    }

    (void)ReadNoteTracks(animName, cursor.position, cursor.remaining, record, alloc);
    FS_FreeFile(fileBuffer);
    entry->data.xanimParts = record;
    entry->freeData = XAnimFreeMemory;
}

/* CoDUOMP.exe 0x00496e00..0x00496ef1 and coduo_lnxded
 * 0x080b9752..0x080b9857 build and intern the same 16-byte-mask remap.
 * Name: exact supporting Mac symbol XAnimSetModel. */
uint16_t XAnimSetModel(XAnimEntry *entry, XModel **models, int32_t modelCount)
{
    XAnimParts *record = entry->payload.leafAsset->data.xanimParts;
    uint16_t *animPartNameTable = record->partNameHandles;
    int32_t animPartCount = (int16_t)animPartNameTable[0];
    const uint16_t *animPartNames = animPartNameTable + 1;
    size_t remapSize = (size_t)((uint32_t)animPartCount + DOBJ_PART_REMAP_PREFIX_SIZE);
    uint32_t remapStorage[(remapSize + sizeof(uint32_t) - 1U) / sizeof(uint32_t)];
    XAnimToXModel *remap = (XAnimToXModel *)remapStorage;
    uint8_t *animToDObjPart = remap->boneIndex;

    Com_Memset(remap->partBits, 0, sizeof(remap->partBits));
    for (int32_t animPartIndex = animPartCount - 1; animPartIndex >= 0; --animPartIndex) {
        animToDObjPart[animPartIndex] = XANIM_PART_REMAP_SENTINEL;
    }

    int32_t sourcePartIndex = 0;
    for (int32_t modelIndex = 0; modelIndex < modelCount; ++modelIndex) {
        const uint16_t *modelPartNames = XModelBoneNames(models[modelIndex]);
        int32_t modelPartCount = XModelNumBones(models[modelIndex]);

        for (int32_t modelPartIndex = 0; modelPartIndex < modelPartCount; ++modelPartIndex) {
            uint16_t modelPartName = modelPartNames[modelPartIndex];

            for (int32_t animPartIndex = animPartCount - 1; animPartIndex >= 0; --animPartIndex) {
                if (modelPartName != animPartNames[animPartIndex]) {
                    continue;
                }

                if (animToDObjPart[animPartIndex] == XANIM_PART_REMAP_SENTINEL) {
                    animToDObjPart[animPartIndex] = (uint8_t)sourcePartIndex;
                    remap->partBits[sourcePartIndex >> 3] |= (uint8_t)(1U << (sourcePartIndex & 7));
                }
                break;
            }
            ++sourcePartIndex;
        }
    }

    return SL_GetStringOfLen((const char *)remap, 0, remapSize, XANIM_PART_REMAP_STRING_TYPE);
}
