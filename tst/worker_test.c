#include "tst.h"
#include <unistd.h>
#include "worker.h"

static int results[3] = { 0 };

static void simple(void* context)
{
	int i = (int)context;
	results[i-1] = i;
}

static int test_do_3_jobs()
{
	worker_init(1, 16, 0);

	worker_add_job(1, simple, (void*)1);
	worker_add_job(2, simple, (void*)2);
	worker_add_job(3, simple, (void*)3);

	sleep(1);

	ASSERT_INT_EQ(1, results[0]);
	ASSERT_INT_EQ(2, results[1]);
	ASSERT_INT_EQ(3, results[2]);

	worker_deinit();
	return 0;
}

int main()
{
	int r = 0;
	RUN_TEST(test_do_3_jobs);
	return r;
}
