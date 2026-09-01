#include "generic_parser.hpp"

#include "qcommon/q_shared_types.h"
#include "../system_fatal.h"

#include <cstring>
#include <new>

namespace {

enum : int32_t {
    GENERIC_PARSER_TEXT_POOL_SIZE = 10240
};

/* Source: CoDUOMP.exe 0x004b30e0..0x004b312d. The original emits an
 * unquoted string unless it is empty or contains a literal space. */
bool AppendQuotedText(const char *text, CTextPool **activePool)
{
    if (std::strchr(text, ' ') == nullptr && text[0] != '\0') {
        (*activePool)->AllocText(text, false, activePool);
        return true;
    }

    (*activePool)->AllocText("\"", false, activePool);
    (*activePool)->AllocText(text, false, activePool);
    (*activePool)->AllocText("\"", false, activePool);
    return true;
}

} // namespace

/* Source: CoDUOMP.exe 0x004b2c80..0x004b2ca4.
 * The original class allocator calls the scalar allocation boundary, reports
 * allocation failure, and zeroes the complete object before construction. */
void *CTextPool::operator new(std::size_t size)
{
    void *allocation = ::operator new(size, std::nothrow);
    if (allocation == nullptr) {
        Sys_OutOfMemory();
    }
    std::memset(allocation, 0, size);
    return allocation;
}

/* Source: CoDUOMP.exe 0x004b2cb0..0x004b2cb7.
 * Name/signature: same-module Mac symbol CTextPool::operator delete(void *). */
void CTextPool::operator delete(void *allocation) noexcept
{
    ::operator delete(allocation);
}

/* Source: CoDUOMP.exe 0x004b2c60..0x004b2c63. */
CTextPool *CTextPool::GetNext() const
{
    return next;
}

/* Source: CoDUOMP.exe 0x004b2c70..0x004b2c73. */
void CTextPool::SetNext(CTextPool *pool)
{
    next = pool;
}

/* Source: CoDUOMP.exe 0x004b2f00..0x004b2f37.
 * Name/signature: same-module Mac symbol CTextPool::CTextPool(int). */
CTextPool::CTextPool(int32_t poolSize)
    : buffer(nullptr), next(nullptr), capacity(poolSize), used(0)
{
    buffer = new (std::nothrow) char[static_cast<size_t>(poolSize)];
    if (buffer == nullptr) {
        Sys_OutOfMemory();
    }
    std::memset(buffer, 0, static_cast<size_t>(poolSize));
}

/* Source behavior is embedded in the MSVC deleting-destructor entry at
 * 0x004b30a0..0x004b30c0. */
CTextPool::~CTextPool()
{
    delete[] buffer;
}

/* Source: CoDUOMP.exe 0x004b2f50..0x004b306b.
 * terminateEntry reserves the copied NUL as part of this allocation. When it
 * is false, the next append begins on the current NUL, which is how the writer
 * builds a continuous document in the same pool. */
char *CTextPool::AllocText(const char *text, bool terminateEntry,
                           CTextPool **activePool)
{
    const int32_t textLength = static_cast<int32_t>(std::strlen(text));
    const int32_t allocationLength =
        textLength + (terminateEntry ? 1 : 0);

    if (used + allocationLength + 1 > capacity) {
        if (activePool == nullptr) {
            return nullptr;
        }

        CTextPool *extension = new CTextPool(capacity);
        (*activePool)->SetNext(extension);
        *activePool = extension;
        return extension->AllocText(text, terminateEntry, nullptr);
    }

    char *result = buffer + used;
    std::strcpy(result, text);
    used += allocationLength;
    buffer[used] = '\0';
    return result;
}

/* Source: CoDUOMP.exe 0x004b3070..0x004b3094.
 * Name: same-module Mac symbol CleanTextPool(CTextPool *). */
void CleanTextPool(CTextPool *pool)
{
    while (pool != nullptr) {
        CTextPool *next = pool->GetNext();
        delete pool;
        pool = next;
    }
}

/* Source: CoDUOMP.exe 0x004b2d60..0x004b2ef3.
 * Name/signature: same-module Mac symbol GetToken(char **, bool, bool).
 * The returned storage is the original single shared 1024-byte token buffer;
 * callers must consume or copy it before requesting another token. */
