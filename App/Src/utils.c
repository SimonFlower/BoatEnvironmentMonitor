/** odds and ends that it would be nice to find in a library! */

#include <ctype.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>

#include "utils.h"

/**
 * @brief Make a string uppercase
 * @retval the string
 */
char *str_upr (char *s) {
	for (char *ptr = s; *ptr ; ptr += 1) {
		*ptr = toupper (*ptr);
	}
	return s;
}

/**
 * @brief Remove leading and trailing whitespace from a string
 * @param str the string to trim
 * @retval the new start of the string (after the end of any whitespace)
 */
char *str_trim(char *str) {
    char *end;

    while (isspace((unsigned char)*str))
        str++;

    if (*str == '\0')
        return str;

    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end))
        end--;
    end[1] = '\0';

    return str;
}

/**
 * @brief a version of strncpy that always null terminates the destination string
 * @param dst the string to copy to
 * @param src the string to copy from
 * @param dsize the maximum length of dst
 * @retval dst
 */
char *strncpy_term (char *dst, const char *src, size_t dsize) {
	char *ret_val = strncpy (dst, src, dsize);
	if (dsize > 0)
		*(dst + dsize -1) = '\0';
	return ret_val;
}

/**
 * @brief convert a string to an integer
 * @param s the string to convert
 * @param value the converted number
 * @retval true for conversion succeeded, false otherwise
 */
bool parse_int(const char *s, int *value) {
    char *end;
    long result;

    if (*s == '\0')
        return false;

    errno = 0;
    result = strtol(s, &end, 10);

    if (*end != '\0')      // conversion stopped before end of string
        return false;

    if (errno == ERANGE)   // overflow/underflow
        return false;

    *value = (int)result;
    return true;
}

/**
 * @brief Parses a string non-destructively for tokens and whitespace.
 * 
 * Can be called in a while loop, like strtok. The end condition is when
 * both the return value and the whitespace length are zero.
 * 
 * @param str          Pointer to the current position in the string.
 * @param whitespace_len Output pointer to receive the length of the whitespace.
 * @return             The length of the token (non-whitespace characters), 
 *                     or 0 if the string starts with whitespace or is empty.
 */
size_t get_next_token(const char *str, size_t *whitespace_len) {
    if (str == NULL || whitespace_len == NULL) {
        return 0;
    }

    // If we have reached the end of the string
    if (*str == '\0') {
        *whitespace_len = 0;
        return 0;
    }

    // Case 1: The current position starts with whitespace
    if (isspace((unsigned char)*str)) {
        size_t ws_len = 0;
        while (str[ws_len] != '\0' && isspace((unsigned char)str[ws_len])) {
            ws_len++;
        }
        *whitespace_len = ws_len;
        return 0;
    }

    // Case 2: The current position starts with a token (non-whitespace)
    int token_len = 0;
    while (str[token_len] != '\0' && !isspace((unsigned char)str[token_len])) {
        token_len++;
    }

    // Count the whitespace immediately following this token
    size_t ws_len = 0;
    while (str[token_len + ws_len] != '\0' && isspace((unsigned char)str[token_len + ws_len])) {
        ws_len++;
    }

    *whitespace_len = ws_len;
    return token_len;
}

