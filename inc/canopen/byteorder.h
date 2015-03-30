#ifndef _CANOPEN_BYTEORDER_H
#define _CANOPEN_BYTEORDER_H

#include <stdlib.h>

void byteorder(void* dst, const void* src, size_t size);

#define BYTEORDER(dst, value) \
({ \
	__typeof__(value) _value = value; \
	byteorder(dst, &_value, sizeof(_value)); \
})


#endif /* _CANOPEN_BYTEORDER_H */

