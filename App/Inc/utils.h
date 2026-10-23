#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <stdbool.h>

char *str_upr (char *s);
char *str_trim(char *str);
char *strncpy_term (char *dst, const char *src, size_t dsize);
bool parse_int(const char *s, int *value);
size_t get_next_token(const char *str, size_t *whitespace_len);

#endif /* UTILS_H */
