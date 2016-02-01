#ifndef CANOPEN_INFO_H_
#define CANOPEN_INFO_H_

#include <stdint.h>
#include <assert.h>

struct canopen_info {
	uint32_t is_active;
	uint32_t device_type;
	uint32_t last_seen;
	uint32_t error_register;
	char name[64];
	char hw_version[64];
	char sw_version[64];
};

extern struct canopen_info* canopen_info_;

static inline struct canopen_info* canopen_info_get(int nodeid)
{
	assert(1 <= nodeid && nodeid <= 127);
	return &canopen_info_[nodeid - 1];
}

int canopen_info_init(const char* iface);
void canopen_info_cleanup(void);

#endif /* CANOPEN_INFO_H_ */
