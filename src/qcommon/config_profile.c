#if !defined(_WIN32) && !defined(_XOPEN_SOURCE)
#define _XOPEN_SOURCE 700
#endif

#include "config_profile.h"
#include "config_script.h"
#include "com_config.h"
#include "com_command_handlers.h"
#include "q_command.h"
#include "q_cvar.h"
#include "q_string.h"
#include "filesystem/filesystem.h"
#include "filesystem/filesystem_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void Com_Printf(const char *format, ...);
int32_t Sys_Milliseconds(void);
extern qboolean com_errorEntered;
extern cvar_t *com_journal;

enum {
    CODUOMP_CONFIG_QUIET_MS = 1000,
    CODUOMP_CONFIG_MAX_AGE_MS = 5000,
    CODUOMP_CONFIG_WAIT_MS = 5000,
    CODUOMP_CONFIG_RETRY_MS = 10000
};

#if defined(WINDOWS_BEHAVIOR)
static const char coduomp_config_name[] = "uoconfig_mp.cfg";
#else
static const char coduomp_config_name[] = "uoconfig_mp_server.cfg";
#endif

typedef struct coduomp_config_input_s {
    struct coduomp_config_input_s *next;
    char *data;
    size_t size;
    char name[MAX_QPATH], source[CODUOMP_CONFIG_PATH];
    int result;
} coduomp_config_input_t;

struct coduomp_config_profile_s {
    struct coduomp_config_profile_s *next;
    char path[CODUOMP_CONFIG_PATH];
    coduomp_config_revision_t observed;
    qboolean protected, loading, announced, loadedSnapshot, discarded;
    unsigned loads;
    char problem[1024];
    coduomp_config_input_t *inputs;
    coduomp_config_job_t *job, *loadedJob;
    char *pending;
    size_t pendingSize;
    uint64_t pendingRevision, jobRevision;
    uint32_t firstChange, lastChange, retryAt;
    unsigned failures;
};

static coduomp_config_profile_t *coduomp_config_profiles, *coduomp_config_active;
static uint64_t coduomp_config_revision;
static coduomp_config_dialog_t coduomp_config_dialog;
static qboolean coduomp_config_registered;

/* NOT_FROM_ORIGINAL_SOURCE: append only commands whose parsed values match
 * the captured settings exactly. Config strings have no invented escapes. */