char *GetToken(char **data, bool allowLineBreaks, bool readUntilEol)
{
    static char token[MAX_TOKEN_CHARS];
    char *cursor = *data;
    int32_t length = 0;

    token[0] = '\0';
    if (cursor == nullptr) {
        return token;
    }

    for (;;) {
        bool crossedLine = false;
        while (static_cast<signed char>(*cursor) <=
               static_cast<signed char>(' ')) {
            if (*cursor == '\0') {
                *data = nullptr;
                return token;
            }
            if (*cursor == '\n') {
                crossedLine = true;
            }
            ++cursor;
        }

        if (crossedLine && !allowLineBreaks) {
            *data = cursor;
            return token;
        }

        if (cursor[0] == '/' && cursor[1] == '/') {
            cursor += 2;
            while (*cursor != '\0' && *cursor != '\n') {
                ++cursor;
            }
            continue;
        }

        if (cursor[0] == '/' && cursor[1] == '*') {
            cursor += 2;
            while (*cursor != '\0' &&
                   !(cursor[0] == '*' && cursor[1] == '/')) {
                ++cursor;
            }
            if (*cursor != '\0') {
                cursor += 2;
            }
            continue;
        }
        break;
    }

    if (*cursor == '"' && !readUntilEol) {
        ++cursor;
        for (;;) {
            const char character = *cursor;
            /* NOT_FROM_ORIGINAL_SOURCE: validate this recovered engine boundary input and state before use. */
            if (character == '\0') {
                break;
            }
            ++cursor;
            if (character == '"') {
                break;
            }
            if (length < MAX_TOKEN_CHARS) {
                token[length++] = character;
            }
        }
    } else if (readUntilEol) {
        while (*cursor != '\0' && *cursor != '\n' && *cursor != '\r' &&
               !(cursor[0] == '/' &&
                 (cursor[1] == '/' || cursor[1] == '*'))) {
            if (length < MAX_TOKEN_CHARS) {
                token[length++] = *cursor;
            }
            ++cursor;
        }
        while (length != 0 &&
               static_cast<signed char>(token[length - 1]) <
                   static_cast<signed char>(' ')) {
            --length;
        }
    } else {
        while (static_cast<signed char>(*cursor) >
               static_cast<signed char>(' ')) {
            if (length < MAX_TOKEN_CHARS) {
                token[length++] = *cursor;
            }
            ++cursor;
        }
    }

    if (token[0] == '"') {
        --length;
        std::memmove(token, token + 1, static_cast<size_t>(length));
        if (length != 0 && token[length - 1] == '"') {
            --length;
        }
    }
    if (length >= MAX_TOKEN_CHARS) {
        length = 0;
    }

    token[length] = '\0';
    *data = cursor;
    return token;
}

/* Source: CoDUOMP.exe 0x004b2d00..0x004b2d24.
 * CGPObject supplies the same zero-filling class allocation contract to the
 * complete CGPObject/CGPValue/CGPGroup hierarchy. */
void *CGPObject::operator new(std::size_t size)
{
    void *allocation = ::operator new(size, std::nothrow);
    if (allocation == nullptr) {
        Sys_OutOfMemory();
    }
    std::memset(allocation, 0, size);
    return allocation;
}

/* Source: CoDUOMP.exe 0x004b2d30..0x004b2d37.
 * Name/signature: same-module Mac symbol CGPObject::operator delete(void *). */
void CGPObject::operator delete(void *allocation) noexcept
{
    ::operator delete(allocation);
}

/* Source: CoDUOMP.exe 0x004b2cc0..0x004b2cc3. */
void CGPObject::SetNext(CGPObject *object)
{
    next = object;
}

/* Source: CoDUOMP.exe 0x004b2cd0..0x004b2cd3. */
CGPObject *CGPObject::GetSortedNext() const
{
    return sortedNext;
}

/* Source: CoDUOMP.exe 0x004b2ce0..0x004b2ce3. */
void CGPObject::SetSortedNext(CGPObject *object)
{
    sortedNext = object;
}

/* Source: CoDUOMP.exe 0x004b2cf0..0x004b2cf3. */
void CGPObject::SetSortedPrevious(CGPObject *object)
{
    sortedPrevious = object;
}

/* Source: CoDUOMP.exe 0x004b30d0..0x004b30dd.
 * Name/signature: same-module Mac symbol CGPObject::CGPObject(char const *). */
CGPObject::CGPObject(const char *objectName)
    : name(objectName), next(nullptr), sortedNext(nullptr),
      sortedPrevious(nullptr)
{
}

/* Source: CoDUOMP.exe 0x004b3130..0x004b3150.
 * Name/signature: same-module Mac symbol
 * CGPValue::CGPValue(char const *, char const *). */
