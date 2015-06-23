#ifndef _CANOPEN_SDO_CLIENT_H
#define _CANOPEN_SDO_CLIENT_H

#include <string.h>
#include <unistd.h>
#include <linux/can.h>

enum sdo_req_state {
	SDO_REQ_REMOTE_ABORT = -2,
	SDO_REQ_ABORTED = -1,
	SDO_REQ_INIT = 0,
	SDO_REQ_INIT_EXPEDIATED,
	SDO_REQ_SEG,
	SDO_REQ_END_SEGMENT,
	SDO_REQ_DONE,
};

enum sdo_req_type { SDO_REQ_DL, SDO_REQ_UL };

struct sdo_req {
	enum sdo_req_type type;
	enum sdo_req_state state;
	struct can_frame frame;
	int is_toggled;

	int index;
	int subindex;

	ssize_t indicated_size;

	char* addr;
	size_t size;
	int pos;

	int have_frame;
};

int sdo_request_download(struct sdo_req* self, int index, int subindex,
			 const void* addr, size_t size);

int sdo_request_upload(struct sdo_req* self, int index, int subindex);

int sdo_dl_req_next_frame(struct sdo_req* self, struct can_frame* out);
int sdo_ul_req_next_frame(struct sdo_req* self, struct can_frame* out);

int sdo_dl_req_feed(struct sdo_req* self, const struct can_frame* frame);
int sdo_ul_req_feed(struct sdo_req* self, const struct can_frame* frame);

int sdo_req_feed(struct sdo_req* self, const struct can_frame* frame);

#endif /* _CANOPEN_SDO_CLIENT_H */

