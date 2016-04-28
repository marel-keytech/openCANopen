#include "tst.h"
#include "conversions.h"

static int test_bool_tostring()
{
	char buf[256];
	char value = 0;
	struct canopen_data data = {
		.type = CANOPEN_BOOLEAN,
		.data = &value,
		.size = 1
	};
	ASSERT_STR_EQ("false", canopen_data_tostring(buf, sizeof(buf), &data));
	value = 1;
	ASSERT_STR_EQ("true", canopen_data_tostring(buf, sizeof(buf), &data));
	return 0;
}

static int test_bool_tostring__with_unknown_size()
{
	char buf[256];
	char value = 0;
	struct canopen_data data = {
		.type = CANOPEN_BOOLEAN,
		.data = &value,
		.size = 4,
		.is_size_unknown = 1
	};
	ASSERT_STR_EQ("false", canopen_data_tostring(buf, sizeof(buf), &data));
	value = 1;
	ASSERT_STR_EQ("true", canopen_data_tostring(buf, sizeof(buf), &data));
	return 0;
}

static int test_bool_tostring__too_long()
{
	char buf[256];
	char value = 0;
	struct canopen_data data = {
		.type = CANOPEN_BOOLEAN,
		.data = &value,
		.size = 4
	};
	ASSERT_PTR_EQ(NULL, canopen_data_tostring(buf, sizeof(buf), &data));
	return 0;
}

static int test_uint_tostring()
{
	char buf[256];
	uint32_t value = 42;
	struct canopen_data data = {
		.type = CANOPEN_UNSIGNED32,
		.data = &value,
		.size = sizeof(value)
	};
	ASSERT_STR_EQ("42", canopen_data_tostring(buf, sizeof(buf), &data));
	return 0;
}

static int test_negative_int_tostring()
{
	char buf[256];
	int32_t value = -42;
	struct canopen_data data = {
		.type = CANOPEN_INTEGER32,
		.data = &value,
		.size = sizeof(value)
	};
	ASSERT_STR_EQ("-42", canopen_data_tostring(buf, sizeof(buf), &data));
	return 0;
}

static int test_positive_int_tostring()
{
	char buf[256];
	int32_t value = 42;
	struct canopen_data data = {
		.type = CANOPEN_INTEGER32,
		.data = &value,
		.size = sizeof(value)
	};
	ASSERT_STR_EQ("42", canopen_data_tostring(buf, sizeof(buf), &data));
	return 0;
}

static int test_negative_int64_tostring()
{
	char buf[256];
	int64_t value = -42;
	struct canopen_data data = {
		.type = CANOPEN_INTEGER64,
		.data = &value,
		.size = sizeof(value)
	};
	ASSERT_STR_EQ("-42", canopen_data_tostring(buf, sizeof(buf), &data));
	return 0;
}

static int test_positive_int64_tostring()
{
	char buf[256];
	int64_t value = 42;
	struct canopen_data data = {
		.type = CANOPEN_INTEGER64,
		.data = &value,
		.size = sizeof(value)
	};
	ASSERT_STR_EQ("42", canopen_data_tostring(buf, sizeof(buf), &data));
	return 0;
}

static int test_float_tostring()
{
	char buf[256];
	float value = 3.14159265;
	struct canopen_data data = {
		.type = CANOPEN_REAL32,
		.data = &value,
		.size = sizeof(value)
	};
	ASSERT_STR_EQ("3.141593e+00",
		      canopen_data_tostring(buf, sizeof(buf), &data));
	return 0;
}

static int test_double_tostring()
{
	char buf[256];
	double value = 3.14159265;
	struct canopen_data data = {
		.type = CANOPEN_REAL64,
		.data = &value,
		.size = sizeof(value)
	};
	ASSERT_STR_EQ("3.141593e+00",
		      canopen_data_tostring(buf, sizeof(buf), &data));
	return 0;
}

static int test_string_tostring()
{
	char buf[256];
	char* value = "foobarXXX";
	struct canopen_data data = {
		.type = CANOPEN_VISIBLE_STRING,
		.data = value,
		.size = 6
	};
	ASSERT_STR_EQ("foobar",
		      canopen_data_tostring(buf, sizeof(buf), &data));
	return 0;
}

static int test_bool_fromstring()
{
	struct canopen_data data;
	ASSERT_INT_EQ(0, canopen_data_fromstring(&data, CANOPEN_BOOLEAN,
							"true"));
	ASSERT_TRUE(*(char*)data.data);
	ASSERT_INT_EQ(0, canopen_data_fromstring(&data, CANOPEN_BOOLEAN,
							"false"));
	ASSERT_FALSE(*(char*)data.data);
	ASSERT_INT_LT(0, canopen_data_fromstring(&data, CANOPEN_BOOLEAN,
							"whatevs"));
	return 0;
}

static int test_uint_fromstring()
{
	struct canopen_data data;

	ASSERT_INT_EQ(0, canopen_data_fromstring(&data, CANOPEN_UNSIGNED32,
							"42"));
	ASSERT_INT_EQ(42, *(uint32_t*)data.data);

	ASSERT_INT_LT(0, canopen_data_fromstring(&data, CANOPEN_UNSIGNED32,
							"-42"));
	return 0;
}

static int test_int_fromstring()
{
	struct canopen_data data;

	ASSERT_INT_EQ(0, canopen_data_fromstring(&data, CANOPEN_INTEGER32,
							"-42"));
	ASSERT_INT_EQ(-42, *(int32_t*)data.data);

	ASSERT_INT_LT(0, canopen_data_fromstring(&data, CANOPEN_INTEGER32,
							"foobar"));
	return 0;
}

static int test_float_fromstring()
{
	struct canopen_data data;

	ASSERT_INT_EQ(0, canopen_data_fromstring(&data, CANOPEN_REAL32,
							"3.14"));
	ASSERT_DOUBLE_GT(3.13, *(float*)data.data);
	ASSERT_DOUBLE_LT(3.15, *(float*)data.data);

	ASSERT_INT_LT(0, canopen_data_fromstring(&data, CANOPEN_REAL32, "meh"));
	return 0;
}

static int test_double_fromstring()
{
	struct canopen_data data;

	ASSERT_INT_EQ(0, canopen_data_fromstring(&data, CANOPEN_REAL64,
							"3.14"));
	ASSERT_DOUBLE_GT(3.13, *(double*)data.data);
	ASSERT_DOUBLE_LT(3.15, *(double*)data.data);

	ASSERT_INT_LT(0, canopen_data_fromstring(&data, CANOPEN_REAL64, "meh"));
	return 0;
}

int main()
{
	int r = 0;
	RUN_TEST(test_bool_tostring);
	RUN_TEST(test_bool_tostring__with_unknown_size);
	RUN_TEST(test_bool_tostring__too_long);
	RUN_TEST(test_uint_tostring);
	RUN_TEST(test_negative_int_tostring);
	RUN_TEST(test_positive_int_tostring);
	RUN_TEST(test_negative_int64_tostring);
	RUN_TEST(test_positive_int64_tostring);
	RUN_TEST(test_float_tostring);
	RUN_TEST(test_double_tostring);
	RUN_TEST(test_string_tostring);

	RUN_TEST(test_bool_fromstring);
	RUN_TEST(test_uint_fromstring);
	RUN_TEST(test_int_fromstring);
	RUN_TEST(test_float_fromstring);
	RUN_TEST(test_double_fromstring);
	return r;
}
