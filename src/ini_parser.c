#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "kv.h"
#include "kv_dict.h"
#include "ini_parser.h"
#include "vector.h"

struct ini_parser {
	struct vector section;
	struct vector buffer;
	struct kv_dict* dict;
};

static inline char* ini__trim_left(char* str)
{
	while (isspace(*str))
		++str;
	return str;
}

static inline char* ini__trim_right(char* str)
{
	char* end = str + strlen(str) - 1;
	while (str < end && isspace(*end))
		*end-- = '\0';
	return str;
}

static inline char* ini__trim(char* str)
{
	return ini__trim_right(ini__trim_left(str));
}

static inline int ini__is_section(const char* str)
{
	return *str == '[';
}

static inline int ini__is_comment(const char* str)
{
	return *str == ';';
}

static inline int ini__is_empty(const char* str)
{
	return *str == '\0';
}

static inline char* ini__to_lower(char* str)
{
	char* ptr = str;
	while (*ptr) {
		*ptr = tolower(*ptr);
		++ptr;
	}
	return str;
}

static int ini__parse_section(struct ini_parser* self, char* str)
{
	size_t len = strlen(str);
	char* end = str + len - 1;
	if (*str != '[' || *end != ']')
		return -1;

	*end = '\0';
	char* section = ini__to_lower(str + 1);

	if (vector_assign(&self->section, section, strlen(section)) < 0)
		return -1;

	return 0;
}

static int ini__parse_key_value(struct ini_parser* self, char* str)
{

	char* eq = strchr(str, '=');
	if (!eq)
		return -1;

	*eq = '\0';
	char* value = ini__trim_left(eq + 1);
	char* key = ini__to_lower(ini__trim_right(str));

	if (vector_copy(&self->buffer, &self->section) < 0
	 || vector_append(&self->buffer, ".", 1) < 0
	 || vector_append(&self->buffer, key, strlen(key) + 1) < 0
	 || vector_append(&self->buffer, value, strlen(value) + 1) < 0)
		return -1;

	if (kv_dict_insert(self->dict, (struct kv*)self->buffer.data) < 0)
		return -1;

	return 0;
}

int ini__parse_line(struct ini_parser* self, char* line)
{
	char* trimmed = ini__trim(line);

	if (ini__is_empty(trimmed) || ini__is_comment(trimmed))
		return 0;

	if (ini__is_section(trimmed))
		return ini__parse_section(self, trimmed);

	return ini__parse_key_value(self, trimmed);
}

int ini_parse(struct kv_dict* dict, FILE* stream)
{
	int rc = -1;
	char* line = NULL;
	size_t size = 0;

	struct ini_parser self;
	memset(&self, 0, sizeof(self));

	self.dict = dict;
	if (vector_assign(&self.section, "(root)", 6) < 0)
		return -1;

	if (vector_reserve(&self.buffer, 128) < 0)
		goto buffer_failure;

	while (getline(&line, &size, stream) >= 0)
		if (ini__parse_line(&self, line) < 0)
			goto done;

	rc = 0;
done:
	free(line);
	vector_destroy(&self.buffer);
buffer_failure:
	vector_destroy(&self.section);
	return rc;
}

