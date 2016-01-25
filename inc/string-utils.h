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

static inline char* string_keep_if(int (*cond)(int c), char* str)
{
	size_t i, k = 0;
	for (i = 0; str[i]; ++i) {
		int c = str[i];
		if (cond(c))
			str[k++] = c;
	}
	str[k] = '\0';
	return str;
}

static inline int string_begins_with(const char* cmp, const char* str)
{
	return strncmp(str, cmp, strlen(cmp)) == 0;
}

static inline int string_ends_with(const char* cmp, const char* str)
{
	size_t cmp_len = strlen(cmp);
	size_t str_len = strlen(str);

	if (cmp_len > str_len)
		return 0;

	return strcmp(str + str_len - cmp_len, cmp) == 0;
}

static inline char* string_tolower(char* str)
{
	char* ptr = str;
	while (*ptr) {
		*ptr = tolower(*ptr);
		++ptr;
	}
	return str;
}

static inline char* string_replace_char(char m, char r, char* str)
{
	char* ptr = str;
	for (; *ptr; ++ptr)
		if (*ptr == m)
			*ptr = r;
	return str;
}

#undef SPACE_CHARACTER

#endif /* STRING_UTILS_H_ */
