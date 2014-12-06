#include <stdlib.h>
#include <string.h>

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
    }
}
#else
void byteorder(void* dst, const void* src, size_t size)
{
    memcpy(dst, src, size);
}
#endif

