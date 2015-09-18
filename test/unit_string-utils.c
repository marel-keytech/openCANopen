#include <string.h>
#include "string-utils.h"
#include "tst.h"

static char* string(const char* str)
{
	static char buffer[256];
	strncpy(buffer, str, sizeof(buffer));
	return buffer;
}

static int test_trim_left()
{
	ASSERT_STR_EQ("foo bar", string_trim_left(string("foo bar")));
	ASSERT_STR_EQ("foo bar", string_trim_left(string("\t foo bar")));
	ASSERT_STR_EQ("foo bar ", string_trim_left(string("\t foo bar ")));
	return 0;
}

static int test_trim_right()
{
	ASSERT_STR_EQ("foo bar", string_trim_right(string("foo bar")));
	ASSERT_STR_EQ("foo bar", string_trim_right(string("foo bar \t")));
	ASSERT_STR_EQ(" foo bar", string_trim_right(string(" foo bar \t")));
	return 0;
}

static int test_trim()
{
	ASSERT_STR_EQ("foo bar", string_trim(string("foo bar")));
	ASSERT_STR_EQ("foo bar", string_trim(string("foo bar \t")));
	ASSERT_STR_EQ("foo bar", string_trim(string(" \tfoo bar \t")));
	return 0;
}

static int test_is_empty()
{
	ASSERT_TRUE(string_is_empty(""));
	ASSERT_FALSE(string_is_empty("asdf"));
	return 0;
}

static int test_keep_if_alpha()
{
	ASSERT_STR_EQ("foobar", string_keep_if(isalpha, string(".f.o.o.b.a.r.")));
	ASSERT_STR_EQ("", string_keep_if(isalpha, string("12345")));
	return 0;
}

static int test_string_begins_with()
{
	ASSERT_FALSE(string_begins_with("foo", ""));
	ASSERT_FALSE(string_begins_with("foo", "f"));
	ASSERT_FALSE(string_begins_with("foo", "fo"));
	ASSERT_TRUE(string_begins_with("foo", "foo"));
	ASSERT_TRUE(string_begins_with("foo", "foobar"));
	return 0;
}

static int test_string_ends_with()
{
	ASSERT_FALSE(string_ends_with("bar", "foo"));
	ASSERT_FALSE(string_ends_with("bar", "foor"));
	ASSERT_FALSE(string_ends_with("bar", "fooar"));
	ASSERT_TRUE(string_ends_with("bar", "foobar"));
	ASSERT_TRUE(string_ends_with("bar", "bar"));
	ASSERT_TRUE(string_ends_with("r", "foobar"));
	return 0;
}

int main()
{
	int r = 0;
	RUN_TEST(test_trim_left);
	RUN_TEST(test_trim_right);
	RUN_TEST(test_trim);
	RUN_TEST(test_is_empty);
	RUN_TEST(test_keep_if_alpha);
	RUN_TEST(test_string_begins_with);
	RUN_TEST(test_string_ends_with);
	return r;
}
