#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "ini_parser.h"
#include "vector.h"

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

static struct ini_section* ini__new_section(const char* name)
{
	size_t name_size = strlen(name) + 1;

	struct ini_section* section = malloc(sizeof(*section) + name_size);
	if (!section)
		return NULL;

	memset(section, 0, sizeof(*section));

	memcpy(section->section, name, name_size);

	if (vector_init(&section->kv, 8 * sizeof(void*)) < 0)
		goto failure;

	return section;

failure:
	free(section);
	return NULL;
}

static int ini__append_new_section(struct ini_file* self, const char* name)
{
	struct ini_section* section = ini__new_section(name);
	if (!section)
		return -1;

	if (vector_append(&self->section, &section, sizeof(void*)) < 0)
		goto failure;

	return 0;

failure:
	free(section);
	return -1;
}

static int ini__parse_section(struct ini_file* self, char* str)
{
	size_t len = strlen(str);
	char* end = str + len - 1;
	if (*str != '[' || *end != ']')
		return -1;

	*end = '\0';
	const char* name = ini__to_lower(str + 1);

	return ini__append_new_section(self, name);
}

static struct ini_key_value* ini__new_key_value(const char* key,
						const char* value)
{
	size_t key_size = strlen(key) + 1;
	size_t value_size = strlen(value) + 1;

	struct ini_key_value* kv = malloc(sizeof(*kv) + key_size + value_size);
	if (!kv)
		return NULL;

	memset(kv, 0, sizeof(*kv));

	kv->value = kv->key + key_size;

	strcpy(kv->key, key);
	strcpy(kv->value, value);

	return kv;
}

static inline struct ini_section* ini__current_section(struct ini_file* self)
{
	size_t index = self->section.index / sizeof(void*) - 1;
	struct ini_section** section = self->section.data;
	return section[index];
}

static int ini__append_new_key_value(struct ini_file* self, const char* key,
				     const char* value)
{
	struct ini_section* section = ini__current_section(self);

	struct ini_key_value* kv = ini__new_key_value(key, value);
	if (!kv)
		return -1;

	if (vector_append(&section->kv, &kv, sizeof(void*)) < 0)
		goto failure;

	return 0;

failure:
	free(kv);
	return -1;
}

static int ini__parse_key_value(struct ini_file* self, char* str)
{
	char* eq = strchr(str, '=');
	if (!eq)
		return -1;

	*eq = '\0';

	char* value = ini__trim_left(eq + 1);
	char* key = ini__to_lower(ini__trim_right(str));

	return ini__append_new_key_value(self, key, value);
}

int ini__parse_line(struct ini_file* self, char* line)
{
	char* trimmed = ini__trim(line);

	if (ini__is_empty(trimmed) || ini__is_comment(trimmed))
		return 0;

	if (ini__is_section(trimmed))
		return ini__parse_section(self, trimmed);

	return ini__parse_key_value(self, trimmed);
}

static int ini__section_cmp(const void* p1, const void* p2)
{
	const struct ini_section* const* sp1 = p1;
	const struct ini_section* const* sp2 = p2;

	const struct ini_section* s1 = *sp1;
	const struct ini_section* s2 = *sp2;

	return strcmp(s1->section, s2->section);
}

static int ini__key_cmp(const void* p1, const void* p2)
{
	const struct ini_key_value* const* kvp1 = p1;
	const struct ini_key_value* const* kvp2 = p2;

	const struct ini_key_value* kv1 = *kvp1;
	const struct ini_key_value* kv2 = *kvp2;

	return strcmp(kv1->key, kv2->key);
}

static inline void ini__sort(struct ini_file* self)
{
	qsort(self->section.data, self->section.index / sizeof(void*),
	      sizeof(struct section*), ini__section_cmp);

	struct ini_section** section = self->section.data;
	size_t section_end = self->section.index / sizeof(void*);

	for (size_t i = 0; i < section_end; ++i) {
		struct ini_key_value** kv = section[i]->kv.data;
		size_t kv_size = section[i]->kv.index / sizeof(void*);

		qsort(kv, kv_size, sizeof(struct kv*), ini__key_cmp);
	}
}

int ini_parse(struct ini_file* self, FILE* stream)
{
	int rc = -1;
	char* line = NULL;
	size_t size = 0;

	memset(self, 0, sizeof(*self));

	vector_init(&self->section, 1024 * sizeof(void*));

	if (ini__append_new_section(self, "(root)") < 0)
		return -1;

	while (getline(&line, &size, stream) >= 0)
		if (ini__parse_line(self, line) < 0)
			goto done;

	ini__sort(self);

	rc = 0;
done:
	free(line);
	return rc;
}

void ini_destroy(struct ini_file* file)
{
	struct ini_section** section = file->section.data;
	size_t section_end = file->section.index / sizeof(void*);

	for (size_t i = 0; i < section_end; ++i) {
		struct ini_key_value** kv = section[i]->kv.data;
		size_t kv_end = section[i]->kv.index / sizeof(void*);

		for (int j = 0; j < kv_end; ++j)
			free(kv[j]);

		vector_destroy(&section[i]->kv);
		free(section[i]);
	}

	vector_destroy(&file->section);
}

const char* ini_find_key(const struct ini_section* section, const char* key)
{
	struct ini_key_value** kvp;
	char kbuf[sizeof(struct ini_key_value) + strlen(key) + 1];
	struct ini_key_value* key_key = (void*)kbuf;
	strcpy(key_key->key, key);
	kvp = bsearch(&key_key, section->kv.data,
		     section->kv.index / sizeof(void*), sizeof(void*),
		     ini__key_cmp);

	if (!kvp)
		return NULL;

	struct ini_key_value* kv = *kvp;

	return kv->value;
}

const char* ini_find(const struct ini_file* file, const char* section,
		     const char* key)
{
	struct ini_section** sp;
	char sbuf[sizeof(struct ini_section) + strlen(section) + 1];
	struct ini_section* section_key = (void*)sbuf;
	strcpy(section_key->section, section);
	sp = bsearch(&section_key, file->section.data,
		     file->section.index / sizeof(void*), sizeof(void*),
		     ini__section_cmp);
	if (!sp)
		return NULL;

	return ini_find_key(*sp, key);
}
