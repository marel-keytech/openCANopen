#include "tst.h"

#include "canopen/types.h"

static int test_type_size(void)
{
	ASSERT_UINT_EQ(1, canopen_type_size(CANOPEN_BOOLEAN));
	ASSERT_UINT_EQ(1, canopen_type_size(CANOPEN_INTEGER8));
	ASSERT_UINT_EQ(2, canopen_type_size(CANOPEN_INTEGER16));
	ASSERT_UINT_EQ(3, canopen_type_size(CANOPEN_INTEGER24));
	ASSERT_UINT_EQ(4, canopen_type_size(CANOPEN_INTEGER32));
	ASSERT_UINT_EQ(5, canopen_type_size(CANOPEN_INTEGER40));
	ASSERT_UINT_EQ(6, canopen_type_size(CANOPEN_INTEGER48));
	ASSERT_UINT_EQ(7, canopen_type_size(CANOPEN_INTEGER56));
	ASSERT_UINT_EQ(8, canopen_type_size(CANOPEN_INTEGER64));
	return 0;
}

static int test_type_to_string(void)
{
	ASSERT_STR_EQ("REAL32", canopen_type_to_string(CANOPEN_REAL32));
	ASSERT_STR_EQ("BOOLEAN", canopen_type_to_string(CANOPEN_BOOLEAN));
	ASSERT_FALSE(canopen_type_to_string(1337));
	return 0;
}

static int test_type_from_string(void)
{
	ASSERT_UINT_EQ(CANOPEN_REAL32, canopen_type_from_string("REAL32"));
	ASSERT_UINT_EQ(CANOPEN_BOOLEAN, canopen_type_from_string("boolean"));
	ASSERT_UINT_EQ(CANOPEN_UNKNOWN, canopen_type_from_string("foobar"));
	return 0;
}

int main()
{
	int r = 0;
	RUN_TEST(test_type_size);
	RUN_TEST(test_type_to_string);
	RUN_TEST(test_type_from_string);
	return r;
}
