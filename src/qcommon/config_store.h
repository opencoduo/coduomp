#ifndef CODUOMP_CONFIG_STORE_H
#define CODUOMP_CONFIG_STORE_H

#include "q_shared_types.h"
#include <stddef.h>
#include <stdint.h>

enum { CODUOMP_CONFIG_PATH = 4096, CODUOMP_CONFIG_MAX_BYTES = 65535 };

typedef struct {
    qboolean exists;
    uint64_t size, digest, device, file;
    int64_t modified;
} coduomp_config_revision_t;

typedef enum {
    CODUOMP_CONFIG_NOT_COMMITTED,
    CODUOMP_CONFIG_COMMITTED,
    CODUOMP_CONFIG_DURABILITY_UNCONFIRMED,
    CODUOMP_CONFIG_RECOVERY_REQUIRED
} coduomp_config_commit_t;

typedef struct coduomp_config_job_s coduomp_config_job_t;

/* NOT_FROM_ORIGINAL_SOURCE: persistence operates only on owned snapshots and
 * resolved paths; worker threads never access engine globals or FS handles. */
typedef struct {
    coduomp_config_commit_t status;
    coduomp_config_revision_t revision;
    char detail[256];
    char rollback[CODUOMP_CONFIG_PATH];
} coduomp_config_result_t;

uint64_t coduomp_config_digest(const void *data, size_t size);
qboolean coduomp_config_path(const char *input, char output[CODUOMP_CONFIG_PATH], char error[256]);
int coduomp_config_read_disk(const char *path, char **data, size_t *size, coduomp_config_revision_t *revision, char error[256]);
qboolean coduomp_config_same_revision(const coduomp_config_revision_t *a, const coduomp_config_revision_t *b);
coduomp_config_job_t *coduomp_config_store_start(const char *path, const char *data, size_t size, const coduomp_config_revision_t *expected, qboolean preserveOriginal, qboolean recoveryHistory);
qboolean coduomp_config_store_finish(coduomp_config_job_t *job, unsigned waitMilliseconds, coduomp_config_result_t *result);
qboolean coduomp_config_recovery_exists(const char *path);
qboolean coduomp_config_read_snapshot(const char *path, char **data, size_t *size, char error[256]);
char *coduomp_config_envelope(const char *data, size_t size, size_t *outputSize);

#endif
