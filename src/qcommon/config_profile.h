#ifndef CODUOMP_CONFIG_PROFILE_H
#define CODUOMP_CONFIG_PROFILE_H

#include "config_store.h"

typedef struct coduomp_config_profile_s coduomp_config_profile_t;
typedef struct {
    char *data;
    size_t size, capacity;
    char error[256];
} coduomp_config_buffer_t;

typedef int (*coduomp_config_dialog_t)(const char *message, const char *backup);

qboolean coduomp_config_append(coduomp_config_buffer_t *buffer, const char *command, const char *name, const char *value);
void coduomp_config_capture_bindings(coduomp_config_buffer_t *buffer);
void coduomp_config_settings_changed(void);
void coduomp_config_begin_profile(void);
void coduomp_config_leave_profile(void);
void coduomp_config_register(coduomp_config_dialog_t dialog);
void coduomp_config_present_recovery(void);
qboolean coduomp_config_preference(const char *file, const char *name);
void coduomp_config_pause(const char *reason);
void coduomp_config_abort_load(void);
void coduomp_config_fail(coduomp_config_profile_t *profile, const char *source, unsigned line, const char *reason);
coduomp_config_profile_t *coduomp_config_current_profile(void);
void coduomp_config_load_reference(coduomp_config_profile_t *profile, int delta);
qboolean coduomp_config_is_primary(const char *file);
int coduomp_config_read(coduomp_config_profile_t *profile, const char *file, qboolean nested, qboolean checked, char **data, size_t *size, char source[CODUOMP_CONFIG_PATH]);
void coduomp_config_service(void);
void coduomp_config_flush(void);
qboolean coduomp_config_save(const char *root, const char *game, const char *file, qboolean defaults);
qboolean coduomp_config_forget_tree(const char *root);

/* Command-queue ownership hooks; every generated command retains its source
 * profile until execution actually ends, including across wait and restarts. */
qboolean coduomp_command_queue_config(coduomp_config_profile_t *profile, const char *source, const char *data, size_t size);
void coduomp_command_queue_profile(const char *file, qboolean append);
void coduomp_command_abort_config(void);
qboolean coduomp_command_config_active(void);

#endif
