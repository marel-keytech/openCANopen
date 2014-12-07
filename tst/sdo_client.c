#include <linux/can.h>

#include "tst.h"
#include "canopen/sdo.h"
#include "canopen/sdo_client.h"

int test_foobar()
{
	return 0;
}

int main()
{
	int r = 0;
	RUN_TEST(test_foobar);
	return 0;
}

