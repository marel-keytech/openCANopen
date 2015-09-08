#ifndef ARC_H_
#define ARC_H_

#include "co_atomic.h"

#ifdef ARC_DEBUG
#include <stdio.h>
#define ARC_PRINT_DEBUG(name, type) \
	printf(ARC_STR(name) "_" type "(%p) = %d\n", self, ref);
#else
#define ARC_PRINT_DEBUG(...)
#endif

#define ARC_STR(x) #x

#define ARC_GENERATE_REF(name) \
int name ## _ref(struct name* self) \
{ \
	int ref = co_atomic_add_fetch(&self->ref, 1); \
	ARC_PRINT_DEBUG(name, "ref") \
	return ref; \
}

#define ARC_GENERATE_UNREF(name, free_fn) \
int name ## _unref(struct name* self) \
{ \
	int ref = co_atomic_sub_fetch(&self->ref, 1); \
	ARC_PRINT_DEBUG(name, "unref") \
	if (ref == 0) \
		free_fn(self); \
	return ref; \
}

#define ARC_GENERATE(name, free_fn) \
	ARC_GENERATE_REF(name) \
	ARC_GENERATE_UNREF(name, free_fn)

#define ARC_PROTOTYPE(name) \
int name ## _ref(struct name* self); \
int name ## _unref(struct name* self);

#endif /* ARC_H_ */
