/** odds and ends that it would be nice to find in a library! */

#include <ctype.h>

#include "utils.h"

char *str_upr (char *s) {
	for (char *ptr = s; *ptr ; ptr += 1) {
		*ptr = toupper (*ptr);
	}
	return s;
}
