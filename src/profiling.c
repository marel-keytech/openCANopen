#include <stdlib.h>
#include "profiling.h"

int profiling_is_active_ = -1;
uint64_t profiling_start_time_ = 0;

int profiling_getenv(void)
{
	return getenv("CANOPEN_PROFILE") != NULL;
}
