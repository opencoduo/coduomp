#ifndef CODUOMP_GENERIC_PARSER_HPP
#define CODUOMP_GENERIC_PARSER_HPP

#include <cstddef>
#include <cstdint>

class CTextPool {
public:
    void *operator new(std::size_t size);
    void operator delete(void *allocation) noexcept;

    explicit CTextPool(int32_t poolSize);
    ~CTextPool();

    CTextPool *GetNext() const;
    void SetNext(CTextPool *pool);

    char *AllocText(const char *text, bool terminateEntry,
                    CTextPool **activePool);

    char *buffer;
    CTextPool *next;
    int32_t capacity;
    int32_t used;
};

void CleanTextPool(CTextPool *pool);
char *GetToken(char **data, bool allowLineBreaks, bool readUntilEol);

/* Same-module Mac symbols identify the CGPObject/CGPGroup/CGPValue hierarchy.
 * Windows instructions prove that next is the insertion-order link, while
 * sortedNext/sortedPrevious form a second name-sorted view of the same objects.
 * CGPValue also uses CGPObject nodes for its value-string list. */
class CGPObject {
public:
    void *operator new(std::size_t size);
    void operator delete(void *allocation) noexcept;

    explicit CGPObject(const char *objectName);

    void SetNext(CGPObject *object);
    CGPObject *GetSortedNext() const;
    void SetSortedNext(CGPObject *object);
    void SetSortedPrevious(CGPObject *object);

    const char *name;
    CGPObject *next;
    CGPObject *sortedNext;
    CGPObject *sortedPrevious;
};

class CGPValue : public CGPObject {
public:
    CGPValue(const char *objectName, const char *initialValue);
    ~CGPValue();

    bool Parse(char **data, CTextPool **activePool);
    void AddValue(const char *value, CTextPool **activePool);
    const char *GetTopValue();
    bool IsList();
    bool Write(CTextPool **activePool, int32_t indentLevel);

    /* Exact original name is not present in the Mac traceback symbols. The
     * Windows body deep-copies this value and optionally moves its strings into
     * a supplied text pool. */
    CGPValue *Clone(CTextPool **activePool);

    CGPObject *values;
};

class CGPGroup : public CGPObject {
public:
    CGPGroup(const char *objectName, CGPGroup *parentGroup);
    ~CGPGroup();

    bool Parse(char **data, CTextPool **activePool);
    CGPGroup *AddGroup(CGPGroup *group);
    CGPGroup *AddGroup(const char *groupName, CTextPool **activePool);
    CGPValue *AddPair(CGPValue *pair);
    CGPValue *AddPair(const char *key, const char *value,
                      CTextPool **activePool);
    void SortObject(CGPObject *object, CGPObject **first,
                    CGPObject **sortedFirst, CGPObject **last);
    void Clean();
    int32_t GetNumSubGroups();
    int32_t GetNumPairs();
    CGPGroup *FindSubGroup(const char *groupName);
    CGPValue *FindPair(const char *key);
    const char *FindPairValue(const char *key, const char *defaultValue);
    void SetWriteable(bool value);
    bool Write(CTextPool **activePool, int32_t indentLevel);

    /* Provisional role name; see CGPValue::Clone. */
    CGPGroup *Clone(CTextPool **activePool);

    CGPObject *pairs;
    CGPObject *sortedPairs;
    CGPObject *lastPair;
    CGPObject *subGroups;
    CGPObject *sortedSubGroups;
    CGPObject *lastSubGroup;
    CGPGroup *parent;
    bool writeable;
};

class CGenericParser2 : public CGPGroup {
public:
    CGenericParser2();
    ~CGenericParser2();

    bool Parse(char **data, bool cleanFirst, bool makeWriteable);
    void Clean();
    void SetWriteable(bool value);
    bool Write(CTextPool *outputPool);

    CTextPool *textPool;
    bool parserWriteable;
};

#if UINTPTR_MAX == UINT32_MAX
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif
static_assert(sizeof(CTextPool) == 0x10,
              "i386 CTextPool size changed");
static_assert(offsetof(CTextPool, next) == 0x4,
              "i386 CTextPool next-pool offset changed");
static_assert(offsetof(CTextPool, capacity) == 0x8,
              "i386 CTextPool capacity offset changed");
static_assert(offsetof(CTextPool, used) == 0xc,
              "i386 CTextPool used-byte offset changed");
static_assert(sizeof(CGPObject) == 0x10,
              "i386 CGPObject size changed");
static_assert(offsetof(CGPObject, next) == 0x4,
              "i386 CGPObject insertion-link offset changed");
static_assert(offsetof(CGPObject, sortedNext) == 0x8,
              "i386 CGPObject sorted-next offset changed");
static_assert(offsetof(CGPObject, sortedPrevious) == 0xc,
              "i386 CGPObject sorted-previous offset changed");
static_assert(sizeof(CGPValue) == 0x14,
              "i386 CGPValue size changed");
static_assert(offsetof(CGPValue, values) == 0x10,
              "i386 CGPValue value-list offset changed");
static_assert(sizeof(CGPGroup) == 0x30,
              "i386 CGPGroup size changed");
static_assert(offsetof(CGPGroup, pairs) == 0x10,
              "i386 CGPGroup pair-list offset changed");
static_assert(offsetof(CGPGroup, sortedPairs) == 0x14,
              "i386 CGPGroup sorted-pair offset changed");
static_assert(offsetof(CGPGroup, lastPair) == 0x18,
              "i386 CGPGroup pair-tail offset changed");
static_assert(offsetof(CGPGroup, subGroups) == 0x1c,
              "i386 CGPGroup subgroup-list offset changed");
static_assert(offsetof(CGPGroup, sortedSubGroups) == 0x20,
              "i386 CGPGroup sorted-subgroup offset changed");
static_assert(offsetof(CGPGroup, lastSubGroup) == 0x24,
              "i386 CGPGroup subgroup-tail offset changed");
static_assert(offsetof(CGPGroup, parent) == 0x28,
              "i386 CGPGroup parent offset changed");
static_assert(offsetof(CGPGroup, writeable) == 0x2c,
              "i386 CGPGroup writeable offset changed");
static_assert(offsetof(CGenericParser2, textPool) == 0x30,
              "i386 CGenericParser2 text-pool offset changed");
static_assert(offsetof(CGenericParser2, parserWriteable) == 0x34,
              "i386 CGenericParser2 writeable offset changed");
static_assert(sizeof(CGenericParser2) == 0x38,
              "i386 CGenericParser2 size changed");
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#endif

#endif