qboolean coduomp_config_append(coduomp_config_buffer_t *buffer, const char *command, const char *name, const char *value)
{
    if (buffer->error[0])
        return qfalse;
    if (!name)
        name = "";
    if (!value)
        value = "";
    size_t length = strlen(command) + (name[0] ? strlen(name) + strlen(value) + 7 : 1);
    if (length > CBUF_COMMAND_CAPACITY || buffer->size + length > CBUF_TEXT_CAPACITY - 1) {
        snprintf(buffer->error, sizeof(buffer->error), "setting '%s' exceeds config execution capacity", name);
        return qfalse;
    }
    char line[CBUF_COMMAND_CAPACITY + 1];
    int written = name[0] ? snprintf(line, sizeof(line), "%s \"%s\" \"%s\"\n", command, name, value) :
        snprintf(line, sizeof(line), "%s\n", command);
    coduomp_config_tokens_t tokens;
    if (written < 0 || written >= (int)sizeof(line) ||
        !coduomp_config_validate(line, (size_t)written, NULL) ||
        !coduomp_command_tokens(line, (size_t)written, 0, &tokens, NULL) ||
        tokens.argc != (name[0] ? 3 : 1) || strcmp(tokens.argv[0], command) ||
        (name[0] && (strcmp(tokens.argv[1], name) || strcmp(tokens.argv[2], value)))) {
        snprintf(buffer->error, sizeof(buffer->error), "setting '%s' cannot be represented by config syntax", name);
        return qfalse;
    }
    size_t required = buffer->size + (size_t)written + 1;
    if (required > buffer->capacity) {
        size_t capacity = required + 4096;
        char *data = realloc(buffer->data, capacity);
        if (!data) {
            snprintf(buffer->error, sizeof(buffer->error), "not enough memory to serialize settings");
            return qfalse;
        }
        buffer->data = data;
        buffer->capacity = capacity;
    }
    memcpy(buffer->data + buffer->size, line, (size_t)written + 1);
    buffer->size += (size_t)written;
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: immutable serialization preserves the original
 * ordering, latched-value selection, and cvar exclusions without truncation. */
static char *coduomp_config_capture(qboolean defaults, size_t *size, char error[256])
{
    coduomp_config_buffer_t buffer = {0};
    if (!defaults)
        coduomp_config_capture_bindings(&buffer);
    for (const cvar_t *var = cvar_vars; var; var = var->next) {
        if (!var->name || !Q_stricmp(var->name, "cl_cdkey"))
            continue;
        if (defaults ? (var->flags & (CVAR_ROM | CVAR_USER_CREATED | CVAR_CHEAT | CVAR_SCRIPT_SETCVAR)) != 0 :
                       (var->flags & CVAR_ARCHIVE) == 0)
            continue;
        const char *value = defaults ? var->resetString : (var->latchedString ? var->latchedString : var->string);
        if (!coduomp_config_append(&buffer, defaults ? "set" : "seta", var->name, value))
            break;
    }
    if (buffer.error[0]) {
        strcpy(error, buffer.error);
        free(buffer.data);
        return NULL;
    }
    if (!buffer.data)
        buffer.data = calloc(1, 1);
    *size = buffer.size;
    return buffer.data;
}

/* NOT_FROM_ORIGINAL_SOURCE: key/cvar mutation revisions let an I/O completion
 * acknowledge only the settings it actually captured. */
void coduomp_config_settings_changed(void)
{
    ++coduomp_config_revision;
    cvar_modifiedFlags |= CVAR_ARCHIVE;
    if (coduomp_config_active) {
        uint32_t now = (uint32_t)Sys_Milliseconds();
        if (!coduomp_config_active->firstChange)
            coduomp_config_active->firstChange = now ? now : 1;
        coduomp_config_active->lastChange = now;
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: failures protect the destination and its recovery
 * history immediately, even if the renderer has not been initialized. */
void coduomp_config_fail(coduomp_config_profile_t *profile, const char *source, unsigned line, const char *reason)
{
    if (!profile)
        profile = coduomp_config_active;
    if (!profile)
        return;
    if (!profile->protected) {
        snprintf(profile->problem, sizeof(profile->problem), "%s:%u: %s. The file has been preserved. Automatic saving is paused for this profile.", source, line, reason);
        Com_Printf("Settings: %s\n", profile->problem);
        profile->announced = qfalse;
    }
    profile->protected = qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: expose one active profile, without changing its
 * identity merely because the filesystem is temporarily shut down. */
coduomp_config_profile_t *coduomp_config_current_profile(void)
{
    return coduomp_config_active;
}

/* NOT_FROM_ORIGINAL_SOURCE: explicit fallback settings are session-only. */
void coduomp_config_pause(const char *reason)
{
    coduomp_config_fail(coduomp_config_active, coduomp_config_active ? coduomp_config_active->path : "settings", 1, reason);
}

/* NOT_FROM_ORIGINAL_SOURCE: errors during file resolution or initialization
 * also count as incomplete loads, even before a command origin is queued. */
void coduomp_config_abort_load(void)
{
    if (coduomp_config_active && (coduomp_config_active->loading || coduomp_config_active->loads))
        coduomp_config_pause("settings initialization did not complete");
}

/* NOT_FROM_ORIGINAL_SOURCE: snapshots cached for preference inspection are
 * consumed by exec, preventing a second read of different or rejected bytes. */
static void coduomp_config_clear_inputs(coduomp_config_profile_t *profile)
{
    while (profile->inputs) {
        coduomp_config_input_t *input = profile->inputs;
        profile->inputs = input->next;
        free(input->data);
        free(input);
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: profiles persist across filesystem restarts;
 * pending jobs continue owning their original paths and captured settings. */
void coduomp_config_begin_profile(void)
{
    char path[CODUOMP_CONFIG_PATH], error[256] = "cannot resolve config destination";
    if (!coduomp_fs_config_destination(NULL, NULL, coduomp_config_name, path, error)) {
        if (!coduomp_config_active) {
            coduomp_config_active = calloc(1, sizeof(*coduomp_config_active));
            if (coduomp_config_active) {
                snprintf(coduomp_config_active->path, sizeof(coduomp_config_active->path), "%s/%s/%s",
                    fs_homepath ? fs_homepath->string : "(unavailable)", fs_currentGameDir, coduomp_config_name);
                coduomp_config_active->next = coduomp_config_profiles;
                coduomp_config_profiles = coduomp_config_active;
            }
        }
        Com_Printf("Cannot resolve settings destination: %s\n", error);
        coduomp_config_pause(error);
        return;
    }
    coduomp_config_profile_t *profile;
    for (profile = coduomp_config_profiles; profile; profile = profile->next) {
        if (!profile->discarded && !Q_stricmp(profile->path, path))
            break;
    }
    if (!profile) {
        profile = calloc(1, sizeof(*profile));
        if (!profile) {
            com_configAutowriteEnabled = qfalse;
            Com_Printf("Could not allocate config protection state; saving disabled.\n");
            return;
        }
        strcpy(profile->path, path);
        profile->next = coduomp_config_profiles;
        coduomp_config_profiles = profile;
        char *bytes;
        size_t size;
        int result = coduomp_config_read_disk(path, &bytes, &size, &profile->observed, error);
        free(bytes);
        if (result < 0)
            coduomp_config_fail(profile, path, 1, error);
        else if (result == 0 && coduomp_config_recovery_exists(path))
            coduomp_config_fail(profile, path, 1, "config is missing but recovery files exist");
    }
    if (coduomp_config_active && coduomp_config_active != profile && coduomp_config_active->loads) {
        coduomp_config_pause("profile changed before its settings finished executing");
        coduomp_config_fail(profile, path, 1, "profile changed during config execution; reload settings before saving");
    }
    qboolean changed = coduomp_config_active != profile;
    coduomp_config_active = profile;
    if (changed)
        coduomp_config_settings_changed();
    profile->loading = qtrue;
    profile->loadedSnapshot = qfalse;
}

/* NOT_FROM_ORIGINAL_SOURCE: detach before a namespace restores another
 * profile's cvars; intervening filesystem teardown cannot save them to the
 * departed profile. Its jobs and pending snapshots remain independently owned. */
void coduomp_config_leave_profile(void)
{
    if (coduomp_config_active && coduomp_config_active->loads)
        coduomp_config_pause("profile departed before its settings finished executing");
    coduomp_config_active = NULL;
}

/* NOT_FROM_ORIGINAL_SOURCE: queued bytes, inserted commands, and nested exec
 * retain a load reference until their final command has actually executed. */
void coduomp_config_load_reference(coduomp_config_profile_t *profile, int delta)
{
    if (!profile)
        return;
    if (delta > 0) {
        profile->loads += (unsigned)delta;
        profile->loading = qtrue;
        profile->loadedSnapshot = qfalse;
    } else {
        profile->loads -= (unsigned)-delta;
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: optional startup files may be absent; a nested
 * include is required. Every present file is validated before publication. */
static coduomp_config_input_t *coduomp_config_input(coduomp_config_profile_t *profile, const char *file, qboolean nested)
{
    coduomp_config_input_t *input = calloc(1, sizeof(*input));
    if (!input) {
        coduomp_config_fail(profile, file, 1, "not enough memory to read settings");
        return NULL;
    }
    Q_strncpyz(input->name, file, sizeof(input->name));
    char error[256] = "could not read config";
    if (com_journal && com_journal->integer) {
        input->result = coduomp_fs_config_read(file, &input->data, &input->size, input->source, error);
        if (com_journal->integer == 2)
            coduomp_config_fail(profile, input->source, 1, "journal playback uses temporary settings");
    } else if (profile && !Q_stricmp(file, coduomp_config_name)) {
        coduomp_config_revision_t observed;
        strcpy(input->source, profile->path);
        input->result = coduomp_config_read_disk(profile->path, &input->data, &input->size, &observed, error);
        if (input->result == 0 && profile->observed.exists) {
            input->result = -1;
            strcpy(error, "previously observed config is missing");
        } else if (input->result >= 0 && !profile->job && !profile->pending)
            profile->observed = observed;
        if (input->result == 0 && !profile->protected)
            input->result = coduomp_fs_config_read(file, &input->data, &input->size, input->source, error);
    } else
        input->result = coduomp_fs_config_read(file, &input->data, &input->size, input->source, error);
    if (input->result == 0) {
        qboolean optional = !nested && (!Q_stricmp(file, coduomp_config_name) || !Q_stricmp(file, "autoexec_mp.cfg") ||
            !Q_stricmp(file, "autoexec.cfg") || !Q_stricmp(file, "language.cfg"));
        if (!optional) {
            input->result = -1;
            strcpy(error, "required config file is missing");
        }
    }
    if (input->result < 0)
        coduomp_config_fail(profile, input->source, 1, error);
    if (input->result == 1) {
        /* UTF-8's signature is metadata, not a command token. */
        if (input->size >= 3 && !memcmp(input->data, "\xef\xbb\xbf", 3)) {
            memmove(input->data, input->data + 3, input->size - 3 + 1);
            input->size -= 3;
        }
        coduomp_config_error_t problem;
        if (!coduomp_config_validate(input->data, input->size, &problem)) {
            coduomp_config_fail(profile, input->source, problem.line, problem.reason);
            input->result = -1;
        }
    }
    return input;
}

/* NOT_FROM_ORIGINAL_SOURCE: startup preference inspection uses the same
 * accepted owned snapshot that the later exec command consumes. */
qboolean coduomp_config_preference(const char *file, const char *name)
{
    coduomp_config_profile_t *profile = coduomp_config_active;
    if (!profile)
        return qfalse;
    coduomp_config_input_t *input;
    for (input = profile->inputs; input; input = input->next) {
        if (!Q_stricmp(input->name, file))
            break;
    }
    if (!input) {
        input = coduomp_config_input(profile, file, qfalse);
        if (!input)
            return qfalse;
        input->next = profile->inputs;
        profile->inputs = input;
    }
    return input->result == 1 && coduomp_config_has_assignment(input->data, input->size, name);
}

/* NOT_FROM_ORIGINAL_SOURCE: return ownership of the exact preflighted bytes. */
int coduomp_config_read(coduomp_config_profile_t *profile, const char *file, qboolean nested, char **data, size_t *size, char source[CODUOMP_CONFIG_PATH])
{
    *data = NULL;
    *size = 0;
    if (!profile)
        profile = coduomp_config_active;
    if (profile)
        profile->loading = qtrue;
    coduomp_config_input_t *input = NULL;
    if (profile) {
        for (coduomp_config_input_t **link = &profile->inputs; *link; link = &(*link)->next) {
            if (!Q_stricmp((*link)->name, file)) {
                input = *link;
                *link = input->next;
                break;
            }
        }
    }
    if (!input)
        input = coduomp_config_input(profile, file, nested);
    if (!input)
        return -1;
    int result = input->result;
    strcpy(source, input->source);
    if (result == 1) {
        *data = input->data;
        *size = input->size;
    } else
        free(input->data);
    free(input);
    return result;
}

/* NOT_FROM_ORIGINAL_SOURCE: acknowledge a captured revision only after a
 * confirmed commit, and never clear a different profile's dirty settings. */
static qboolean coduomp_config_poll(coduomp_config_profile_t *profile, unsigned wait)
{
    coduomp_config_result_t result;
    if (profile->loadedJob && coduomp_config_store_finish(profile->loadedJob, wait, &result)) {
        profile->loadedJob = NULL;
        if (result.status != CODUOMP_CONFIG_COMMITTED)
            Com_Printf("Could not update last-loaded settings snapshot for %s: %s\n", profile->path, result.detail);
    }
    if (!profile->job)
        return qtrue;
    if (!coduomp_config_store_finish(profile->job, wait, &result))
        return qfalse;
    profile->job = NULL;
    if (result.status == CODUOMP_CONFIG_COMMITTED) {
        profile->observed = result.revision;
        if (profile->pendingRevision == profile->jobRevision) {
            free(profile->pending);
            profile->pending = NULL;
            if (profile != coduomp_config_active || coduomp_config_revision == profile->jobRevision)
                profile->firstChange = 0;
        }
        if (profile == coduomp_config_active && coduomp_config_revision == profile->jobRevision)
            cvar_modifiedFlags &= ~(uint32_t)CVAR_ARCHIVE;
        if (profile->failures)
            Com_Printf("Settings saved successfully to %s.\n", profile->path);
        profile->failures = 0;
        profile->retryAt = 0;
        coduomp_fs_config_changed();
        if (result.detail[0])
            Com_Printf("Settings: %s\n", result.detail);
    } else {
        if (profile->failures++ == 0) {
            Com_Printf("Could not save %s: %s. Settings remain pending.\n", profile->path, result.detail);
            if (result.rollback[0])
                Com_Printf("Previous bytes retained at %s\n", result.rollback);
        }
        profile->retryAt = (uint32_t)Sys_Milliseconds() + CODUOMP_CONFIG_RETRY_MS * (profile->failures < 6 ? profile->failures : 6);
        if (result.status == CODUOMP_CONFIG_RECOVERY_REQUIRED || result.status == CODUOMP_CONFIG_DURABILITY_UNCONFIRMED)
            coduomp_config_fail(profile, profile->path, 1, result.detail);
    }
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: create directories only after resolving and
 * validating the complete destination, before handing paths to the worker. */
static qboolean coduomp_config_prepare_directory(const char *path)
{
    if (strlen(path) >= MAX_OSPATH)
        return qfalse;
    char writable[MAX_OSPATH];
    strcpy(writable, path);
    return FS_CreatePath(writable) == qfalse;
}

/* NOT_FROM_ORIGINAL_SOURCE: keep the owned pending snapshot even when worker
 * dispatch or a previous save fails, including after this profile departs. */
static void coduomp_config_dispatch(coduomp_config_profile_t *profile)
{
    if (profile->job || !profile->pending || profile->protected || profile->discarded || profile->loads || profile->loading)
        return;
    if (!coduomp_config_prepare_directory(profile->path)) {
        if (!profile->failures++)
            Com_Printf("Cannot create the config directory for %s.\n", profile->path);
        profile->retryAt = (uint32_t)Sys_Milliseconds() + CODUOMP_CONFIG_RETRY_MS;
        return;
    }
    profile->job = coduomp_config_store_start(profile->path, profile->pending, profile->pendingSize, &profile->observed, qfalse, qtrue);
    if (profile->job)
        profile->jobRevision = profile->pendingRevision;
    else {
        if (!profile->failures++)
            Com_Printf("Could not start config save for %s; settings remain pending.\n", profile->path);
        profile->retryAt = (uint32_t)Sys_Milliseconds() + CODUOMP_CONFIG_RETRY_MS;
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: capture last-loaded settings only after all
 * queued input and initialization finish, before any ordinary automatic save. */
static void coduomp_config_loaded(coduomp_config_profile_t *profile)
{
    profile->loading = qfalse;
    coduomp_config_clear_inputs(profile);
    if (profile->protected || profile->loadedSnapshot || profile->loadedJob)
        return;
    profile->loadedSnapshot = qtrue;
    if (!profile->observed.exists)
        coduomp_config_settings_changed();
    char error[256] = "could not capture settings";
    size_t size, envelopeSize;
    char *data = coduomp_config_capture(qfalse, &size, error);
    if (!data) {
        coduomp_config_fail(profile, profile->path, 1, error);
        return;
    }
    char *envelope = coduomp_config_envelope(data, size, &envelopeSize);
    free(data);
    if (!envelope)
        return;
    char path[CODUOMP_CONFIG_PATH];
    if (snprintf(path, sizeof(path), "%s.loaded.cfg", profile->path) >= (int)sizeof(path)) {
        free(envelope);
        return;
    }
    coduomp_config_revision_t observed;
    char *old;
    size_t oldSize;
    if (coduomp_config_prepare_directory(profile->path) && coduomp_config_read_disk(path, &old, &oldSize, &observed, error) >= 0) {
        free(old);
        profile->loadedJob = coduomp_config_store_start(path, envelope, envelopeSize, &observed, qfalse, qfalse);
    }
    free(envelope);
}

/* NOT_FROM_ORIGINAL_SOURCE: snapshot current settings only for their owning
 * profile. An incomplete load or a recovery session cannot overwrite it. */
static void coduomp_config_capture_pending(coduomp_config_profile_t *profile)
{
    if (!profile || profile->protected || profile->loading || profile->discarded || profile->loads ||
        !com_configAutowriteEnabled || com_errorEntered || !(cvar_modifiedFlags & CVAR_ARCHIVE))
        return;
    if (profile->pending && profile->pendingRevision == coduomp_config_revision)
        return;
    char error[256] = "could not serialize settings";
    size_t size;
    char *snapshot = coduomp_config_capture(qfalse, &size, error);
    if (!snapshot) {
        coduomp_config_fail(profile, profile->path, 1, error);
        return;
    }
    free(profile->pending);
    profile->pending = snapshot;
    profile->pendingSize = size;
    profile->pendingRevision = coduomp_config_revision;
}

/* NOT_FROM_ORIGINAL_SOURCE: coalesce frame-time changes and collect worker
 * results on the main thread; only owned snapshots are retried after departure. */
void coduomp_config_service(void)
{
    uint32_t now = (uint32_t)Sys_Milliseconds();
    for (coduomp_config_profile_t *profile = coduomp_config_profiles; profile; profile = profile->next) {
        coduomp_config_poll(profile, 0);
        if (profile == coduomp_config_active && !profile->loads && cmd_text.cursize == 0 && com_configAutowriteEnabled) {
            if (profile->loading)
                coduomp_config_loaded(profile);
            if (!profile->firstChange && (cvar_modifiedFlags & CVAR_ARCHIVE))
                profile->firstChange = profile->lastChange = now ? now : 1;
            if ((uint32_t)(now - profile->lastChange) >= CODUOMP_CONFIG_QUIET_MS ||
                (profile->firstChange && (uint32_t)(now - profile->firstChange) >= CODUOMP_CONFIG_MAX_AGE_MS))
                coduomp_config_capture_pending(profile);
        }
        if (profile->pending && (!profile->retryAt || (int32_t)(now - profile->retryAt) >= 0))
            coduomp_config_dispatch(profile);
    }
    coduomp_config_present_recovery();
}

/* NOT_FROM_ORIGINAL_SOURCE: explicit shutdown/profile departure requests an
 * immediate snapshot and a bounded wait, without serializing the next profile. */
void coduomp_config_flush(void)
{
    coduomp_config_profile_t *profile = coduomp_config_active;
    if (com_errorEntered || !profile)
        return;
    if (profile->loading && !profile->loads && cmd_text.cursize == 0 && com_configAutowriteEnabled)
        coduomp_config_loaded(profile);
    coduomp_config_capture_pending(profile);
    coduomp_config_poll(profile, CODUOMP_CONFIG_WAIT_MS);
    coduomp_config_dispatch(profile);
    if (!coduomp_config_poll(profile, CODUOMP_CONFIG_WAIT_MS))
        Com_Printf("Config save is still pending for %s; its owned snapshot is retained.\n", profile->path);
}

/* NOT_FROM_ORIGINAL_SOURCE: all named exports and server promotions use the
 * same transaction. Exporting elsewhere never unlocks a protected primary. */
qboolean coduomp_config_save(const char *root, const char *game, const char *file, qboolean defaults)
{
    char path[CODUOMP_CONFIG_PATH], error[256] = "could not resolve destination";
    if (!coduomp_fs_config_destination(root, game, file, path, error)) {
        Com_Printf("Cannot save settings: %s\n", error);
        return qfalse;
    }
    coduomp_config_profile_t *owner = NULL;
    for (coduomp_config_profile_t *profile = coduomp_config_profiles; profile; profile = profile->next) {
        if (!Q_stricmp(profile->path, path)) {
            owner = profile;
            break;
        }
    }
    if (owner && owner == coduomp_config_active && owner->loading && !owner->loads && com_configAutowriteEnabled && !coduomp_command_config_active())
        coduomp_config_loaded(owner);
    if (owner && (owner->protected || owner->loading || owner->loads || owner->discarded)) {
        Com_Printf("Settings at %s are protected. Use config_retry after repair or config_restore to restore a backup.\n", path);
        return qfalse;
    }
    if (root && coduomp_config_active && (coduomp_config_active->protected || coduomp_config_active->loading || coduomp_config_active->loads)) {
        Com_Printf("Cannot promote a recovery or incomplete session into global settings.\n");
        return qfalse;
    }
    if (owner && !coduomp_config_poll(owner, CODUOMP_CONFIG_WAIT_MS)) {
        Com_Printf("A save is already pending for %s.\n", path);
        return qfalse;
    }
    size_t size;
    char *data = coduomp_config_capture(defaults, &size, error);
    if (!data) {
        Com_Printf("Cannot save settings: %s\n", error);
        return qfalse;
    }
    if (!owner) {
        owner = calloc(1, sizeof(*owner));
        if (!owner) {
            free(data);
            return qfalse;
        }
        strcpy(owner->path, path);
        owner->next = coduomp_config_profiles;
        coduomp_config_profiles = owner;
        char *old;
        size_t oldSize;
        int read = coduomp_config_read_disk(path, &old, &oldSize, &owner->observed, error);
        free(old);
        if (read < 0) {
            free(data);
            coduomp_config_fail(owner, path, 1, error);
            return qfalse;
        }
    }
    free(owner->pending);
    owner->pending = data;
    owner->pendingSize = size;
    owner->pendingRevision = coduomp_config_revision;
    coduomp_config_dispatch(owner);
    if (!coduomp_config_poll(owner, CODUOMP_CONFIG_WAIT_MS) || owner->pending) {
        Com_Printf("Save to %s has not been confirmed.\n", path);
        return qfalse;
    }
    Com_Printf("Settings saved to %s.\n", path);
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: recovery selection is restricted to the same
 * profile's managed snapshots; each snapshot is checked before use. */
static qboolean coduomp_config_backup(coduomp_config_profile_t *profile, const char *selection, char **data, size_t *size, char path[CODUOMP_CONFIG_PATH])
{
    int written;
    if (!strcmp(selection, "loaded"))
        written = snprintf(path, CODUOMP_CONFIG_PATH, "%s.loaded.cfg", profile->path);
    else if (strlen(selection) == 1 && selection[0] >= '1' && selection[0] <= '3')
        written = snprintf(path, CODUOMP_CONFIG_PATH, "%s.backup%c.cfg", profile->path, selection[0]);
    else
        return qfalse;
    if (written < 0 || written >= CODUOMP_CONFIG_PATH)
        return qfalse;
    char error[256];
    if (!coduomp_config_read_snapshot(path, data, size, error))
        return qfalse;
    coduomp_config_error_t problem;
    if (!coduomp_config_validate(*data, *size, &problem)) {
        free(*data);
        *data = NULL;
        return qfalse;
    }
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: give recovery choices a timestamp and an exact
 * path, without treating a snapshot's date as evidence of successful gameplay. */
static void coduomp_config_backup_description(const char *path, char description[CODUOMP_CONFIG_PATH + 100])
{
    char *data, error[256], date[80] = "date unavailable";
    size_t size;
    coduomp_config_revision_t revision;
    if (coduomp_config_read_disk(path, &data, &size, &revision, error) == 1) {
#if defined(_WIN32)
        time_t seconds = (time_t)(revision.modified / INT64_C(10000000) - INT64_C(11644473600));
#else
        time_t seconds = (time_t)(revision.modified / INT64_C(1000000000));
#endif
        const struct tm *local = localtime(&seconds);
        if (local)
            strftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S %Z", local);
        free(data);
    }
    snprintf(description, CODUOMP_CONFIG_PATH + 100, "%s\nSaved: %s", path, date);
}

/* NOT_FROM_ORIGINAL_SOURCE: recovery actions must be intentional user actions,
 * not commands hidden inside a config being executed. */
static qboolean coduomp_config_recovery_allowed(void)
{
    if (!coduomp_config_active || coduomp_command_config_active() || coduomp_config_active->loads) {
        Com_Printf("Run recovery commands from the console after config execution has finished.\n");
        return qfalse;
    }
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: load a checked backup for this session only. */
static void coduomp_config_use_backup_f(void)
{
    if (!coduomp_config_recovery_allowed())
        return;
    const char *selection = Cmd_Argc() == 2 ? Cmd_Argv(1) : "loaded";
    char *data, path[CODUOMP_CONFIG_PATH];
    size_t size;
    if (!coduomp_config_backup(coduomp_config_active, selection, &data, &size, path)) {
        Com_Printf("No complete backup selected. Use config_use_backup <loaded|1|2|3>.\n");
        return;
    }
    coduomp_config_pause("using a backup for this session");
    coduomp_config_active->announced = qtrue;
    coduomp_command_queue_config(coduomp_config_active, path, data, size);
    free(data);
}

/* NOT_FROM_ORIGINAL_SOURCE: defaults remain temporary until an explicit named
 * export; the original and recovery history stay protected. */
static void coduomp_config_defaults_f(void)
{
    if (!coduomp_config_recovery_allowed())
        return;
    coduomp_config_pause("using temporary defaults");
    coduomp_config_active->announced = qtrue;
    Cbuf_InsertText("unbindall\ncvar_restart\nexec default_mp.cfg\n");
}

/* NOT_FROM_ORIGINAL_SOURCE: a manual repair is re-read and checked. All
 * startup scripts must finish again before automatic saving is re-enabled. */
static void coduomp_config_retry_f(void)
{
    if (!coduomp_config_recovery_allowed())
        return;
    coduomp_config_profile_t *profile = coduomp_config_active;
    if (!coduomp_config_poll(profile, CODUOMP_CONFIG_WAIT_MS)) {
        Com_Printf("Wait for the outstanding config transaction before retrying.\n");
        return;
    }
    char *data, error[256];
    size_t size;
    coduomp_config_revision_t observed;
    if (coduomp_config_read_disk(profile->path, &data, &size, &observed, error) != 1) {
        coduomp_config_fail(profile, profile->path, 1, "config could not be reloaded; repair the original first");
        Com_Printf("Cannot retry %s: repair the original file first.\n", profile->path);
        return;
    }
    if (size >= 3 && !memcmp(data, "\xef\xbb\xbf", 3)) {
        memmove(data, data + 3, size - 3 + 1);
        size -= 3;
    }
    coduomp_config_error_t problem;
    if (!coduomp_config_validate(data, size, &problem)) {
        coduomp_config_fail(profile, profile->path, problem.line, problem.reason);
        Com_Printf("Cannot retry %s:%u: %s\n", profile->path, problem.line, problem.reason);
        free(data);
        return;
    }
    profile->observed = observed;
    profile->protected = qfalse;
    profile->loading = qtrue;
    profile->announced = qfalse;
    profile->loadedSnapshot = qfalse;
    profile->problem[0] = '\0';
    profile->failures = 0;
    free(profile->pending);
    profile->pending = NULL;
    coduomp_config_clear_inputs(profile);
    Cbuf_InsertText("exec autoexec_mp.cfg\n");
    coduomp_command_queue_config(profile, profile->path, data, size);
    free(data);
}

/* NOT_FROM_ORIGINAL_SOURCE: explicit restore preserves even malformed original
 * bytes under a unique name before replacement, then loads the selected bytes. */
static void coduomp_config_restore_f(void)
{
    if (!coduomp_config_recovery_allowed())
        return;
    coduomp_config_profile_t *profile = coduomp_config_active;
    if (!coduomp_config_poll(profile, CODUOMP_CONFIG_WAIT_MS))
        return;
    const char *selection = Cmd_Argc() == 2 ? Cmd_Argv(1) : "loaded";
    char *data, path[CODUOMP_CONFIG_PATH], error[256];
    size_t size;
    if (!coduomp_config_backup(profile, selection, &data, &size, path)) {
        Com_Printf("No complete backup selected. Use config_restore <loaded|1|2|3>.\n");
        return;
    }
    char *original;
    size_t originalSize;
    coduomp_config_revision_t observed;
    if (coduomp_config_read_disk(profile->path, &original, &originalSize, &observed, error) < 0 ||
        !coduomp_config_prepare_directory(profile->path)) {
        Com_Printf("Cannot preserve the original config: %s\n", error);
        free(original);
        free(data);
        return;
    }
    free(original);
    profile->protected = qtrue;
    profile->announced = qtrue;
    profile->job = coduomp_config_store_start(profile->path, data, size, &observed, qtrue, qfalse);
    coduomp_config_result_t result;
    if (profile->job && coduomp_config_store_finish(profile->job, CODUOMP_CONFIG_WAIT_MS, &result)) {
        profile->job = NULL;
        if (result.status == CODUOMP_CONFIG_COMMITTED) {
            Com_Printf("Restored settings to %s. Original preserved at %s\n", profile->path,
                result.rollback[0] ? result.rollback : "(original was missing)");
            profile->observed = result.revision;
            profile->protected = qfalse;
            profile->loading = qtrue;
            profile->loadedSnapshot = qfalse;
            profile->problem[0] = '\0';
            free(profile->pending);
            profile->pending = NULL;
            coduomp_command_queue_config(profile, path, data, size);
            coduomp_fs_config_changed();
        } else
            Com_Printf("Restore not confirmed: %s. Original and transaction files remain protected.\n", result.detail);
    } else
        Com_Printf("Restore not confirmed yet. Saving remains paused; use config_retry after the transaction finishes.\n");
    free(data);
}

/* NOT_FROM_ORIGINAL_SOURCE: report state and exact recovery locations without
 * printing any saved setting values. */
static void coduomp_config_status_f(void)
{
    coduomp_config_profile_t *profile = coduomp_config_active;
    if (!profile)
        return;
    Com_Printf("Settings: %s\nSaving: %s\n", profile->path, profile->protected ? "paused" : profile->loading ? "waiting for config execution" : "enabled");
    if (profile->problem[0])
        Com_Printf("%s\n", profile->problem);
    const char *selections[] = {"loaded", "1", "2", "3"};
    for (size_t i = 0; i < sizeof(selections) / sizeof(selections[0]); ++i) {
        char *data, path[CODUOMP_CONFIG_PATH];
        size_t size;
        if (coduomp_config_backup(profile, selections[i], &data, &size, path)) {
            char description[CODUOMP_CONFIG_PATH + 100];
            coduomp_config_backup_description(path, description);
            Com_Printf("Complete backup %s: %s\n", selections[i], description);
            free(data);
        }
    }
    Com_Printf("config_use_backup <loaded|1|2|3>: temporary session\nconfig_defaults: temporary defaults\nconfig_retry: reload after manual repair\nconfig_restore <loaded|1|2|3>: preserve original, replace, and load\n");
}

/* NOT_FROM_ORIGINAL_SOURCE: normal localized startup precedes presentation;
 * platform dialogs also work when no renderer can be created. */
void coduomp_config_present_recovery(void)
{
    coduomp_config_profile_t *profile = coduomp_config_active;
    if (!profile || !profile->protected || profile->announced || profile->loads || !coduomp_config_registered)
        return;
    profile->announced = qtrue;
    if (!coduomp_config_dialog) {
        coduomp_config_status_f();
        return;
    }
    char *data = NULL, path[CODUOMP_CONFIG_PATH] = "";
    size_t size = 0;
    const char *selections[] = {"loaded", "1", "2", "3"};
    for (size_t i = 0; i < sizeof(selections) / sizeof(selections[0]); ++i) {
        if (coduomp_config_backup(profile, selections[i], &data, &size, path))
            break;
    }
    char description[CODUOMP_CONFIG_PATH + 100];
    if (data)
        coduomp_config_backup_description(path, description);
    int choice = coduomp_config_dialog(profile->problem, data ? description : NULL);
    if (choice < 0) {
        free(data);
        Com_Quit_f();
    }
    if (choice > 0 && data)
        coduomp_command_queue_config(profile, path, data, size);
    else
        Cbuf_InsertText("unbindall\ncvar_restart\nexec default_mp.cfg\n");
    free(data);
}

/* NOT_FROM_ORIGINAL_SOURCE: register user-facing recovery controls once. */
void coduomp_config_register(coduomp_config_dialog_t dialog)
{
    coduomp_config_dialog = dialog;
    if (coduomp_config_registered)
        return;
    coduomp_config_registered = qtrue;
    Cmd_AddCommand("config_status", coduomp_config_status_f);
    Cmd_AddCommand("config_use_backup", coduomp_config_use_backup_f);
    Cmd_AddCommand("config_defaults", coduomp_config_defaults_f);
    Cmd_AddCommand("config_retry", coduomp_config_retry_f);
    Cmd_AddCommand("config_restore", coduomp_config_restore_f);
}

/* NOT_FROM_ORIGINAL_SOURCE: clearing server settings first drains writers and
 * retires in-memory recovery state so deleted settings cannot return later. */
qboolean coduomp_config_forget_tree(const char *root)
{
    char normalized[CODUOMP_CONFIG_PATH], error[256];
#if !defined(_WIN32)
    char resolved[CODUOMP_CONFIG_PATH];
    if (realpath(root, resolved))
        root = resolved;
#endif
    if (!coduomp_config_path(root, normalized, error)) {
        Com_Printf("Cannot clear settings: %s\n", error);
        return qfalse;
    }
    root = normalized;
    size_t length = strlen(root);
    while (length && (root[length - 1] == '/' || root[length - 1] == '\\'))
        normalized[--length] = '\0';
    for (coduomp_config_profile_t *profile = coduomp_config_profiles; profile; profile = profile->next) {
        if (Q_stricmpn(profile->path, root, (int)length) || (profile->path[length] != '/' && profile->path[length] != '\\'))
            continue;
        if (!coduomp_config_poll(profile, CODUOMP_CONFIG_WAIT_MS) || profile->loadedJob) {
            Com_Printf("Wait for settings I/O to finish before clearing server configs.\n");
            return qfalse;
        }
        profile->discarded = qtrue;
        coduomp_config_fail(profile, profile->path, 1, "server settings were explicitly cleared");
        profile->announced = qtrue;
        free(profile->pending);
        profile->pending = NULL;
        coduomp_config_clear_inputs(profile);
    }
    return qtrue;
}
