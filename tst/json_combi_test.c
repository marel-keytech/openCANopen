#include "tst.h"
#include "json_combi.h"

int test_simple_string()
{
	char* json = json_root(
		json_str("foo", "bar")
	);
	ASSERT_STR_EQ("{\"foo\"=\"bar\"}", json);
	free(json);
	return 0;
}

int test_two_strings()
{
	char* json = json_root(
		json_str("foo", "bar"),
		json_str("bar", "foo")
	);
	ASSERT_STR_EQ("{\"foo\"=\"bar\",\"bar\"=\"foo\"}", json);
	free(json);
	return 0;
}

int test_simple_object()
{
	char* json = json_root(
		json_obj("my object",
			json_str("foo", "bar")
		)
	);
	ASSERT_STR_EQ("{\"my object\"={\"foo\"=\"bar\"}}", json);
	free(json);
	return 0;
}

int test_simple_integer()
{
	char* json = json_root(
		json_int("answer", 42)
	);
	ASSERT_STR_EQ("{\"answer\"=42}", json);
	free(json);
	return 0;
}

int test_simple_unsigned()
{
	char* json = json_root(
		json_uint("answer", 42)
	);
	ASSERT_STR_EQ("{\"answer\"=42}", json);
	free(json);
	return 0;
}

int test_simple_real()
{
	char* json = json_root(
		json_real("pi", 3.141592)
	);
	ASSERT_STR_EQ("{\"pi\"=3.141592e+00}", json);
	free(json);
	return 0;
}

int main()
{
	int r = 0;
	RUN_TEST(test_simple_string);
	RUN_TEST(test_two_strings);
	RUN_TEST(test_simple_object);
	RUN_TEST(test_simple_integer);
	RUN_TEST(test_simple_unsigned);
	RUN_TEST(test_simple_real);
	return r;
}

