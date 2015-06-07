#include "tst.h"
#include <unistd.h>
#include "worker.h"

static int rxfd;
static int txfd;

static void simple(void* context)
{
	int i = (int)context;
	write(txfd, &i, sizeof(i));
}

static void sleep_job(void* context)
{
	(void)context;
	usleep(1000);
}

static int test_do_3_jobs()
{
	int pipefds[2];

	pipe(pipefds);
	rxfd = pipefds[0];
	txfd = pipefds[1];

	worker_init(4, 16, 0);

	worker_add_job(0, sleep_job, NULL);
	worker_add_job(1, simple, (void*)1);
	worker_add_job(2, simple, (void*)2);
	worker_add_job(3, simple, (void*)3);
	worker_add_job(0, sleep_job, NULL);
	worker_add_job(4, simple, (void*)4);
	worker_add_job(5, simple, (void*)5);
	worker_add_job(6, simple, (void*)6);
	worker_add_job(0, sleep_job, NULL);
	worker_add_job(7, simple, (void*)7);
	worker_add_job(8, simple, (void*)8);
	worker_add_job(9, simple, (void*)9);

	int r1, r2, r3, r4, r5, r6, r7, r8, r9;
	read(rxfd, &r1, sizeof(int));
	read(rxfd, &r2, sizeof(int));
	read(rxfd, &r3, sizeof(int));
	read(rxfd, &r4, sizeof(int));
	read(rxfd, &r5, sizeof(int));
	read(rxfd, &r6, sizeof(int));
	read(rxfd, &r7, sizeof(int));
	read(rxfd, &r8, sizeof(int));
	read(rxfd, &r9, sizeof(int));

	ASSERT_INT_EQ(1, r1);
	ASSERT_INT_EQ(2, r2);
	ASSERT_INT_EQ(3, r3);
	ASSERT_INT_EQ(4, r4);
	ASSERT_INT_EQ(5, r5);
	ASSERT_INT_EQ(6, r6);
	ASSERT_INT_EQ(7, r7);
	ASSERT_INT_EQ(8, r8);
	ASSERT_INT_EQ(9, r9);

	worker_deinit();
	return 0;
}

int main()
{
	int r = 0;
	RUN_TEST(test_do_3_jobs);
	return r;
}

