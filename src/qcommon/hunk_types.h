#ifndef QCOMMON_HUNK_TYPES_H
#define QCOMMON_HUNK_TYPES_H

#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
#define HUNK_TYPES_STATIC_ASSERT static_assert
#define HUNK_TYPES_ALIGNOF alignof
#else
#define HUNK_TYPES_STATIC_ASSERT _Static_assert
#define HUNK_TYPES_ALIGNOF _Alignof
#endif

#define HUNK_TEMP_MAGIC UINT32_C(0x89537892)
#define HUNK_TEMP_FREED_MAGIC UINT32_C(0x89537893)

enum {
    HUNK_ALIGNMENT = 32,
    HUNK_TEMP_ALIGNMENT = 16
};

/* CoDUOMP.exe and coduo_lnxded agree on the complete 0x18-byte i386 debug
 * log node: size +0x00, printed +0x04, next +0x08, label +0x0c, source file
 * +0x10, and source line +0x14. Linux Hunk_Log at 0x0806bd84 walks the
 * linked records and Hunk_SmallLog at 0x0806bedd groups them by source. */
typedef struct hunk_log_block_s {
    size_t size;
    uint8_t printed;
    uint8_t padding05[3];
    struct hunk_log_block_s *next;
    const char *label;
    const char *sourceFile;
    int32_t sourceLine;
} hunk_log_block_t;

/* Both original i386 engines reserve a 0x10-byte header before temporary
 * hunk payloads and access only magic +0x00 and the size delta +0x04. Native
 * hosts widen the size field while retaining the same 16-byte header extent. */
typedef struct hunk_temp_header_s {
    uint32_t magic;
    size_t sizeDelta;
#if UINTPTR_MAX == UINT32_MAX
    uint8_t unused08[8];
#endif
} hunk_temp_header_t;

/*
 * The stock client and dedicated-server hunk allocators retain the same
 * hunk_state_t name but not the same private state layout.  The client layout
 * is proven by CoDUOMP.exe 0x004355b0..0x00435b41 and the contiguous globals
 * at 0x0096786c..0x0096789b.  The Linux layout is proven by coduo_lnxded
 * 0x0806c0c7..0x0806c38d and the contiguous globals at
 * 0x08238548..0x08238568.  Select the complete original layout at the type
 * boundary; do not combine the two records into a synthetic superset.
 */
#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "hunk_state_t requires one platform behavior"
#elif defined(LINUX_BEHAVIOR)
typedef struct hunk_state_s {
    size_t lowMark;
    size_t lowTempMark;
    size_t lowUsed;
    size_t lowTemp;
    size_t highMark;
    size_t highTempMark;
    size_t highUsed;
    size_t highTemp;
    size_t totalSize;
} hunk_state_t;
#else
/* WINDOWS_BEHAVIOR is the default for client builds. */
typedef struct hunk_state_s {
    size_t totalSize;
    size_t totalZoneSize; /* Reported but never written by CoDUOMP.exe. */
    size_t highMark;
    size_t highTempMark;
    size_t highUsed;
    size_t highTemp;
    hunk_log_block_t *logBlocks; /* No CoDUOMP.exe producer; remains null. */
    int32_t logFile; /* No CoDUOMP.exe producer; leaves hunk logging off. */
    size_t lowMark;
    size_t lowTempMark;
    size_t lowUsed;
    size_t lowTemp;
} hunk_state_t;
#endif

