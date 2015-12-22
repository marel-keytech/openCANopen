#include <assert.h>
#include <string.h>
#include "canopen/sdo.h"
#include "canopen/sdo_srv.h"
#include "canopen.h"
#include "net-util.h"
#include "sock.h"

#ifndef CAN_MAX_DLC
#define CAN_MAX_DLC 8
#endif

#define MIN(a, b) ((a) < (b) ? (a) : (b))

int sdo_srv_init(struct sdo_srv* self, const struct sock* sock, int nodeid,
		 sdo_srv_fn on_init, sdo_srv_fn on_done)
{
	memset(self, 0, sizeof(*self));

	self->sock = *sock;
	self->nodeid = nodeid;
	self->on_init = on_init;
	self->on_done = on_done;
	self->comm_state = SDO_SRV_COMM_INIT_REQ;
	self->pos = 0;

	return vector_init(&self->buffer, 8);
}

void sdo_srv_destroy(struct sdo_srv* self)
{
	vector_destroy(&self->buffer);
}

int sdo_srv__send(struct sdo_srv* self, struct can_frame* cf)
{
	cf->can_id = R_TSDO + self->nodeid;
	return sock_send(&self->sock, cf, -1);
}

int sdo_srv__on_done(struct sdo_srv* self)
{
	sdo_srv_fn on_done = self->on_done;
	if (on_done)
		on_done(self);

	self->comm_state = SDO_SRV_COMM_INIT_REQ;

	return 0;
}

int sdo_srv__on_init(struct sdo_srv* self)
{
	sdo_srv_fn on_init = self->on_init;
	return on_init ? on_init(self) : 0;
}

int sdo_srv__remote_abort(struct sdo_srv* self, const struct can_frame* cf)
{
	self->status = SDO_REQ_REMOTE_ABORT;
	self->comm_state = SDO_SRV_COMM_INIT_REQ;
	self->abort_code = sdo_get_abort_code(cf);
	sdo_srv__on_done(self);
	return -1;
}

int sdo_srv_abort(struct sdo_srv* self, enum sdo_abort_code code)
{
	struct can_frame cf;
	sdo_clear_frame(&cf);
	self->status = SDO_REQ_LOCAL_ABORT;
	self->abort_code = code;
	self->comm_state = SDO_SRV_COMM_INIT_REQ;
	sdo_abort(&cf, code, self->index, self->subindex);
	sdo_srv__send(self, &cf);
	return -1;
}

int sdo_srv__init_req(struct sdo_srv* self, const struct can_frame* cf)
{
	self->is_toggled = 0;
	vector_clear(&self->buffer);

	if (cf->can_dlc < 4)
		return sdo_srv_abort(self, SDO_ABORT_GENERAL);

	self->index = sdo_get_index(cf);
	self->subindex = sdo_get_subindex(cf);

	return 0;
}

static ssize_t get_expedited_size(const struct can_frame* cf)
{
	assert(cf->can_dlc >= SDO_EXPEDIATED_DATA_IDX);

	size_t max_size = cf->can_dlc - SDO_EXPEDIATED_DATA_IDX;

	return sdo_is_size_indicated(cf)
	     ? MIN(sdo_get_expediated_size(cf), max_size) : max_size;
}

int sdo_srv__dl_init_res(struct sdo_srv* self)
{
	struct can_frame cf;
	sdo_clear_frame(&cf);
	sdo_set_cs(&cf, SDO_SCS_DL_INIT_RES);
	sdo_set_index(&cf, self->index);
	sdo_set_subindex(&cf, self->subindex);
	cf.can_dlc = 4;
	return sdo_srv__send(self, &cf);
}

int sdo_srv__dl_expediated(struct sdo_srv* self, const struct can_frame* cf)
{
	const void* data = &cf->data[SDO_EXPEDIATED_DATA_IDX];
	size_t size = get_expedited_size(cf);
	assert(size <= SDO_EXPEDIATED_DATA_SIZE);

	/* vector should already be at least 8 bytes so this should not fail */
	int rc = vector_assign(&self->buffer, data, size);
	assert(rc == 0);

	self->status = SDO_REQ_OK;
	sdo_srv__on_done(self);

	return sdo_srv__dl_init_res(self);
}

int sdo_srv__dl_init_req(struct sdo_srv* self, const struct can_frame* cf)
{
	if (sdo_srv__init_req(self, cf) < 0)
		return -1;

	self->req_type = SDO_REQ_DOWNLOAD;

	if (sdo_srv__on_init(self) < 0)
		return -1;

	if (sdo_is_expediated(cf))
		return sdo_srv__dl_expediated(self, cf);

	self->status = SDO_REQ_PENDING;
	self->comm_state = SDO_SRV_COMM_DL_SEG_REQ;
	return sdo_srv__dl_init_res(self);
}

static int get_segment_size(const struct can_frame* cf)
{
	assert(cf->can_dlc >= SDO_SEGMENT_IDX);

	size_t max_size = cf->can_dlc - SDO_SEGMENT_IDX;

	return sdo_is_size_indicated(cf)
	     ? MIN(sdo_get_segment_size(cf), max_size) : max_size;
}

int sdo_srv__dl_seg_res(struct sdo_srv* self)
{
	struct can_frame cf;
	sdo_clear_frame(&cf);
	sdo_set_cs(&cf, SDO_SCS_DL_SEG_RES);
	if (self->is_toggled) sdo_toggle(&cf);
	self->is_toggled ^= 1;
	cf.can_dlc = 1;
	return sdo_srv__send(self, &cf);
}

