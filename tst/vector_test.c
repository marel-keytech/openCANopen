#include "tst.h"
#include "vector.h"

static int test_init_destroy()
{
	struct vector vector;
	ASSERT_INT_EQ(0, vector_init(&vector, 42));
	ASSERT_UINT_EQ(0, vector.index);
	ASSERT_UINT_EQ(42, vector.size);
	ASSERT_TRUE(vector.data);
	vector_destroy(&vector);
	return 0;
}

static int test_reserve_less()
{
	struct vector vector;
	vector_init(&vector, 42);
	ASSERT_UINT_EQ(42, vector.size);
	vector_reserve(&vector, 5);
	ASSERT_UINT_EQ(42, vector.size);
	vector_reserve(&vector, 42);
	ASSERT_UINT_EQ(42, vector.size);
	vector_destroy(&vector);
	return 0;
}

static int test_reserve_more()
{
	struct vector vector;
	vector_init(&vector, 42);
	ASSERT_UINT_EQ(42, vector.size);
	vector_reserve(&vector, 43);
	ASSERT_UINT_GE(43, vector.size);
	vector_destroy(&vector);
	return 0;
}

static int test_append_nothing()
{
	struct vector vector;
	vector_init(&vector, 42);
	vector_append(&vector, "", 0);
	ASSERT_UINT_EQ(0, vector.index);
	ASSERT_UINT_EQ(42, vector.size);
	vector_destroy(&vector);
	return 0;
}

static int test_append_something()
{
	struct vector vector;
	vector_init(&vector, 42);
	vector_append(&vector, "a", 1);
	ASSERT_UINT_EQ(1, vector.index);
	ASSERT_UINT_EQ(42, vector.size);
	ASSERT_INT_EQ('a', *(char*)vector.data);
	vector_destroy(&vector);
	return 0;
}

static int test_vector_clear()
{
	struct vector vector;
	vector.index = 42;
	vector_clear(&vector);
	ASSERT_UINT_EQ(0, vector.index);
	return 0;
}

static int test_vector_assign_once()
{
	struct vector vector;
	vector_init(&vector, 1);
	char* str = "foobar";
	size_t size = strlen(str) + 1;
	vector_assign(&vector, str, size);
	ASSERT_UINT_GE(size, vector.size);
	ASSERT_UINT_EQ(size, vector.index);
	ASSERT_STR_EQ("foobar", vector.data);
	vector_destroy(&vector);
	return 0;
}

static int test_vector_assign_twice()
{
	struct vector vector;
	vector_init(&vector, 1);
	char* str1 = "foo";
	char* str2 = "bar";
	size_t size1 = strlen(str1) + 1;
	size_t size2 = strlen(str2) + 1;
	vector_assign(&vector, str1, size1);
	ASSERT_STR_EQ("foo", vector.data);
	ASSERT_INT_EQ(4, vector.index);
	vector_assign(&vector, str2, size2);
	ASSERT_STR_EQ("bar", vector.data);
	ASSERT_INT_EQ(4, vector.index);
	vector_destroy(&vector);
	return 0;
}

static int test_vector_fill()
{
	struct vector vector;
	vector_init(&vector, 1);
	vector_fill(&vector, 0, 4);
	vector_fill(&vector, 'a', 3);
	ASSERT_STR_EQ("aaa", vector.data);
	ASSERT_INT_EQ(3, vector.index);
	vector_destroy(&vector);
	return 0;
}

int main()
{
	int r = 0;
	RUN_TEST(test_init_destroy);
	RUN_TEST(test_reserve_less);
	RUN_TEST(test_reserve_more);
	RUN_TEST(test_append_nothing);
	RUN_TEST(test_append_something);
	RUN_TEST(test_vector_clear);
	RUN_TEST(test_vector_assign_once);
	RUN_TEST(test_vector_assign_twice);
	RUN_TEST(test_vector_fill);
	return r;
}
