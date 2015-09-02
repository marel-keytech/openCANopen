#ifndef SDO_ASYNC_H_
#define SDO_ASYNC_H_

#include <mloop.h>
#include "vector.h"
#include "canopen/sdo_req_enums.h"
#include "canopen/sdo.h"

struct sdo_async;
struct can_frame;

typedef void (*sdo_async_fn)(struct sdo_async* async);

enum sdo_async_state {
	SDO_ASYNC_IDLE,
	SDO_ASYNC_STARTING,
	SDO_ASYNC_RUNNING,
	SDO_ASYNC_STOPPING
};

enum sdo_async_comm_state {
	SDO_ASYNC_COMM_START = 0,
	SDO_ASYNC_COMM_INIT_RESPONSE,
	SDO_ASYNC_COMM_SEG_RESPONSE,
};

struct sdo_async {
	int fd;
	unsigned int nodeid;
	enum sdo_req_type type;
	enum sdo_async_state state;
	enum sdo_async_comm_state comm_state;
	struct mloop_timer* timer;
	struct vector buffer;
	sdo_async_fn on_done;
	int index, subindex;
	int is_toggled;
	size_t pos;
	enum sdo_req_status status;
	enum sdo_abort_code abort_code;
};

struct sdo_async_info {
	enum sdo_req_type type;
	int index, subindex;
	unsigned long timeout;
	const void* data;
	size_t size;
	sdo_async_fn on_done;
};

int sdo_async_init(struct sdo_async* self, int fd, int nodeid);
void sdo_async_destroy(struct sdo_async* self);

int sdo_async_start(struct sdo_async* self, const struct sdo_async_info* info);
int sdo_async_stop(struct sdo_async* self);

int sdo_async_feed(struct sdo_async* self, const struct can_frame* frame);

#endif /* SDO_ASYNC_H_ */