int sdo_srv__seg_req(struct sdo_srv* self, const struct can_frame* cf)
{
	if (cf->can_dlc < 1)
		return sdo_srv_abort(self, SDO_ABORT_GENERAL);

	if (self->is_toggled != sdo_is_toggled(cf))
		return sdo_srv_abort(self, SDO_ABORT_TOGGLE);

	return 0;
}

int sdo_srv__dl_seg_req(struct sdo_srv* self, const struct can_frame* cf)
{
	if (self->comm_state != SDO_SRV_COMM_DL_SEG_REQ)
		return sdo_srv_abort(self, SDO_ABORT_GENERAL);

	if (sdo_srv__seg_req(self, cf) < 0)
		return -1;

	const void* data = &cf->data[SDO_SEGMENT_IDX];
	size_t size = get_segment_size(cf);

	if (vector_append(&self->buffer, data, size) < 0)
		return sdo_srv_abort(self, SDO_ABORT_NOMEM);

	if (sdo_is_end_segment(cf)) {
		self->status = SDO_REQ_OK;
		sdo_srv__on_done(self);
	}

	return sdo_srv__dl_seg_res(self);
}

int sdo_srv__ul_expediated(struct sdo_srv* self)
{
	struct can_frame cf;
	void* data = &cf.data[SDO_EXPEDIATED_DATA_IDX];
	size_t size = self->buffer.index;

	assert(size <= SDO_EXPEDIATED_DATA_SIZE);

	sdo_clear_frame(&cf);
	sdo_set_cs(&cf, SDO_SCS_UL_INIT_RES);
	sdo_set_index(&cf, self->index);
	sdo_set_subindex(&cf, self->subindex);

	sdo_expediate(&cf);
	sdo_indicate_size(&cf);
	sdo_set_expediated_size(&cf, self->buffer.index);

	memcpy(data, self->buffer.data, size);
	cf.can_dlc = SDO_EXPEDIATED_DATA_IDX + size;

	sdo_srv__on_done(self);

	return sdo_srv__send(self, &cf);
}

int sdo_srv__ul_init_res(struct sdo_srv* self)
{
	struct can_frame cf;
	sdo_clear_frame(&cf);
	sdo_set_cs(&cf, SDO_SCS_UL_INIT_RES);
	sdo_set_index(&cf, self->index);
	sdo_set_subindex(&cf, self->subindex);
	sdo_indicate_size(&cf);
	sdo_set_indicated_size(&cf, self->buffer.index);
	cf.can_dlc = CAN_MAX_DLC;
	return sdo_srv__send(self, &cf);
}

int sdo_srv__ul_init_req(struct sdo_srv* self, const struct can_frame* cf)
{
	if (sdo_srv__init_req(self, cf) < 0)
		return -1;

	self->req_type = SDO_REQ_UPLOAD;
	if (sdo_srv__on_init(self) < 0)
		return -1;

	if (self->buffer.index <= SDO_EXPEDIATED_DATA_SIZE)
		return sdo_srv__ul_expediated(self);

	self->status = SDO_REQ_PENDING;
	self->comm_state = SDO_SRV_COMM_UL_SEG_REQ;
	self->pos = 0;

	return sdo_srv__ul_init_res(self);
}

int sdo_srv__ul_seg_req(struct sdo_srv* self, const struct can_frame* cf)
{
	if (self->comm_state != SDO_SRV_COMM_UL_SEG_REQ)
		return sdo_srv_abort(self, SDO_ABORT_GENERAL);

	if (sdo_srv__seg_req(self, cf) < 0)
		return -1;

	struct can_frame rcf;
	sdo_clear_frame(&rcf);
	sdo_set_cs(&rcf, SDO_SCS_UL_SEG_RES);

	size_t size = MIN(SDO_SEGMENT_MAX_SIZE, self->buffer.index - self->pos);
	assert(size > 0);

	sdo_set_segment_size(&rcf, size);

	void* dst = &rcf.data[SDO_SEGMENT_IDX];
	const void* src = &self->buffer.data[self->pos];

	memcpy(dst, src, size);

	rcf.can_dlc = SDO_SEGMENT_IDX + size;
	self->pos += size;

	if (self->is_toggled) sdo_toggle(&rcf);
	self->is_toggled ^= 1;

	if (self->pos >= self->buffer.index) {
		self->status = SDO_REQ_OK;
		sdo_srv__on_done(self);
		sdo_end_segment(&rcf);
	}

	return sdo_srv__send(self, &rcf);
}

int sdo_srv_feed(struct sdo_srv* self, const struct can_frame* cf)
{
	assert(cf->can_id == R_RSDO + self->nodeid);

	enum sdo_ccs cs = sdo_get_cs(cf);

	switch (cs) {
	case SDO_CCS_ABORT: return sdo_srv__remote_abort(self, cf);
	case SDO_CCS_DL_INIT_REQ: return sdo_srv__dl_init_req(self, cf);
	case SDO_CCS_DL_SEG_REQ: return sdo_srv__dl_seg_req(self, cf);
	case SDO_CCS_UL_INIT_REQ: return sdo_srv__ul_init_req(self, cf);
	case SDO_CCS_UL_SEG_REQ: return sdo_srv__ul_seg_req(self, cf);
	}

	return sdo_srv_abort(self, SDO_ABORT_INVALID_CS);
}
