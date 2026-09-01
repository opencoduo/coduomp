#ifndef SHARED_SCRIPT_YY_RUNTIME_H
#define SHARED_SCRIPT_YY_RUNTIME_H

#include "qcommon/script_runtime_types.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

int32_t yylex(void);
int32_t yy_get_next_buffer(void);
int32_t yy_get_previous_state(void);
int32_t yy_try_NUL_trans(int32_t state);
void yyrestart(FILE *inputFile);
void yy_switch_to_buffer(script_yy_buffer_t *buffer);
void yy_load_buffer_state(void);
script_yy_buffer_t *yy_create_buffer(FILE *inputFile, int32_t size);
void yy_delete_buffer(script_yy_buffer_t *buffer);
void yy_init_buffer(script_yy_buffer_t *buffer, FILE *inputFile);
void yy_flush_buffer(script_yy_buffer_t *buffer);
script_yy_buffer_t *yy_scan_buffer(char *base, uint32_t size);
script_yy_buffer_t *yy_scan_string(const char *text);
script_yy_buffer_t *yy_scan_bytes(const char *bytes, int32_t length);
void yy_fatal_error(const char *message);
void *yy_flex_alloc(size_t size);
void *yy_flex_realloc(void *ptr, size_t size);
void yy_flex_free(void *ptr);
int32_t yyerror(void);
int32_t yywrap(void);

#endif
