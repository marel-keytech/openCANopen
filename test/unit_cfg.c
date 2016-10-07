#include "tst.h"
#include "cfg.h"
#include "canopen/master.h"

int cfg__load_stream(FILE* stream);

static int test_priority_order(void)
{
	const char* text =
	"[all]\n"
	"a=1\n"
	"b=2\n"
	"c=3\n"
	"[=mynode]\n"
	"a=4\n"
	"b=5\n"
	"[#42]\n"
	"a=6\n"
	"";

	struct co_master_node* node;

	node = co_master_get_node(23);
	strcpy(node->name, "foobar");

	node = co_master_get_node(42);
	strcpy(node->name, "mynode");

	FILE* stream = fmemopen((void*)text, strlen(text) + 1, "r");
	cfg__load_stream(stream);
	fclose(stream);

	ASSERT_UINT_EQ(1, cfg_get_uint(23, "a", 0));
	ASSERT_UINT_EQ(2, cfg_get_uint(23, "b", 0));
	ASSERT_UINT_EQ(3, cfg_get_uint(23, "c", 0));

	ASSERT_UINT_EQ(6, cfg_get_uint(42, "a", 0));
	ASSERT_UINT_EQ(5, cfg_get_uint(42, "b", 0));
	ASSERT_UINT_EQ(3, cfg_get_uint(42, "c", 0));

	cfg_unload();
	return 0;
}

int main()
{
	int r = 0;
	RUN_TEST(test_priority_order);
	return r;
}
