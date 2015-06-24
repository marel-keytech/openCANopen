#include "tst.h"

#define SDO_FIFO_LENGTH 3
#include "sdo_fifo.h"

static int test_enqueue()
{
	struct sdo_fifo fifo;
	sdo_fifo_init(&fifo);

	ASSERT_INT_EQ(0, sdo_fifo_length(&fifo));

	ASSERT_INT_EQ(0, sdo_fifo_enqueue(&fifo, SDO_ELEM_UL, 0x1000, 0, NULL));
	ASSERT_INT_EQ(1, sdo_fifo_length(&fifo));
	ASSERT_INT_EQ(0x1000, sdo_fifo_head(&fifo)->index);
	ASSERT_INT_EQ(0, sdo_fifo_head(&fifo)->subindex);

	ASSERT_INT_EQ(0, sdo_fifo_enqueue(&fifo, SDO_ELEM_UL, 0x2000, 1, NULL));
	ASSERT_INT_EQ(2, sdo_fifo_length(&fifo));
	ASSERT_INT_EQ(0x1000, sdo_fifo_head(&fifo)->index);
	ASSERT_INT_EQ(0, sdo_fifo_head(&fifo)->subindex);

	ASSERT_INT_EQ(0, sdo_fifo_enqueue(&fifo, SDO_ELEM_UL, 0x3000, 2, NULL));
	ASSERT_INT_EQ(3, sdo_fifo_length(&fifo));
	ASSERT_INT_EQ(0x1000, sdo_fifo_head(&fifo)->index);
	ASSERT_INT_EQ(0, sdo_fifo_head(&fifo)->subindex);

	ASSERT_INT_EQ(-1, sdo_fifo_enqueue(&fifo, SDO_ELEM_UL, 0x4000, 3, NULL));
	ASSERT_INT_EQ(3, sdo_fifo_length(&fifo));
	ASSERT_INT_EQ(0x1000, sdo_fifo_head(&fifo)->index);
	ASSERT_INT_EQ(0, sdo_fifo_head(&fifo)->subindex);

	return 0;
}

static int test_dequeue()
{
	struct sdo_fifo fifo;
	sdo_fifo_init(&fifo);

	ASSERT_INT_EQ(-1, sdo_fifo_dequeue(&fifo));

	sdo_fifo_enqueue(&fifo, SDO_ELEM_UL, 0x1000, 1, NULL);
	sdo_fifo_enqueue(&fifo, SDO_ELEM_UL, 0x2000, 2, NULL);
	sdo_fifo_enqueue(&fifo, SDO_ELEM_UL, 0x3000, 3, NULL);

	ASSERT_INT_EQ(3, sdo_fifo_length(&fifo));
	ASSERT_INT_EQ(0x1000, sdo_fifo_head(&fifo)->index);
	ASSERT_INT_EQ(1, sdo_fifo_head(&fifo)->subindex);

	ASSERT_INT_EQ(0, sdo_fifo_dequeue(&fifo));
	ASSERT_INT_EQ(2, sdo_fifo_length(&fifo));
	ASSERT_INT_EQ(0x2000, sdo_fifo_head(&fifo)->index);
	ASSERT_INT_EQ(2, sdo_fifo_head(&fifo)->subindex);

	ASSERT_INT_EQ(0, sdo_fifo_dequeue(&fifo));
	ASSERT_INT_EQ(1, sdo_fifo_length(&fifo));
	ASSERT_INT_EQ(0x3000, sdo_fifo_head(&fifo)->index);
	ASSERT_INT_EQ(3, sdo_fifo_head(&fifo)->subindex);

	ASSERT_INT_EQ(0, sdo_fifo_dequeue(&fifo));
	ASSERT_INT_EQ(0, sdo_fifo_length(&fifo));

	ASSERT_INT_EQ(-1, sdo_fifo_dequeue(&fifo));

	return 0;
}

static int test_enqueue_dequeue()
{
	struct sdo_fifo fifo;
	sdo_fifo_init(&fifo);

	sdo_fifo_enqueue(&fifo, SDO_ELEM_UL, 0x1000, 1, NULL);
	sdo_fifo_enqueue(&fifo, SDO_ELEM_UL, 0x2000, 2, NULL);
	sdo_fifo_dequeue(&fifo);
	sdo_fifo_enqueue(&fifo, SDO_ELEM_UL, 0x3000, 3, NULL);
	sdo_fifo_enqueue(&fifo, SDO_ELEM_UL, 0x4000, 4, NULL);

	ASSERT_INT_EQ(3, sdo_fifo_length(&fifo));
	ASSERT_INT_EQ(0x2000, sdo_fifo_head(&fifo)->index);
	ASSERT_INT_EQ(2, sdo_fifo_head(&fifo)->subindex);

	ASSERT_INT_EQ(0, sdo_fifo_dequeue(&fifo));
	ASSERT_INT_EQ(2, sdo_fifo_length(&fifo));
	ASSERT_INT_EQ(0x3000, sdo_fifo_head(&fifo)->index);
	ASSERT_INT_EQ(3, sdo_fifo_head(&fifo)->subindex);

	ASSERT_INT_EQ(0, sdo_fifo_dequeue(&fifo));
	ASSERT_INT_EQ(1, sdo_fifo_length(&fifo));
	ASSERT_INT_EQ(0x4000, sdo_fifo_head(&fifo)->index);
	ASSERT_INT_EQ(4, sdo_fifo_head(&fifo)->subindex);

	ASSERT_INT_EQ(0, sdo_fifo_dequeue(&fifo));
	ASSERT_INT_EQ(0, sdo_fifo_length(&fifo));

	return 0;
}

int main()
{
	int r = 0;
	RUN_TEST(test_enqueue);
	RUN_TEST(test_dequeue);
	RUN_TEST(test_enqueue_dequeue);
	return r;
}
