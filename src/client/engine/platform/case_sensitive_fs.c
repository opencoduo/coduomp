#include "case_sensitive_fs.h"

#include "filesystem/filesystem.h"

#include <errno.h>
#include <stdint.h>
#include <string.h>

#if CASE_SENSITIVE_FS
#include <dirent.h>
#include <sys/stat.h>
#endif

/*
 * NOT_FROM_ORIGINAL_SOURCE: native case-sensitive-host compatibility. The
 * original Windows client relies on host case-insensitive path lookup while
 * preserving directory-entry spelling. These helpers keep trusted host roots
 * exact and case-fold only game-controlled relative path components.
 */

enum {
    CODUOMP_CASE_COMPONENT_SIZE = 256,
    CODUOMP_CASE_CACHE_SIZE = 256
};

#if CASE_SENSITIVE_FS
typedef struct coduomp_case_cache_entry_s {
    uint32_t hash;
    qboolean occupied;
    char parent[MAX_OSPATH];
    char foldedComponent[CODUOMP_CASE_COMPONENT_SIZE];
    char actualComponent[CODUOMP_CASE_COMPONENT_SIZE];
} coduomp_case_cache_entry_t;

static coduomp_case_cache_entry_t coduomp_caseCache[CODUOMP_CASE_CACHE_SIZE];
#endif

static qboolean coduomp_copy_path(char *destination, size_t destinationSize, const char *source)
{
    const size_t length = strlen(source);
    if (length + 1u > destinationSize)
        return qfalse;
    memcpy(destination, source, length + 1u);
    return qtrue;
}

#if CASE_SENSITIVE_FS
static qboolean coduomp_is_path_separator(char character)
{
    return character == '/' || character == '\\' ? qtrue : qfalse;
}

static int32_t coduomp_ascii_lower(int32_t character)
{
    if (character >= 'A' && character <= 'Z')
        return character + ('a' - 'A');
    return character;
}

static uint32_t coduomp_case_cache_hash(const char *parent, const char *component)
{
    uint32_t hash = 2166136261u;

    for (const unsigned char *cursor = (const unsigned char *)parent; *cursor != '\0'; ++cursor) {
        hash ^= *cursor;
        hash *= 16777619u;
    }
    hash ^= (unsigned char)'/';
    hash *= 16777619u;
    for (const unsigned char *cursor = (const unsigned char *)component; *cursor != '\0'; ++cursor) {
        hash ^= (uint32_t)coduomp_ascii_lower(*cursor);
        hash *= 16777619u;
    }
    return hash;
}

static void coduomp_fold_component(const char *component, char *folded, size_t foldedSize)
{
    size_t index = 0;
    while (component[index] != '\0' && index + 1u < foldedSize) {
        folded[index] = (char)coduomp_ascii_lower((unsigned char)component[index]);
        ++index;
    }
    folded[index] = '\0';
}

static qboolean coduomp_case_equal(const char *left, const char *right)
{
    while (*left != '\0' && *right != '\0') {
        if (coduomp_ascii_lower((unsigned char)*left) != coduomp_ascii_lower((unsigned char)*right)) {
            return qfalse;
        }
        ++left;
        ++right;
    }
    return *left == *right ? qtrue : qfalse;
}

static qboolean coduomp_append_component(char *path, size_t pathSize, const char *component)
{
    const size_t pathLength = strlen(path);
    const size_t componentLength = strlen(component);
    const qboolean needsSeparator = pathLength != 0 && path[pathLength - 1] != '/' ? qtrue : qfalse;
    const size_t required = pathLength + (needsSeparator != qfalse ? 1u : 0u) + componentLength + 1u;

    if (required > pathSize)
        return qfalse;
    if (needsSeparator != qfalse)
        path[pathLength] = '/';
    memcpy(path + pathLength + (needsSeparator != qfalse ? 1u : 0u), component, componentLength + 1u);
    return qtrue;
}

static qboolean coduomp_cache_lookup(const char *parent, const char *component, char *actual, size_t actualSize)
{
    char folded[CODUOMP_CASE_COMPONENT_SIZE];
    coduomp_fold_component(component, folded, sizeof(folded));
    const uint32_t hash = coduomp_case_cache_hash(parent, component);
    coduomp_case_cache_entry_t *const entry = &coduomp_caseCache[hash % CODUOMP_CASE_CACHE_SIZE];

    if (entry->occupied == qfalse || entry->hash != hash || strcmp(entry->parent, parent) != 0 ||
        strcmp(entry->foldedComponent, folded) != 0) {
        return qfalse;
    }
    return coduomp_copy_path(actual, actualSize, entry->actualComponent);
}

static void coduomp_cache_store(const char *parent, const char *component, const char *actual)
{
    const uint32_t hash = coduomp_case_cache_hash(parent, component);
    coduomp_case_cache_entry_t *const entry = &coduomp_caseCache[hash % CODUOMP_CASE_CACHE_SIZE];

    entry->hash = hash;
    entry->occupied = qtrue;
    (void)coduomp_copy_path(entry->parent, sizeof(entry->parent), parent);
    coduomp_fold_component(component, entry->foldedComponent, sizeof(entry->foldedComponent));
    (void)coduomp_copy_path(entry->actualComponent, sizeof(entry->actualComponent), actual);
}