CGPValue::CGPValue(const char *objectName, const char *initialValue)
    : CGPObject(objectName), values(nullptr)
{
    if (initialValue != nullptr) {
        AddValue(initialValue, nullptr);
    }
}

/* Source: CoDUOMP.exe 0x004b3160..0x004b3181 and the inlined body in the
 * deleting-destructor entry at 0x004b3720..0x004b3755. */
CGPValue::~CGPValue()
{
    while (values != nullptr) {
        CGPObject *nextValue = values->next;
        delete values;
        values = nextValue;
    }
}

/* Source: CoDUOMP.exe 0x004b3190..0x004b32d2. Exact original method name is
 * not present in the Mac traceback table; Clone is its proven role. */
CGPValue *CGPValue::Clone(CTextPool **activePool)
{
    const char *copiedName = name;
    if (activePool != nullptr) {
        copiedName = (*activePool)->AllocText(name, true, activePool);
    }

    CGPValue *copy = new CGPValue(copiedName, nullptr);

    for (CGPObject *value = values; value != nullptr; value = value->next) {
        const char *copiedValue = value->name;
        if (activePool != nullptr) {
            copiedValue =
                (*activePool)->AllocText(value->name, true, activePool);
        }
        copy->AddValue(copiedValue, nullptr);
    }
    return copy;
}

/* Source: CoDUOMP.exe 0x004b32e0..0x004b32f3.
 * Name: same-module Mac symbol CGPValue::IsList. */
bool CGPValue::IsList()
{
    return values != nullptr && values->next != nullptr;
}

/* Source: CoDUOMP.exe 0x004b3300..0x004b330c.
 * Name: same-module Mac symbol CGPValue::GetTopValue. */
const char *CGPValue::GetTopValue()
{
    return values != nullptr ? values->name : nullptr;
}

/* Source: CoDUOMP.exe 0x004b3310..0x004b33ab.
 * Name/signature: same-module Mac symbol CGPValue::AddValue. The first value
 * node's sortedNext field is the original list-tail cache. */
void CGPValue::AddValue(const char *value, CTextPool **activePool)
{
    const char *storedValue = value;
    if (activePool != nullptr) {
        storedValue =
            (*activePool)->AllocText(value, true, activePool);
    }

    CGPObject *entry = new CGPObject(storedValue);

    if (values == nullptr) {
        values = entry;
        values->SetSortedNext(entry);
        return;
    }

    values->GetSortedNext()->SetNext(entry);
    values->SetSortedNext(entry);
}

/* Source: CoDUOMP.exe 0x004b33b0..0x004b341d.
 * Name/signature: same-module Mac symbol CGPValue::Parse. */
bool CGPValue::Parse(char **data, CTextPool **activePool)
{
    for (;;) {
        char *token = GetToken(data, true, true);
        if (token[0] == '\0') {
            return false;
        }
        if (std::strcmp(token, "]") == 0) {
            return true;
        }

        const char *storedValue =
            (*activePool)->AllocText(token, true, activePool);
        AddValue(storedValue, nullptr);
    }
}

/* Source: CoDUOMP.exe 0x004b3420..0x004b3618. The Mac traceback table omits
 * this leaf method; its output grammar and ownership are fully proven by the
 * Windows body. */
bool CGPValue::Write(CTextPool **activePool, int32_t indentLevel)
{
    if (values == nullptr) {
        return true;
    }

    for (int32_t indent = 0; indent < indentLevel; ++indent) {
        (*activePool)->AllocText("\t", false, activePool);
    }
    AppendQuotedText(name, activePool);
    (*activePool)->AllocText("\r\n", false, activePool);

    if (values->next == nullptr) {
        (*activePool)->AllocText("\t\t", false, activePool);
        AppendQuotedText(values->name, activePool);
        (*activePool)->AllocText("\r\n", false, activePool);
        return true;
    }

    for (int32_t indent = 0; indent < indentLevel; ++indent) {
        (*activePool)->AllocText("\t", false, activePool);
    }
    (*activePool)->AllocText("[\r\n", false, activePool);

    for (CGPObject *value = values; value != nullptr; value = value->next) {
        for (int32_t indent = 0; indent < indentLevel + 1; ++indent) {
            (*activePool)->AllocText("\t", false, activePool);
        }
        AppendQuotedText(value->name, activePool);
        (*activePool)->AllocText("\r\n", false, activePool);
    }

    for (int32_t indent = 0; indent < indentLevel; ++indent) {
        (*activePool)->AllocText("\t", false, activePool);
    }
    (*activePool)->AllocText("]\r\n", false, activePool);
    return true;
}

