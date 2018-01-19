#include "tst.h"
#include "trace-buffer.h"

#include "socketcan.h"

#include <stdlib.h>

int test_incomplete_buffer(void)
{
	struct tracebuffer tb;

	ASSERT_INT_GE(0, tb_init(&tb, 3 * sizeof(struct can_frame)));
	ASSERT_INT_EQ(4, tb.length);

	struct can_frame cf = { 0 };
	cf.can_id = 1;
	tb_append(&tb, &cf);
	cf.can_id = 2;
	tb_append(&tb, &cf);

	struct can_frame* buffer;
	size_t size;
	FILE* stream = open_memstream((char**)&buffer, &size);

	tb_dump(&tb, stream);

	ASSERT_INT_EQ(1, buffer[0].can_id);
	ASSERT_INT_EQ(2, buffer[1].can_id);

	fclose(stream);
	free(buffer);
	tb_destroy(&tb);
	return 0;
}

int test_full_buffer(void)
{
	struct tracebuffer tb;

	ASSERT_INT_GE(0, tb_init(&tb, 3 * sizeof(struct can_frame)));
	ASSERT_INT_EQ(4, tb.length);

	struct can_frame cf = { 0 };

	for (int i = 0; i < 6; ++i)
		tb_append(&tb, &cf);

	for (int i = 0; i < 4; ++i) {
		cf.can_id = i + 1;
		tb_append(&tb, &cf);
	}

	struct can_frame* buffer;
	size_t size;
	FILE* stream = open_memstream((char**)&buffer, &size);

	tb_dump(&tb, stream);

	ASSERT_INT_EQ(1, buffer[0].can_id);
	ASSERT_INT_EQ(2, buffer[1].can_id);
	ASSERT_INT_EQ(3, buffer[2].can_id);

	fclose(stream);
	free(buffer);
	tb_destroy(&tb);
	return 0;
}

int main()
{
	int r = 0;
	RUN_TEST(test_incomplete_buffer);
	RUN_TEST(test_full_buffer);
	return r;
}
