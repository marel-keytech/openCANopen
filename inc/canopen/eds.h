#ifndef CANOPEN_EDS_H_
#define CANOPEN_EDS_H_

#include "canopen/types.h"
#include "sys/tree.h"

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

struct eds_obj_node;

RB_HEAD(eds_obj_tree, eds_obj_node);

struct canopen_eds {
	uint32_t vendor;
	uint32_t product;
	uint32_t revision;
	char name[256];

	struct eds_obj_tree obj_tree;
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

struct canopen_eds* eds_db_get(int index);
size_t eds_db_length(void);

const struct eds_obj* eds_obj_first(const struct canopen_eds* eds);
const struct eds_obj* eds_obj_next(const struct canopen_eds* eds,
				   const struct eds_obj* obj);

#endif /* CANOPEN_EDS_H_ */