/* Source: CoDUOMP.exe 0x004b3620..0x004b3645.
 * Name/signature: same-module Mac symbol
 * CGPGroup::CGPGroup(char const *, CGPGroup *). */
CGPGroup::CGPGroup(const char *objectName, CGPGroup *parentGroup)
    : CGPObject(objectName), pairs(nullptr), sortedPairs(nullptr),
      lastPair(nullptr), subGroups(nullptr), sortedSubGroups(nullptr),
      lastSubGroup(nullptr), parent(parentGroup), writeable(false)
{
}

/* Source: CoDUOMP.exe 0x004b3650..0x004b3654.
 * Name: same-module Mac symbol CGPGroup::~CGPGroup. */
CGPGroup::~CGPGroup()
{
    Clean();
}

/* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
int32_t CGPGroup::GetNumSubGroups()
{
    CGPGroup *group = static_cast<CGPGroup *>(subGroups);
    int32_t count = 0;
    while (group != nullptr) {
        ++count;
        group = static_cast<CGPGroup *>(group->next);
    }
    return count;
}

/* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
int32_t CGPGroup::GetNumPairs()
{
    CGPValue *pair = static_cast<CGPValue *>(pairs);
    int32_t count = 0;
    while (pair != nullptr) {
        ++count;
        pair = static_cast<CGPValue *>(pair->next);
    }
    return count;
}

/* Source: CoDUOMP.exe 0x004b3680..0x004b3717.
 * Name: same-module Mac symbol CGPGroup::Clean. */
void CGPGroup::Clean()
{
    while (pairs != nullptr) {
        CGPValue *pair = static_cast<CGPValue *>(pairs);
        CGPObject *nextPair = pair->next;
        delete pair;
        pairs = nextPair;
    }

    while (subGroups != nullptr) {
        CGPGroup *group = static_cast<CGPGroup *>(subGroups);
        CGPObject *nextGroup = group->next;
        delete group;
        subGroups = nextGroup;
    }

    pairs = nullptr;
    sortedPairs = nullptr;
    lastPair = nullptr;
    subGroups = nullptr;
    sortedSubGroups = nullptr;
    lastSubGroup = nullptr;
    parent = nullptr;
    writeable = false;
}

/* Source: CoDUOMP.exe 0x004b2d40..0x004b2d43. */
void CGPGroup::SetWriteable(bool value)
{
    writeable = value;
}

/* Source: CoDUOMP.exe 0x004b3780..0x004b3859. Exact original method name is
 * not present in the Mac traceback table; Clone is its proven role. */
CGPGroup *CGPGroup::Clone(CTextPool **activePool)
{
    const char *copiedName = name;
    if (activePool != nullptr) {
        copiedName = (*activePool)->AllocText(name, true, activePool);
    }

    CGPGroup *copy = new CGPGroup(copiedName, nullptr);

    for (CGPGroup *group = static_cast<CGPGroup *>(subGroups);
         group != nullptr;
         group = static_cast<CGPGroup *>(group->next)) {
        copy->AddGroup(group->Clone(activePool));
    }
    for (CGPValue *pair = static_cast<CGPValue *>(pairs);
         pair != nullptr;
         pair = static_cast<CGPValue *>(pair->next)) {
        copy->AddPair(pair->Clone(activePool));
    }
    return copy;
}

/* Source: CoDUOMP.exe 0x004b3860..0x004b38d4.
 * Name/signature: same-module Mac symbol CGPGroup::SortObject. */
void CGPGroup::SortObject(CGPObject *object, CGPObject **first,
                          CGPObject **sortedFirst, CGPObject **last)
{
    if (*first == nullptr) {
        *first = object;
        *sortedFirst = object;
        *last = object;
        return;
    }

    (*last)->SetNext(object);

    CGPObject *previous = nullptr;
    CGPObject *current = *sortedFirst;
    while (current != nullptr) {
        if (std::strcmp(object->name, current->name) < 0) {
            current->SetSortedPrevious(object);
            object->SetSortedNext(current);
            break;
        }
        previous = current;
        current = current->GetSortedNext();
    }

    if (previous != nullptr) {
        previous->SetSortedNext(object);
        object->SetSortedPrevious(previous);
    } else {
        *sortedFirst = object;
    }
    *last = object;
}

