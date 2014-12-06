#include <stdlib.h>
#include <string.h>

#ifdef __powerpc__
static void fix_endianness(void* dst, const void* src, size_t size)
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
static inline void fix_endianness(void* dst, const void* src, size_t size)
{
    memcpy(dst, src, size);
}
#endif
