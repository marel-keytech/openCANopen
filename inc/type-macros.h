#ifndef TYPE_MACROS_H_
#define TYPE_MACROS_H_

#include <stddef.h>

#define container_of(ptr, type, member) \
({ \
	const typeof( ((type *)0)->member ) *__mptr = (ptr); \
	(type *)( (char *)__mptr - offsetof(type,member) ); \
})

#endif /* TYPE_MACROS_H_ */
