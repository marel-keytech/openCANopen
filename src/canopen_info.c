#include <sharedmalloc.h>
#include "canopen_info.h"

struct canopen_info* canopen_info_ = NULL;

static const char canopen_info_name[] = "canopen2";
static const char canopen_info_description[] = "canopen2.xml";

int canopen_info_init(void)
{
	canopen_info_ = s_malloc(sizeof(struct canopen_info) * 127,
				 canopen_info_name, canopen_info_description);
	return canopen_info_? 0 : -1;
}

void canopen_info_cleanup(void)
{
	s_free(canopen_info_);
}

