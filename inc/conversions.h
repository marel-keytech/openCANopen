#ifndef CONVERSIONS_H_
#define CONVERSIONS_H_

#include <stdint.h>
#include "canopen/types.h"

struct canopen_data {
	enum canopen_type type;
	void* data;
	size_t size;
	uint64_t value;
};

char* canopen_data_tostring(char* dst, size_t size, struct canopen_data* src);
int canopen_data_fromstring(struct canopen_data* dst,
			    enum canopen_type expected_type, char* str);

#endif /* CONVERSIONS_H_ */
