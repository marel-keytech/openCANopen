#ifndef STRING_UTILS_H_
#define STRING_UTILS_H_

#include <string.h>
#include <ctype.h>

#define SPACE_CHARACTER " \f\n\r\t\v"

static inline char* string_trim_left(char* str)
{
	return str + strspn(str, SPACE_CHARACTER);
}

static inline char* string_trim_right(char* str)
{
	char* end = str + strlen(str) - 1;
	while (str < end && isspace(*end))
		*end-- = '\0';
	return str;
}

static inline char* string_trim(char* str)
{
	return string_trim_right(string_trim_left(str));
}

static inline int string_is_empty(const char* str)
{
	return *str == '\0';
}

#undef SPACE_CHARACTER

#endif /* STRING_UTILS_H_ */