#if UINTPTR_MAX == UINT32_MAX
HUNK_TYPES_STATIC_ASSERT(HUNK_TYPES_ALIGNOF(hunk_log_block_t) == 4, "i386 hunk-log block alignment changed");
HUNK_TYPES_STATIC_ASSERT(sizeof(hunk_log_block_t) == 0x18, "i386 hunk-log block extent changed");
HUNK_TYPES_STATIC_ASSERT(offsetof(hunk_log_block_t, size) == 0x00, "i386 hunk-log size moved");
HUNK_TYPES_STATIC_ASSERT(offsetof(hunk_log_block_t, printed) == 0x04, "i386 hunk-log printed flag moved");
HUNK_TYPES_STATIC_ASSERT(offsetof(hunk_log_block_t, next) == 0x08, "i386 hunk-log link moved");
HUNK_TYPES_STATIC_ASSERT(offsetof(hunk_log_block_t, label) == 0x0c, "i386 hunk-log label moved");
HUNK_TYPES_STATIC_ASSERT(offsetof(hunk_log_block_t, sourceFile) == 0x10, "i386 hunk-log source file moved");
HUNK_TYPES_STATIC_ASSERT(offsetof(hunk_log_block_t, sourceLine) == 0x14, "i386 hunk-log source line moved");
HUNK_TYPES_STATIC_ASSERT(HUNK_TYPES_ALIGNOF(hunk_temp_header_t) == 4, "i386 hunk temporary-header alignment changed");
HUNK_TYPES_STATIC_ASSERT(offsetof(hunk_temp_header_t, magic) == 0x00, "i386 hunk temporary magic moved");
HUNK_TYPES_STATIC_ASSERT(offsetof(hunk_temp_header_t, sizeDelta) == 0x04, "i386 hunk temporary size delta moved");
HUNK_TYPES_STATIC_ASSERT(offsetof(hunk_temp_header_t, unused08) == 0x08, "i386 hunk temporary unused tail moved");
#if defined(LINUX_BEHAVIOR)
HUNK_TYPES_STATIC_ASSERT(sizeof(hunk_state_t) == 0x24, "Linux i386 hunk-state extent changed");
HUNK_TYPES_STATIC_ASSERT(offsetof(hunk_state_t, lowMark) == 0x00, "Linux hunk low mark moved");
HUNK_TYPES_STATIC_ASSERT(offsetof(hunk_state_t, lowTempMark) == 0x04, "Linux hunk low temporary mark moved");
HUNK_TYPES_STATIC_ASSERT(offsetof(hunk_state_t, lowUsed) == 0x08, "Linux hunk low permanent use moved");
HUNK_TYPES_STATIC_ASSERT(offsetof(hunk_state_t, lowTemp) == 0x0c, "Linux hunk low temporary use moved");
HUNK_TYPES_STATIC_ASSERT(offsetof(hunk_state_t, highMark) == 0x10, "Linux hunk high mark moved");
HUNK_TYPES_STATIC_ASSERT(offsetof(hunk_state_t, highTempMark) == 0x14, "Linux hunk high temporary mark moved");
HUNK_TYPES_STATIC_ASSERT(offsetof(hunk_state_t, highUsed) == 0x18, "Linux hunk high permanent use moved");
HUNK_TYPES_STATIC_ASSERT(offsetof(hunk_state_t, highTemp) == 0x1c, "Linux hunk high temporary use moved");
HUNK_TYPES_STATIC_ASSERT(offsetof(hunk_state_t, totalSize) == 0x20, "Linux hunk total size moved");
#else
HUNK_TYPES_STATIC_ASSERT(HUNK_TYPES_ALIGNOF(hunk_state_t) == 4, "Windows i386 hunk-state alignment changed");
HUNK_TYPES_STATIC_ASSERT(sizeof(hunk_state_t) == 0x30, "Windows i386 hunk-state extent changed");
HUNK_TYPES_STATIC_ASSERT(offsetof(hunk_state_t, totalSize) == 0x00, "Windows hunk total size moved");
HUNK_TYPES_STATIC_ASSERT(offsetof(hunk_state_t, totalZoneSize) == 0x04, "Windows hunk total zone size moved");
HUNK_TYPES_STATIC_ASSERT(offsetof(hunk_state_t, highMark) == 0x08, "Windows hunk high mark moved");
HUNK_TYPES_STATIC_ASSERT(offsetof(hunk_state_t, highTempMark) == 0x0c, "Windows hunk high temporary mark moved");
HUNK_TYPES_STATIC_ASSERT(offsetof(hunk_state_t, highUsed) == 0x10, "Windows hunk high permanent use moved");
HUNK_TYPES_STATIC_ASSERT(offsetof(hunk_state_t, highTemp) == 0x14, "Windows hunk high temporary use moved");
HUNK_TYPES_STATIC_ASSERT(offsetof(hunk_state_t, logBlocks) == 0x18, "Windows hunk log list moved");
HUNK_TYPES_STATIC_ASSERT(offsetof(hunk_state_t, logFile) == 0x1c, "Windows hunk log file moved");
HUNK_TYPES_STATIC_ASSERT(offsetof(hunk_state_t, lowMark) == 0x20, "Windows hunk low mark moved");
HUNK_TYPES_STATIC_ASSERT(offsetof(hunk_state_t, lowTempMark) == 0x24, "Windows hunk low temporary mark moved");
HUNK_TYPES_STATIC_ASSERT(offsetof(hunk_state_t, lowUsed) == 0x28, "Windows hunk low permanent use moved");
HUNK_TYPES_STATIC_ASSERT(offsetof(hunk_state_t, lowTemp) == 0x2c, "Windows hunk low temporary use moved");
#endif
#endif
HUNK_TYPES_STATIC_ASSERT(sizeof(hunk_temp_header_t) == 0x10, "hunk temporary-header extent changed");

#undef HUNK_TYPES_STATIC_ASSERT
#undef HUNK_TYPES_ALIGNOF

#endif
