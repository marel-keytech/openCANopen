#ifndef STRING_UTILS_H_
#define STRING_UTILS_H_

#include <string.h>
#include <ctype.h>

#define SPACE_CHARACTER " \f\n\r\t\v"

static inline char* string_trim_left(char* str)
{
	return str + strspn(str, SPACE_CHARACTER);
}

char* string_trim_right(char* str);

static inline char* string_trim(char* str)
{
	return string_trim_right(string_trim_left(str));
}

static inline int string_is_empty(const char* str)
{
	return *str == '\0';
}

char* string_keep_if(int (*cond)(int c), char* str);

static inline int string_begins_with(const char* cmp, const char* str)
{
	return strncmp(str, cmp, strlen(cmp)) == 0;
}

int string_ends_with(const char* cmp, const char* str);

char* string_tolower(char* str);

char* string_replace_char(char m, char r, char* str);

#ifndef IN_STRING_UTILS_C_
#undef SPACE_CHARACTER
#endif

#endif /* STRING_UTILS_H_ */