/* Source: CoDUOMP.exe 0x004b38e0..0x004b3994.
 * Name/signature: same-module Mac symbol CGPGroup::AddPair. */
CGPValue *CGPGroup::AddPair(const char *key, const char *value,
                            CTextPool **activePool)
{
    const char *storedKey = key;
    const char *storedValue = value;
    if (activePool != nullptr) {
        storedKey = (*activePool)->AllocText(key, true, activePool);
        if (value != nullptr) {
            storedValue =
                (*activePool)->AllocText(value, true, activePool);
        }
    }

    CGPValue *pair = new CGPValue(storedKey, storedValue);
    return AddPair(pair);
}

/* Source: CoDUOMP.exe 0x004b39a0..0x004b39b0.
 * Name/signature: same-module Mac symbol CGPGroup::AddPair(CGPValue *). */
CGPValue *CGPGroup::AddPair(CGPValue *pair)
{
    SortObject(pair, &pairs, &sortedPairs, &lastPair);
    return pair;
}

/* Source: CoDUOMP.exe 0x004b39c0..0x004b3a35.
 * Name/signature: same-module Mac symbol CGPGroup::AddGroup. */
CGPGroup *CGPGroup::AddGroup(const char *groupName,
                             CTextPool **activePool)
{
    const char *storedName = groupName;
    if (activePool != nullptr) {
        storedName =
            (*activePool)->AllocText(groupName, true, activePool);
    }

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    CGPGroup *group = new CGPGroup(storedName, nullptr);
    return AddGroup(group);
}

/* Source: CoDUOMP.exe 0x004b3a40..0x004b3a50.
 * Name/signature: same-module Mac symbol CGPGroup::AddGroup(CGPGroup *). */
CGPGroup *CGPGroup::AddGroup(CGPGroup *group)
{
    SortObject(group, &subGroups, &sortedSubGroups, &lastSubGroup);
    return group;
}

/* Source: CoDUOMP.exe 0x004b3a60..0x004b3a86. Exact Mac leaf symbol is absent;
 * the insertion-order subgroup lookup and strcmp operands prove this role. */
CGPGroup *CGPGroup::FindSubGroup(const char *groupName)
{
    for (CGPGroup *group = static_cast<CGPGroup *>(subGroups);
         group != nullptr;
         group = static_cast<CGPGroup *>(group->next)) {
        if (std::strcmp(groupName, group->name) == 0) {
            return group;
        }
    }
    return nullptr;
}

/* Source: CoDUOMP.exe 0x004b3a90..0x004b3bb0.
 * Name/signature: same-module Mac symbol CGPGroup::Parse. */
bool CGPGroup::Parse(char **data, CTextPool **activePool)
{
    char key[MAX_TOKEN_CHARS];
    char *token = GetToken(data, true, false);

    while (token[0] != '\0') {
        if (std::strcmp(token, "}") == 0) {
            return true;
        }

        std::strcpy(key, token);
        /* 0x004b3afa..0x004b3b04 sets both GetToken boolean arguments to
         * one (DL=allowLineBreaks, BL=readUntilEol). A pair's value is the
         * complete remainder of its source line, not only its first word. */
        token = GetToken(data, true, true);

        if (std::strcmp(token, "{") == 0) {
            CGPGroup *group = AddGroup(key, activePool);
            group->SetWriteable(writeable);
            if (!group->Parse(data, activePool)) {
                return false;
            }
        } else if (std::strcmp(token, "[") == 0) {
            CGPValue *pair = AddPair(key, nullptr, activePool);
            if (!pair->Parse(data, activePool)) {
                return false;
            }
        } else {
            AddPair(key, token, activePool);
        }

        token = GetToken(data, true, false);
    }

    /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
    return parent == nullptr;
}

/* Source: CoDUOMP.exe 0x004b3bc0..0x004b3ce9. The Mac traceback table omits
 * this leaf method; the Windows body proves the complete text grammar. */
