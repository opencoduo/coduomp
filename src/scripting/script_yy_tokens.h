#ifndef SHARED_SCRIPT_YY_TOKENS_H
#define SHARED_SCRIPT_YY_TOKENS_H

#include <stdint.h>

void TextValue(const char *text, int32_t length);
void StringValue(const char *text, int32_t length);
void IntegerValue(const char *text);
void FloatValue(const char *text);

#endif
