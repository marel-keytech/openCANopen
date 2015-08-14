#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "canopen/byteorder.h"

#ifdef __powerpc__

void byteorder(void* dst, const void* src, size_t size)
{
	uint8_t* d = (uint8_t*)dst;
	uint8_t* s = (uint8_t*)src;
	int i = (int)size;

	switch(size)
	{
	case 8: d[i - 1] = s[size - i]; --i;
		d[i - 1] = s[size - i]; --i;
		d[i - 1] = s[size - i]; --i;
		d[i - 1] = s[size - i]; --i;
	case 4: d[i - 1] = s[size - i]; --i;
		d[i - 1] = s[size - i]; --i;
	case 2: d[i - 1] = s[size - i]; --i;
	case 1: d[i - 1] = s[size - i];
		break;
	default:
		abort();
	}
}

void byteorder2(void* dst, const void* src, size_t dst_size, size_t src_size)
{
	uint8_t* d = (uint8_t*)dst;
	uint8_t* s = (uint8_t*)src;
	int d_i = dst_size;
	int s_i = 0;

	assert(dst_size >= src_size);

	switch(src_size)
	{
	case 8: d[--d_i] = s[s_i++];
	case 7:	d[--d_i] = s[s_i++];
	case 6:	d[--d_i] = s[s_i++];
	case 5:	d[--d_i] = s[s_i++];
	case 4: d[--d_i] = s[s_i++];
	case 3:	d[--d_i] = s[s_i++];
	case 2: d[--d_i] = s[s_i++];
	case 1: d[--d_i] = s[s_i++];
		break;
	default:
		abort();
	}
}

#else

void byteorder(void* dst, const void* src, size_t size)
{
	memcpy(dst, src, size);
}

void byteorder2(void* dst, const void* src, size_t dst_size, size_t src_size)
{
	assert(dst_size >= src_size);
	memcpy(dst, src, src_size);
}

#endif

