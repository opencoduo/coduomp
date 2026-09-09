#if !defined(WINDOWS_BEHAVIOR)
#error "config_script.c is client-only"
#endif

#include "config_script.h"
#include "q_string.h"

#include <string.h>
#include <stdio.h>

/* NOT_FROM_ORIGINAL_SOURCE: apply the config extension without silently
 * truncating a name that already fits the virtual filesystem's path limit. */
qboolean coduomp_config_filename(const char *input, char *output, size_t capacity)
{
    size_t length = strlen(input);
    qboolean extension = qfalse;
    for (size_t i = length; i > 1 && input[i - 1] != '/' && input[i - 1] != '\\'; --i) {
        if (input[i - 1] == '.') {
            extension = qtrue;
            break;
        }
    }
    if (length + (extension ? 0 : 4) >= capacity)
        return qfalse;
    snprintf(output, capacity, "%s%s", input, extension ? "" : ".cfg");
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: checked settings share comment-aware boundaries
 * in execution and preflight. Ordinary scripts preserve the command buffer's
 * newline and unquoted-semicolon delimiters. Backslashes are ordinary bytes. */
size_t coduomp_command_span(const char *text, size_t length, qboolean checked)
{
    qboolean quoted = qfalse, block = qfalse, comment = qfalse;
    for (size_t i = 0; i < length; ++i) {
        const char c = text[i];
        const char next = i + 1 < length ? text[i + 1] : '\0';
        if (checked && block) {
            if (c == '*' && next == '/') {
                block = qfalse;
                ++i;
            }
            continue;
        }
        if (c == '\r' || c == '\n')
            return i;
        if (checked && comment)
            continue;
        if (c == '"')
            quoted = !quoted;
        if (quoted)
            continue;
        if (checked && c == '/' && next == '/')
            comment = qtrue;
        else if (checked && c == '/' && next == '*') {
            block = qtrue;
            ++i;
        } else if (c == ';')
            return i;
    }
    return length;
}

/* NOT_FROM_ORIGINAL_SOURCE: attach a structural failure to its exact byte. */
static qboolean coduomp_script_error(coduomp_config_error_t *error, size_t byte, const char *reason)
{
    if (error != NULL) {
        error->byte = byte;
        error->line = 1;
        error->reason = reason;
    }
    return qfalse;
}

/* NOT_FROM_ORIGINAL_SOURCE: length-delimited tokenization shared by command
 * execution, validation, preference inspection, and serializer round trips.
 * Execution accepts the end of a quoted token or comment at end of input,
 * as Cmd_TokenizeString2 does; saved-settings preflight requires closure. */
qboolean coduomp_command_tokens(const char *text, size_t length, int maxTokens, qboolean checked, coduomp_config_tokens_t *tokens, coduomp_config_error_t *error)
{
    size_t i = 0, used = 0;
    tokens->argc = 0;
    if (length >= sizeof(tokens->text))
        return coduomp_script_error(error, 0, "command exceeds token storage capacity");
    while (i < length) {
        if (maxTokens > 0 && --maxTokens == 0) {
            if (tokens->argc == CMD_ARGUMENT_CAPACITY || length - i + 1 > sizeof(tokens->text) - used)
                return coduomp_script_error(error, i, "too many command arguments");
            tokens->argv[tokens->argc++] = tokens->text + used;
            memcpy(tokens->text + used, text + i, length - i);
            tokens->text[used + length - i] = '\0';
            return qtrue;
        }
        for (;;) {
            while (i < length && (unsigned char)text[i] <= ' ')
                ++i;
            if (i == length || (i + 1 < length && text[i] == '/' && text[i + 1] == '/'))
                return qtrue;
            if (i + 1 >= length || text[i] != '/' || text[i + 1] != '*')
                break;
            const size_t start = i;
            i += 2;
            while (i + 1 < length && (text[i] != '*' || text[i + 1] != '/'))
                ++i;
            if (i + 1 >= length)
                return checked ? coduomp_script_error(error, start, "unfinished block comment") : qtrue;
            i += 2;
        }
        if (tokens->argc == CMD_ARGUMENT_CAPACITY)
            return checked ? coduomp_script_error(error, i, "too many command arguments") : qtrue;
        tokens->argv[tokens->argc++] = tokens->text + used;
        const qboolean quoted = text[i] == '"';
        const size_t start = i;
        if (quoted)
            ++i;
        while (i < length) {
            const char c = text[i];
            if (c == '"' || (!quoted && ((unsigned char)c <= ' ' ||
                (c == '/' && i + 1 < length && (text[i + 1] == '/' || text[i + 1] == '*')))))
                break;
            if (checked && quoted && (c == '\n' || c == '\r'))
                return coduomp_script_error(error, start, "quoted value crosses a line boundary");
            if (used + 1 >= sizeof(tokens->text))
                return coduomp_script_error(error, i, "command exceeds token storage capacity");
            tokens->text[used++] = text[i++];
        }
        tokens->text[used++] = '\0';
        if (quoted) {
            if (i == length)
                return checked ? coduomp_script_error(error, start, "unfinished quoted value") : qtrue;
            ++i;
        }
        if (i < length && (unsigned char)text[i] <= ' ')
            ++i;
    }
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: reject only definitely incomplete built-in
 * commands. A bare cvar or bind key remains a valid query; mod commands do
 * not need to be known during preflight. */
static qboolean coduomp_config_signature(const coduomp_config_tokens_t *tokens)
{
    if (tokens->argc == 0)
        return qtrue;
    const char *name = tokens->argv[0];
    if (!Q_stricmp(name, "set"))
        return tokens->argc >= 3 && tokens->argv[1][0] != '\0';
    if (!Q_stricmp(name, "seta") || !Q_stricmp(name, "setu") || !Q_stricmp(name, "sets"))
        return tokens->argc == 3 && tokens->argv[1][0] != '\0';
    if (!Q_stricmp(name, "setfromcvar"))
        return tokens->argc == 3 && tokens->argv[1][0] != '\0' && tokens->argv[2][0] != '\0';
    if (!Q_stricmp(name, "bind"))
        return tokens->argc >= 2 && tokens->argv[1][0] != '\0';
    if (!Q_stricmp(name, "exec") || !Q_stricmp(name, "vstr") || !Q_stricmp(name, "unbind") || !Q_stricmp(name, "reset"))
        return tokens->argc == 2 && tokens->argv[1][0] != '\0';
    return qtrue;
}

/* NOT_FROM_ORIGINAL_SOURCE: validate the whole script before any of its
 * commands are queued. Empty files and a missing final newline are valid. */
qboolean coduomp_config_validate(const char *text, size_t length, coduomp_config_error_t *error)
{
    coduomp_config_error_t local = {0, 1, NULL};
    if (error == NULL)
        error = &local;
    *error = local;
    for (size_t i = 0; i < length; ++i) {
        if (text[i] == '\0') {
            coduomp_script_error(error, i, "embedded NUL byte (binary or unsupported text encoding)");
            goto failed;
        }
    }
    if (length > CBUF_TEXT_CAPACITY - 1) {
        coduomp_script_error(error, 0, "file exceeds command queue capacity");
        goto failed;
    }
    if (length >= 2 && (((unsigned char)text[0] == 255 && (unsigned char)text[1] == 254) ||
                        ((unsigned char)text[0] == 254 && (unsigned char)text[1] == 255))) {
        coduomp_script_error(error, 0, "UTF-16 is unsupported; save as UTF-8 or legacy text");
        goto failed;
    }
    for (size_t offset = 0; offset < length;) {
        size_t span = coduomp_command_span(text + offset, length - offset, qtrue);
        if (span >= CBUF_COMMAND_CAPACITY) {
            coduomp_script_error(error, offset, "command exceeds execution capacity");
            goto failed;
        }
        coduomp_config_tokens_t tokens;
        if (!coduomp_command_tokens(text + offset, span, 0, qtrue, &tokens, error)) {
            error->byte += offset;
            goto failed;
        }
        if (!coduomp_config_signature(&tokens)) {
            coduomp_script_error(error, offset, "incomplete built-in command");
            goto failed;
        }
        offset += span + (span < length - offset);
    }
    return qtrue;
failed:
    error->line = 1;
    for (size_t i = 0; i < error->byte; ++i) {
        if (text[i] == '\n' || (text[i] == '\r' && (i + 1 == length || text[i + 1] != '\n')))
            ++error->line;
    }
    return qfalse;
}

/* NOT_FROM_ORIGINAL_SOURCE: inspect accepted command tokens without applying
 * settings or mistaking a quoted binding/value for an assignment. */
qboolean coduomp_config_has_assignment(const char *text, size_t length, const char *name, qboolean checked)
{
    if (!checked) {
        const char *end = memchr(text, '\0', length);
        if (end)
            length = (size_t)(end - text);
    }
    for (size_t offset = 0; offset < length;) {
        size_t span = coduomp_command_span(text + offset, length - offset, checked);
        coduomp_config_tokens_t tokens;
        if (!coduomp_command_tokens(text + offset, span, 0, checked, &tokens, NULL))
            return qfalse;
        if (tokens.argc >= 2 && !Q_stricmp(tokens.argv[0], name))
            return qtrue;
        if (tokens.argc >= 3 && !Q_stricmp(tokens.argv[1], name) &&
            (!Q_stricmp(tokens.argv[0], "set") || !Q_stricmp(tokens.argv[0], "seta") ||
             !Q_stricmp(tokens.argv[0], "setu") || !Q_stricmp(tokens.argv[0], "sets")))
            return qtrue;
        offset += span + (span < length - offset);
    }
    return qfalse;
}
