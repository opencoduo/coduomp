#ifndef CODUOMP_CONFIG_SCRIPT_H
#define CODUOMP_CONFIG_SCRIPT_H

#include "command_types.h"
#include "q_shared_types.h"

typedef struct {
    size_t byte;
    unsigned line;
    const char *reason;
} coduomp_config_error_t;

/* NOT_FROM_ORIGINAL_SOURCE: owned token storage permits inspection without
 * changing the command interpreter's published arguments. */
typedef struct {
    int argc;
    const char *argv[CMD_ARGUMENT_CAPACITY];
    char text[CMD_TOKEN_BUFFER_CAPACITY];
} coduomp_config_tokens_t;

size_t coduomp_command_span(const char *text, size_t length, qboolean checked);
qboolean coduomp_command_tokens(const char *text, size_t length, int maxTokens, qboolean checked, coduomp_config_tokens_t *tokens, coduomp_config_error_t *error);
qboolean coduomp_config_validate(const char *text, size_t length, coduomp_config_error_t *error);
qboolean coduomp_config_has_assignment(const char *text, size_t length, const char *name, qboolean checked);
qboolean coduomp_config_filename(const char *input, char *output, size_t capacity);

#endif
