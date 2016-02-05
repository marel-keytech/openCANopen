#include <unistd.h>
#include <stdint.h>

static inline char nibble_to_char(int nibble)
{
	static char map[] = {
		'0', '1', '2', '3',
		'4', '5', '6', '7',
		'8', '9', 'A', 'B',
		'C', 'D', 'E', 'F'
	};

	return map[nibble];
}

static inline uint8_t get_low_nibble(uint8_t data)
{
	return data & 0x0f;
}

static inline uint8_t get_high_nibble(uint8_t data)
{
	return (data & 0xf0) >> 4;
}

const char* hexdump(const void* data, size_t size)
{
	const char* p = data;
	static __thread char buffer[256];

	size_t k = 0;
	for(size_t i = 0; i < size && k + 2 < sizeof(buffer); ++i) {
		buffer[k++] = nibble_to_char(get_high_nibble(p[i]));
		buffer[k++] = nibble_to_char(get_low_nibble(p[i]));
	}
	buffer[k] = '\0';

	return buffer;
}
