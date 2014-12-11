#ifndef _CANOPEN_SDO_CLIENT_H
#define _CANOPEN_SDO_CLIENT_H

#include <string.h>
#include <linux/can.h>

enum sdo_req_state {
	SDO_REQ_ABORTED = -1,
	SDO_REQ_INIT = 0,
	SDO_REQ_SEG,
	SDO_REQ_SEG_TOGGLED,
	SDO_REQ_DONE,
};

struct sdo_req {
	struct sdo_req* next_req;

	int nodeid;
	int index;
	int subindex;

	enum sdo_req_state;
};

struct sdo_dl_req {
	struct sdo_req req;

	const void* addr;
	size_t size;
	int pos;
};

struct sdo_ul_req {
	struct sdo_req req;

	size_t expected_size;
	void* addr;
	size_t size;
	int pos;
};

struct sdo_client {
	struct sdo_req* requests[128];
	struct can_frame outgoing;
	int have_frame;
};

int sdo_request_download(struct sdo_client* self, int nodeid, int index,
			 int subindex, const void* addr, size_t size);

int sdo_request_upload(struct sdo_client* self, int nodeid, int index,
		       int subindex, size_t expected_size);

int sdo_dl_req_next_frame(struct sdo_dl_req* self, struct can_frame* out);
int sdo_ul_req_next_frame(struct sdo_ul_req* self, struct can_frame* out);

int sdo_dl_req_feed(struct sdo_dl_req* self, const struct can_frame* frame);
int sdo_ul_req_feed(struct sdo_ul_req* self, const struct can_frame* frame);

static inline void sdo_client_init(struct sdo_client* self)
{
	memset(self, 0, sizeof(*self));
}

int sdo_client_feed(struct sdo_client* self, const struct can_frame* frame);
static inline struct can_frame* sdo_client_get_frame(struct sdo_client* self)
{
	return self->have_frame ? &self->outgoing : NULL;
}

#endif /* _CANOPEN_SDO_CLIENT_H */

