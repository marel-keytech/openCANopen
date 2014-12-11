#include "canopen/sdo.h"
#include "canopen/sdo_client.h"


int sdo_request_download(struct sdo_client* self, int nodeid, int index,
			 int subindex, const void* addr, size_t size)
{
	memset(&self->outgoing, 0, sizeof(struct can_frame));

	sdo_set_cs(&self->outgoing, SDO_CCS_DL_INIT_REQ);

	return 0;
}

int sdo_request_upload(struct sdo_client* self, int nodeid, int index,
		       int subindex, size_t expected_size)
{
	return 0;
}

