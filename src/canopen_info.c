#include <stdio.h>
#include <sharedmalloc.h>
#include "canopen_info.h"

struct canopen_info* canopen_info_ = NULL;

static const char canopen_info_name[] = "canopen2";
static const char canopen_info_description[] = "canopen2.xml";

int canopen_info_init(const char* iface)
{
	char buffer[256];
	snprintf(buffer, sizeof(buffer), "%s.%s", canopen_info_name, iface);
	buffer[sizeof(buffer) - 1] = '\0';

	canopen_info_ = s_malloc(sizeof(struct canopen_info) * 127, buffer,
				 canopen_info_description);
	if (!canopen_info_)
		return -1;

	for (size_t i = 0; i < 127; ++i)
		canopen_info_[i].is_active = 0;

	return 0;
}

void canopen_info_cleanup(void)
{
	s_free(canopen_info_);
}

