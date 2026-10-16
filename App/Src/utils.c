/** odds and ends that it would be nice to find in a library! */

#include <ctype.h>
#include <string.h>

#include "utils.h"

char *str_upr (char *s) {
	for (char *ptr = s; *ptr ; ptr += 1) {
		*ptr = toupper (*ptr);
	}
	return s;
}

char *str_trim(char *str)
{
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
