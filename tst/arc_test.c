#include <stdlib.h>
#include <string.h>
#include "tst.h"
#include "arc.h"

struct arc_test {
	int ref;
	int dummy;
};

static inline struct arc_test* arc_test_new(void)
{
	struct arc_test* self = malloc(sizeof(*self));
	if (!self)
		return NULL;

	memset(self, 0, sizeof(*self));

	self->ref = 1;

	return self;
}

static inline void arc_test_free_fn(struct arc_test* self)
{
	free(self);
}

ARC_GENERATE(arc_test, arc_test_free_fn);

static int test_ref_unref(void)
{
	struct arc_test* test = arc_test_new();
	ASSERT_TRUE(test);

	ASSERT_INT_EQ(1, test->ref);

	ASSERT_INT_EQ(2, arc_test_ref(test));
	ASSERT_INT_EQ(2, test->ref);

	ASSERT_INT_EQ(1, arc_test_unref(test));
	ASSERT_INT_EQ(1, test->ref);

	ASSERT_INT_EQ(0, arc_test_unref(test));

	return 0;
}

int main()
{
	int r = 0;
	RUN_TEST(test_ref_unref);
	return r;
}

