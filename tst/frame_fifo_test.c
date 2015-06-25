#include <errno.h>
#include "tst.h"

#include "frame_fifo.h"

static int test_enqueue()
{
	struct can_frame cf = { 0 };
	struct frame_fifo fifo;
	frame_fifo_init(&fifo);

	ASSERT_INT_EQ(0, frame_fifo_length(&fifo));

	cf.can_id = 1;
	frame_fifo_enqueue(&fifo, &cf);
	ASSERT_INT_EQ(1, frame_fifo_length(&fifo));
	ASSERT_INT_EQ(1, fifo.frame[0].can_id);

	cf.can_id = 2;
	frame_fifo_enqueue(&fifo, &cf);
	ASSERT_INT_EQ(2, frame_fifo_length(&fifo));
	ASSERT_INT_EQ(1, fifo.frame[0].can_id);
	ASSERT_INT_EQ(2, fifo.frame[1].can_id);

	cf.can_id = 3;
	frame_fifo_enqueue(&fifo, &cf);
	ASSERT_INT_EQ(2, frame_fifo_length(&fifo));
	ASSERT_INT_EQ(2, fifo.frame[1].can_id);
	ASSERT_INT_EQ(3, fifo.frame[0].can_id);

	cf.can_id = 4;
	frame_fifo_enqueue(&fifo, &cf);
	ASSERT_INT_EQ(2, frame_fifo_length(&fifo));
	ASSERT_INT_EQ(3, fifo.frame[0].can_id);
	ASSERT_INT_EQ(4, fifo.frame[1].can_id);

	frame_fifo_clear(&fifo);
	return 0;
}

static int test_dequeue()
{
	struct can_frame in = { 0 };
	struct can_frame out = { 0 };
	struct frame_fifo fifo;
	frame_fifo_init(&fifo);

	in.can_id = 42;
	frame_fifo_enqueue(&fifo, &in);
	ASSERT_INT_EQ(1, frame_fifo_length(&fifo));
	ASSERT_INT_EQ(1, frame_fifo_dequeue(&fifo, &out, 0));
	ASSERT_INT_EQ(0, frame_fifo_length(&fifo));
	ASSERT_INT_EQ(42, out.can_id);

	errno = 0;
	ASSERT_INT_EQ(-1, frame_fifo_dequeue(&fifo, &out, 0));
	ASSERT_INT_EQ(EAGAIN, errno);

	in.can_id = 23;
	frame_fifo_enqueue(&fifo, &in);

	in.can_id = 7;
	frame_fifo_enqueue(&fifo, &in);

	ASSERT_INT_EQ(2, frame_fifo_length(&fifo));
	ASSERT_INT_EQ(1, frame_fifo_dequeue(&fifo, &out, 0));
	ASSERT_INT_EQ(1, frame_fifo_length(&fifo));
	ASSERT_INT_EQ(23, out.can_id);

	ASSERT_INT_EQ(1, frame_fifo_dequeue(&fifo, &out, 0));
	ASSERT_INT_EQ(0, frame_fifo_length(&fifo));
	ASSERT_INT_EQ(7, out.can_id);

	errno = 0;
	ASSERT_INT_EQ(-1, frame_fifo_dequeue(&fifo, &out, 0));
	ASSERT_INT_EQ(EAGAIN, errno);

	frame_fifo_clear(&fifo);
	return 0;
}


int main()
{
	int r = 0;
	RUN_TEST(test_enqueue);
	RUN_TEST(test_dequeue);
	return r;
}
