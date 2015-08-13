#include <stdlib.h>
#include "kv.h"
#include "tst.h"

static int simple_key_value()
{
	struct kv* kv = kv_new("foo", "bar");
	ASSERT_STR_EQ("foo", kv_key(kv));
	ASSERT_STR_EQ("bar", kv_value(kv));
	kv_del(kv);
	return 0;
}

static int empty_key()
{
	struct kv* kv = kv_new("", "bar");
	ASSERT_STR_EQ("", kv_key(kv));
	ASSERT_STR_EQ("bar", kv_value(kv));
	kv_del(kv);
	return 0;
}

static int empty_value()
{
	struct kv* kv = kv_new("foo", "");
	ASSERT_STR_EQ("foo", kv_key(kv));
	ASSERT_STR_EQ("", kv_value(kv));
	kv_del(kv);
	return 0;
}

static int do_copy()
{
	struct kv* cp;
	struct kv* kv = kv_new("foo", "bar");
	ASSERT_STR_EQ("foo", kv_key(kv));
	ASSERT_STR_EQ("bar", kv_value(kv));
	cp = kv_copy(kv);
	ASSERT_STR_EQ("foo", kv_key(cp));
	ASSERT_STR_EQ("bar", kv_value(cp));
	kv_del(cp);
	kv_del(kv);
	return 0;
}

int raw_copy()
{
	struct kv* kv = kv_new("foo", "bar");
	void* cp = malloc(kv_raw_size(kv));
	ASSERT_STR_EQ("foo", kv_key(kv));
	ASSERT_STR_EQ("bar", kv_value(kv));
	kv_raw_copy(cp, kv, kv_raw_size(kv));
	ASSERT_INT_EQ(0, memcmp(cp, kv, kv_raw_size(kv)));
	kv_del(cp);
	kv_del(kv);
	return 0;
}

int main()
{
	int r = 0;

	RUN_TEST(simple_key_value);
	RUN_TEST(empty_key);
	RUN_TEST(empty_value);
	RUN_TEST(do_copy);
	RUN_TEST(raw_copy);

	return r;
}
