#include "prioq.h"
#include "tst.h"

/*
static void dump_array(struct prioq* q)
{
	for (int i = 0; i < q->index; ++i)
		printf("%lu ", q->head[i].priority);

	printf("\n");
}
*/

static int test_prioq_init_clear()
{
	struct prioq q;
	ASSERT_INT_EQ(0, prioq_init(&q, 42));
	prioq_clear(&q);
	return 0;
}

static int test_prioq_init_grow_clear()
{
	struct prioq q;
	ASSERT_INT_EQ(0, prioq_init(&q, 42));
	ASSERT_INT_GT(0, prioq_grow(&q, 64));
	prioq_clear(&q);
	return 0;
}

static int test_prioq_init_grow_down_clear()
{
	struct prioq q;
	ASSERT_INT_EQ(0, prioq_init(&q, 42));
	ASSERT_INT_EQ(0, prioq_grow(&q, 3));
	prioq_clear(&q);
	return 0;
}

static int test_prioq_parent()
{
	ASSERT_INT_EQ(0, _prioq_parent(1));
	ASSERT_INT_EQ(0, _prioq_parent(2));

	ASSERT_INT_EQ(1, _prioq_parent(3));
	ASSERT_INT_EQ(1, _prioq_parent(4));

	ASSERT_INT_EQ(2, _prioq_parent(5));
	ASSERT_INT_EQ(2, _prioq_parent(6));

	ASSERT_INT_EQ(3, _prioq_parent(7));
	ASSERT_INT_EQ(3, _prioq_parent(8));

	ASSERT_INT_EQ(4, _prioq_parent(9));
	ASSERT_INT_EQ(4, _prioq_parent(10));
	return 0;
}

static int test_prioq_lchild()
{
	ASSERT_INT_EQ(1, _prioq_lchild(0));
	ASSERT_INT_EQ(3, _prioq_lchild(1));
	ASSERT_INT_EQ(5, _prioq_lchild(2));
	return 0;
}

static int test_prioq_rchild()
{
	ASSERT_INT_EQ(2, _prioq_rchild(0));
	ASSERT_INT_EQ(4, _prioq_rchild(1));
	ASSERT_INT_EQ(6, _prioq_rchild(2));
	return 0;
}

static int test_prioq_swap()
{
	struct prioq_elem a, b;

	a.priority = 1;
	a.data = (void*)'a';
	b.priority = 2;
	b.data = (void*)'b';

	_prioq_swap(&a, &b);

	ASSERT_INT_EQ(2, a.priority);
	ASSERT_INT_EQ(1, b.priority);
	ASSERT_INT_EQ('b', (char)a.data);
	ASSERT_INT_EQ('a', (char)b.data);

	return 0;
}

static int test_bubble_up_head()
{
	struct prioq q;
	prioq_init(&q, 2);

	q.index = 1;
	_prioq_bubble_up(&q, 0);

	prioq_clear(&q);
	return 0;
}

static int test_sink_down_right()
{
	struct prioq q;
	prioq_init(&q, 3);

	q.index = 3;
	q.head[0].priority = 2;
	q.head[1].priority = 1;
	q.head[2].priority = 0;

	_prioq_sink_down(&q, 0);

	ASSERT_INT_EQ(0, q.head[0].priority);
	ASSERT_INT_EQ(1, q.head[1].priority);
	ASSERT_INT_EQ(2, q.head[2].priority);

	prioq_clear(&q);
	return 0;
}

static int test_sink_down_left()
{
	struct prioq q;
	prioq_init(&q, 3);

	q.index = 2;
	q.head[0].priority = 1;
	q.head[1].priority = 0;
	q.head[2].priority = 2;

	_prioq_sink_down(&q, 0);

	ASSERT_INT_EQ(0, q.head[0].priority);
	ASSERT_INT_EQ(1, q.head[1].priority);
	ASSERT_INT_EQ(2, q.head[2].priority);

	prioq_clear(&q);
	return 0;
}

static int test_get_smaller_child_left()
{
	struct prioq q;
	prioq_init(&q, 3);

	q.index = 3;
	q.head[0].priority = 1;
	q.head[1].priority = 0;
	q.head[2].priority = 2;

	ASSERT_INT_EQ(1, _prioq_get_smaller_child(&q, 0));

	prioq_clear(&q);
	return 0;
}

static int test_get_smaller_child_right()
{
	struct prioq q;
	prioq_init(&q, 3);

	q.index = 3;
	q.head[0].priority = 1;
	q.head[1].priority = 2;
	q.head[2].priority = 0;

	ASSERT_INT_EQ(2, _prioq_get_smaller_child(&q, 0));

	prioq_clear(&q);
	return 0;
}

static void heapsort(unsigned long* dst, unsigned long* src, size_t size)
{
	struct prioq q;
	int i;

	prioq_init(&q, size);

	for(i = 0; i < size; ++i)
		prioq_insert(&q, src[i], NULL);

	for(i = 0; i < size; ++i)
	{
		dst[i] = q.head->priority;
		prioq_pop(&q);
	}

	prioq_clear(&q);
}

static int test_heapsort_wo_duplicates()
{
	unsigned long input[] = { 5, 9, 1, 8, 4, 0, 3, 7, 2, 6 };
	unsigned long output[10] = { 0 };

	heapsort(output, input, 10);

	ASSERT_INT_EQ(0, output[0]);
	ASSERT_INT_EQ(1, output[1]);
	ASSERT_INT_EQ(2, output[2]);
	ASSERT_INT_EQ(3, output[3]);
	ASSERT_INT_EQ(4, output[4]);
	ASSERT_INT_EQ(5, output[5]);
	ASSERT_INT_EQ(6, output[6]);
	ASSERT_INT_EQ(7, output[7]);
	ASSERT_INT_EQ(8, output[8]);
	ASSERT_INT_EQ(9, output[9]);

	return 0;
}

static int test_heapsort_w_duplicates()
{
	unsigned long input[] = { 5, 9, 1, 8, 4, 1, 3, 4, 2, 6 };
	unsigned long output[10] = { 0 };

	heapsort(output, input, 10);

	ASSERT_INT_EQ(1, output[0]);
	ASSERT_INT_EQ(1, output[1]);
	ASSERT_INT_EQ(2, output[2]);
	ASSERT_INT_EQ(3, output[3]);
	ASSERT_INT_EQ(4, output[4]);
	ASSERT_INT_EQ(4, output[5]);
	ASSERT_INT_EQ(5, output[6]);
	ASSERT_INT_EQ(6, output[7]);
	ASSERT_INT_EQ(8, output[8]);
	ASSERT_INT_EQ(9, output[9]);

	return 0;
}

int main()
{
	int r = 0;
	RUN_TEST(test_prioq_init_clear);
	RUN_TEST(test_prioq_init_grow_clear);
	RUN_TEST(test_prioq_init_grow_down_clear);
	RUN_TEST(test_prioq_parent);
	RUN_TEST(test_prioq_lchild);
	RUN_TEST(test_prioq_rchild);
	RUN_TEST(test_prioq_swap);
	RUN_TEST(test_bubble_up_head);
	RUN_TEST(test_sink_down_left);
	RUN_TEST(test_sink_down_right);
	RUN_TEST(test_get_smaller_child_left);
	RUN_TEST(test_get_smaller_child_right);
	RUN_TEST(test_heapsort_wo_duplicates);
	RUN_TEST(test_heapsort_w_duplicates);
	return r;
}

