#ifndef CANOPEN_EDS_H_
#define CANOPEN_EDS_H_

#include "canopen/types.h"

struct canopen_eds;

enum eds_obj_access {
	EDS_OBJ_R = 1,
	EDS_OBJ_W = 2,
	EDS_OBJ_RW = 3,
	EDS_OBJ_CONST = 4,
};

struct eds_obj {
	uint32_t key;
	enum canopen_type type;
	enum eds_obj_access access;
	const char* name;
	const char* default_value;
	const char* low_limit;
	const char* high_limit;
	const char* unit;
	const char* scaling;
};

int eds_db_load(void);
void eds_db_unload(void);

const struct canopen_eds* eds_db_find(int vendor, int product, int revision);

const struct eds_obj* eds_obj_find(const struct canopen_eds* eds,
				   int index, int subindex);

static inline int eds_obj_index(const struct eds_obj* obj)
{
	return obj->key >> 8;
}

static inline int eds_obj_subindex(const struct eds_obj* obj)
{
	return obj->key & 0xff;
}

const struct eds_obj* eds_obj_first(const struct canopen_eds* eds);
const struct eds_obj* eds_obj_next(const struct canopen_eds* eds,
				   const struct eds_obj* obj);

#endif /* CANOPEN_EDS_H_ */
