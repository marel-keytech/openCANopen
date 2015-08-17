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
	enum canopen_type type;
	enum eds_obj_access access;
};

int eds_db_load(void);
void eds_db_unload(void);

const struct canopen_eds* eds_db_find(int vendor, int product, int revision);

const struct eds_obj* eds_obj_find(const struct canopen_eds* eds,
				   int index, int subindex);

void eds_obj_dump(const struct canopen_eds* eds);

#endif /* CANOPEN_EDS_H_ */