static qboolean coduomp_find_component(const char *parent, const char *requested, char *actual, size_t actualSize)
{
    char cached[CODUOMP_CASE_COMPONENT_SIZE];
    if (coduomp_cache_lookup(parent, requested, cached, sizeof(cached)) != qfalse) {
        char cachedPath[MAX_OSPATH];
        struct stat status;
        if (coduomp_copy_path(cachedPath, sizeof(cachedPath), parent) != qfalse &&
            coduomp_append_component(cachedPath, sizeof(cachedPath), cached) != qfalse && stat(cachedPath, &status) == 0) {
            return coduomp_copy_path(actual, actualSize, cached);
        }
    }

    DIR *const directory = opendir(parent);
    if (directory == NULL)
        return qfalse;

    qboolean found = qfalse;
    qboolean ambiguous = qfalse;
    char match[CODUOMP_CASE_COMPONENT_SIZE] = "";
    for (;;) {
        const struct dirent *const entry = readdir(directory);
        if (entry == NULL)
            break;
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0 || coduomp_case_equal(entry->d_name, requested) == qfalse) {
            continue;
        }
        if (found != qfalse && strcmp(match, entry->d_name) != 0) {
            ambiguous = qtrue;
            break;
        }
        if (coduomp_copy_path(match, sizeof(match), entry->d_name) == qfalse) {
            ambiguous = qtrue;
            break;
        }
        found = qtrue;
    }
    closedir(directory);

    if (ambiguous != qfalse) {
        Com_Printf("WARNING: ambiguous case-insensitive path component "
                   "'%s' below '%s'\n",
                   requested, parent);
        return qfalse;
    }
    if (found == qfalse)
        return qfalse;

    coduomp_cache_store(parent, requested, match);
    return coduomp_copy_path(actual, actualSize, match);
}
#endif

qboolean coduomp_resolve_case_path(const char *trustedRoot, const char *requestedPath, char *resolvedPath, size_t resolvedPathSize)
{
    if (trustedRoot == NULL || trustedRoot[0] == '\0' || requestedPath == NULL || resolvedPath == NULL || resolvedPathSize == 0) {
        return qfalse;
    }

#if !CASE_SENSITIVE_FS
    return coduomp_copy_path(resolvedPath, resolvedPathSize, requestedPath);
#else
    size_t rootLength = strlen(trustedRoot);
    while (rootLength > 1u && coduomp_is_path_separator(trustedRoot[rootLength - 1u]) != qfalse) {
        --rootLength;
    }
    const qboolean rootEndsWithSeparator = coduomp_is_path_separator(trustedRoot[rootLength - 1u]);
    if (strncmp(requestedPath, trustedRoot, rootLength) != 0 || (rootEndsWithSeparator == qfalse && requestedPath[rootLength] != '\0' &&
                                                                 coduomp_is_path_separator(requestedPath[rootLength]) == qfalse)) {
        return qfalse;
    }

    struct stat exactStatus;
    if (stat(requestedPath, &exactStatus) == 0) {
        return coduomp_copy_path(resolvedPath, resolvedPathSize, requestedPath);
    }
    if (errno != ENOENT && errno != ENOTDIR)
        return qfalse;

    if (rootLength + 1u > resolvedPathSize)
        return qfalse;
    memcpy(resolvedPath, trustedRoot, rootLength);
    resolvedPath[rootLength] = '\0';

    const char *cursor = requestedPath + rootLength;
    while (coduomp_is_path_separator(*cursor) != qfalse)
        ++cursor;

    while (*cursor != '\0') {
        char component[CODUOMP_CASE_COMPONENT_SIZE];
        size_t componentLength = 0;
        while (cursor[componentLength] != '\0' && coduomp_is_path_separator(cursor[componentLength]) == qfalse) {
            if (componentLength + 1u >= sizeof(component))
                return qfalse;
            component[componentLength] = cursor[componentLength];
            ++componentLength;
        }
        component[componentLength] = '\0';
        if (componentLength == 0 || (componentLength == 1 && component[0] == '.') ||
            (componentLength == 2 && component[0] == '.' && component[1] == '.')) {
            return qfalse;
        }

        char exactCandidate[MAX_OSPATH];
        if (coduomp_copy_path(exactCandidate, sizeof(exactCandidate), resolvedPath) == qfalse ||
            coduomp_append_component(exactCandidate, sizeof(exactCandidate), component) == qfalse) {
            return qfalse;
        }

        struct stat componentStatus;
        const char *actualComponent = component;
        char resolvedComponent[CODUOMP_CASE_COMPONENT_SIZE];
        if (stat(exactCandidate, &componentStatus) != 0) {
            if (errno != ENOENT && errno != ENOTDIR)
                return qfalse;
            if (coduomp_find_component(resolvedPath, component, resolvedComponent, sizeof(resolvedComponent)) == qfalse) {
                return qfalse;
            }
            actualComponent = resolvedComponent;
        }
        if (coduomp_append_component(resolvedPath, resolvedPathSize, actualComponent) == qfalse) {
            return qfalse;
        }

        cursor += componentLength;
        while (coduomp_is_path_separator(*cursor) != qfalse)
            ++cursor;
    }
    return qtrue;
#endif
}

FILE *coduomp_fopen_case_read(const char *trustedRoot, const char *requestedPath)
{
    FILE *file = fopen(requestedPath, "rb");
#if CASE_SENSITIVE_FS
    const int openError = errno;
    if (file == NULL && (openError == ENOENT || openError == ENOTDIR)) {
        char resolvedPath[MAX_OSPATH];
        if (coduomp_resolve_case_path(trustedRoot, requestedPath, resolvedPath, sizeof(resolvedPath)) != qfalse &&
            strcmp(resolvedPath, requestedPath) != 0) {
            file = fopen(resolvedPath, "rb");
        }
    }
#else
    (void)trustedRoot;
#endif
    return file;
}

void coduomp_case_path_cache_clear(void)
{
#if CASE_SENSITIVE_FS
    memset(coduomp_caseCache, 0, sizeof(coduomp_caseCache));
#endif
}
