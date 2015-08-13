#include <stdlib.h>

#include "kv.h"
#include "kv_dict.h"
#include "tst.h"

static int one_kv()
{
	struct kv* kv = kv_new("foo", "bar");
	struct kv_dict* dict = kv_dict_new();

	ASSERT_INT_GE(0, kv_dict_insert(dict, kv));
	ASSERT_STR_EQ("bar", kv_value(kv_dict_find(dict, "foo")));

	kv_dict_del(dict);
	kv_del(kv);
	return 0;
}

static int three_kv()
{
	struct kv* kv0 = kv_new("foo", "bar");
	struct kv* kv1 = kv_new("abc", "x");
	struct kv* kv2 = kv_new("123", "hmm");
	struct kv_dict* dict = kv_dict_new();

	ASSERT_INT_GE(0, kv_dict_insert(dict, kv0));
	ASSERT_INT_GE(0, kv_dict_insert(dict, kv1));
	ASSERT_INT_GE(0, kv_dict_insert(dict, kv2));

	ASSERT_STR_EQ("bar", kv_value(kv_dict_find(dict, "foo")));
	ASSERT_STR_EQ("x", kv_value(kv_dict_find(dict, "abc")));
	ASSERT_STR_EQ("hmm", kv_value(kv_dict_find(dict, "123")));

	kv_dict_del(dict);
	kv_del(kv2);
	kv_del(kv1);
	kv_del(kv0);
	return 0;
}

static int remove_key()
{
	struct kv* kv0 = kv_new("foo", "bar");
	struct kv* kv1 = kv_new("abc", "x");
	struct kv* kv2 = kv_new("123", "hmm");
	struct kv_dict* dict = kv_dict_new();

	ASSERT_INT_GE(0, kv_dict_insert(dict, kv0));
	ASSERT_INT_GE(0, kv_dict_insert(dict, kv1));
	ASSERT_INT_GE(0, kv_dict_insert(dict, kv2));

	ASSERT_STR_EQ("bar", kv_value(kv_dict_find(dict, "foo")));
	ASSERT_STR_EQ("x", kv_value(kv_dict_find(dict, "abc")));
	ASSERT_STR_EQ("hmm", kv_value(kv_dict_find(dict, "123")));

	kv_dict_remove(dict, "foo");
	ASSERT_FALSE(kv_dict_find(dict, "foo"));
	ASSERT_STR_EQ("x", kv_value(kv_dict_find(dict, "abc")));
	ASSERT_STR_EQ("hmm", kv_value(kv_dict_find(dict, "123")));

	kv_dict_remove(dict, "abc");
	ASSERT_FALSE(kv_dict_find(dict, "foo"));
	ASSERT_FALSE(kv_dict_find(dict, "abc"));
	ASSERT_STR_EQ("hmm", kv_value(kv_dict_find(dict, "123")));

	kv_dict_remove(dict, "123");
	ASSERT_FALSE(kv_dict_find(dict, "foo"));
	ASSERT_FALSE(kv_dict_find(dict, "abc"));
	ASSERT_FALSE(kv_dict_find(dict, "123"));

	kv_dict_del(dict);
	kv_del(kv2);
	kv_del(kv1);
	kv_del(kv0);
	return 0;
}

static int iterators()
{
	struct kv* kv0 = kv_new("a.b.c", "foo");
	struct kv* kv1 = kv_new("a.b.1", "bar");
	struct kv* kv2 = kv_new("a.a", "hmm");
	struct kv_dict* dict = kv_dict_new();

	ASSERT_INT_GE(0, kv_dict_insert(dict, kv0));
	ASSERT_INT_GE(0, kv_dict_insert(dict, kv1));
	ASSERT_INT_GE(0, kv_dict_insert(dict, kv2));

	const struct kv_dict_iter* it = kv_dict_find_ge(dict, "a.b");
	ASSERT_TRUE(it);
	ASSERT_STR_EQ("bar", kv_value(kv_dict_iter_kv(it)));

	it = kv_dict_iter_next(dict, it);
	ASSERT_TRUE(it);
	ASSERT_STR_EQ("foo", kv_value(kv_dict_iter_kv(it)));

	it = kv_dict_iter_next(dict, it);
	ASSERT_FALSE(it);

	kv_dict_del(dict);
	kv_del(kv2);
	kv_del(kv1);
	kv_del(kv0);
	return 0;
}

int main()
{
	int r = 0;

	RUN_TEST(one_kv);
	RUN_TEST(three_kv);
	RUN_TEST(remove_key);
	RUN_TEST(iterators);

	return r;
}
