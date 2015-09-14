#include "canopen/byteorder.h"
#include "canopen/sdo_sync.h"

struct sdo_req* sdo_sync_read(int nodeid, int index, int subindex)
{
	struct sdo_req_info info = {
		.type = SDO_REQ_UPLOAD,
		.index = index,
		.subindex = subindex
	};

	struct sdo_req* req = sdo_req_new(&info);
	if (!req)
		return NULL;

	if (sdo_req_start(req, sdo_req_queue_get(nodeid)) < 0)
		goto done;

	sdo_req_wait(req);

	if (req->status != SDO_REQ_OK)
		goto done;

	return req;

done:
	sdo_req_unref(req);
	return NULL;
}

#define DECLARE_SDO_READ(type, name) \
type sdo_sync_read_ ## name(int nodeid, int index, int subindex) \
{ \
	type value = 0; \
	struct sdo_req* req = sdo_sync_read(nodeid, index, subindex); \
	if (!req) \
		return 0; \
	if (req->data.index > sizeof(value)) \
		goto done; \
	byteorder2(&value, req->data.data, sizeof(value), req->data.index); \
done: \
	sdo_req_unref(req); \
	return value; \
}

DECLARE_SDO_READ(int64_t, i64)
DECLARE_SDO_READ(uint64_t, u64)
DECLARE_SDO_READ(int32_t, i32)
DECLARE_SDO_READ(uint32_t, u32)
DECLARE_SDO_READ(int16_t, i16)
DECLARE_SDO_READ(uint16_t, u16)
DECLARE_SDO_READ(int8_t, i8)
DECLARE_SDO_READ(uint8_t, u8)