bool CGPGroup::Write(CTextPool **activePool, int32_t indentLevel)
{
    if (indentLevel >= 0) {
        for (int32_t indent = 0; indent < indentLevel; ++indent) {
            (*activePool)->AllocText("\t", false, activePool);
        }
        AppendQuotedText(name, activePool);
        (*activePool)->AllocText("\r\n", false, activePool);
        for (int32_t indent = 0; indent < indentLevel; ++indent) {
            (*activePool)->AllocText("\t", false, activePool);
        }
        (*activePool)->AllocText("{\r\n", false, activePool);
    }

    for (CGPValue *pair = static_cast<CGPValue *>(pairs);
         pair != nullptr;
         pair = static_cast<CGPValue *>(pair->next)) {
        pair->Write(activePool, indentLevel + 1);
    }
    for (CGPGroup *group = static_cast<CGPGroup *>(subGroups);
         group != nullptr;
         group = static_cast<CGPGroup *>(group->next)) {
        group->Write(activePool, indentLevel + 1);
    }

    if (indentLevel >= 0) {
        for (int32_t indent = 0; indent < indentLevel; ++indent) {
            (*activePool)->AllocText("\t", false, activePool);
        }
        (*activePool)->AllocText("}\r\n", false, activePool);
    }
    return true;
}

/* Source: CoDUOMP.exe 0x004b3cf0..0x004b3d16. Exact Mac leaf symbol is absent;
 * the insertion-order pair lookup and strcmp operands prove this role. */
CGPValue *CGPGroup::FindPair(const char *key)
{
    for (CGPValue *pair = static_cast<CGPValue *>(pairs);
         pair != nullptr;
         pair = static_cast<CGPValue *>(pair->next)) {
        if (std::strcmp(key, pair->name) == 0) {
            return pair;
        }
    }
    return nullptr;
}

/* Source: CoDUOMP.exe 0x004b3d20..0x004b3d59. Exact Mac leaf symbol is absent;
 * it returns the caller default only when the pair itself is absent. */
const char *CGPGroup::FindPairValue(const char *key,
                                    const char *defaultValue)
{
    CGPValue *pair = FindPair(key);
    if (pair == nullptr) {
        return defaultValue;
    }
    return pair->GetTopValue();
}

/* Source: CoDUOMP.exe 0x004b3d60..0x004b3d8f.
 * Name/signature: same-module Mac symbol CGenericParser2::CGenericParser2. */
CGenericParser2::CGenericParser2()
    : CGPGroup("Top Level", nullptr), textPool(nullptr),
      parserWriteable(false)
{
}

/* Source: CoDUOMP.exe 0x004b3d90..0x004b3dd7.
 * Name: same-module Mac symbol CGenericParser2::~CGenericParser2. The base
 * destructor performs the second, already-empty CGPGroup::Clean seen in the
 * Windows epilogue. */
CGenericParser2::~CGenericParser2()
{
    Clean();
}

/* Source: CoDUOMP.exe 0x004b2d50..0x004b2d53. */
void CGenericParser2::SetWriteable(bool value)
{
    parserWriteable = value;
}

/* Source: CoDUOMP.exe 0x004b3de0..0x004b3eb2.
 * Name/signature: same-module Mac symbol CGenericParser2::Parse. */
bool CGenericParser2::Parse(char **data, bool cleanFirst,
                            bool makeWriteable)
{
    if (cleanFirst) {
        Clean();
    }

    if (textPool == nullptr) {
        textPool = new CTextPool(GENERIC_PARSER_TEXT_POOL_SIZE);
    }

    CGenericParser2::SetWriteable(makeWriteable);
    CGPGroup::SetWriteable(makeWriteable);
    /* 0x004b3e7c..0x004b3e99 copies the owned head into a stack-local
     * allocation cursor before parsing. CTextPool::AllocText advances only
     * this cursor when it appends an extension, leaving textPool at the head
     * so CleanTextPool can release the complete chain. */
    CTextPool *activePool = textPool;
    return CGPGroup::Parse(data, &activePool);
}

/* Source: CoDUOMP.exe 0x004b3ec0..0x004b3ef3.
 * Name: same-module Mac symbol CGenericParser2::Clean. */
void CGenericParser2::Clean()
{
    CGPGroup::Clean();
    CleanTextPool(textPool);
    textPool = nullptr;
}

/* Source: CoDUOMP.exe 0x004b3f00..0x004b3f4b. Exact Mac leaf symbol is absent;
 * it serializes the top-level ordered pairs and groups without a root wrapper. */
bool CGenericParser2::Write(CTextPool *outputPool)
{
    CTextPool *activePool = outputPool;
    for (CGPValue *pair = static_cast<CGPValue *>(pairs);
         pair != nullptr;
         pair = static_cast<CGPValue *>(pair->next)) {
        pair->Write(&activePool, 0);
    }
    for (CGPGroup *group = static_cast<CGPGroup *>(subGroups);
         group != nullptr;
         group = static_cast<CGPGroup *>(group->next)) {
        group->Write(&activePool, 0);
    }
    return true;
}
