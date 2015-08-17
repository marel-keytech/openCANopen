#ifndef INI_PARSER_H_INCLUDED_
#define INI_PARSER_H_INCLUDED_

#include <stdio.h>
#include "vector.h"

struct ini_key_value {
	char* value;
	char key[0];
};

struct ini_section {
	struct vector kv;
	char section[0];
};

struct ini_file {
	struct vector section;
};

int ini_parse(struct ini_file* file, FILE* stream);
void ini_destroy(struct ini_file* file);

const char* ini_find(const struct ini_file* file, const char* section,
		     const char* key);
const char* ini_find_key(const struct ini_section* file, const char* key);

static inline size_t ini_get_length(const struct ini_file* ini)
{
	return ini->section.index / sizeof(void*);
}

static inline const struct ini_section*
ini_get_section(const struct ini_file* ini, size_t index)
{
	struct ini_section** section = ini->section.data;
	return section[index];
}

static inline size_t ini_get_section_length(const struct ini_section* section)
{
	return section->kv.index / sizeof(void*);
}

#endif /* INI_PARSER_H_INCLUDED_ */

