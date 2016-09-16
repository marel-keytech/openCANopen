#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <canopen-driver.h>

#define EXPORT __attribute__((visibility("default")))
#define MIN(a, b) (a) < (b) ? (a) : (b)
#define NAME_SIZE 256

struct example {
	char name[NAME_SIZE];
};

void on_read_name(struct co_drv* drv, struct co_sdo_req* req)
{
	struct example* self = co_get_context(drv);

	if (co_sdo_req_get_status(req) != CO_SDO_REQ_OK)
		return;

	const char* data = co_sdo_req_get_data(req);
	size_t size = MIN(co_sdo_req_get_size(req), NAME_SIZE - 1);

	memset(self->name, 0, NAME_SIZE);
	memcpy(self->name, data, size);
}

int start_read_name(struct co_drv* drv, struct example* self)
{
	struct co_sdo_req* req = co_sdo_req_new(drv);
	if (!req)
		return -1;

	co_sdo_req_set_type(req, CO_SDO_UPLOAD);
	co_sdo_req_set_indices(req, 0x1008, 0);
	co_sdo_req_set_done_fn(req, on_read_name);

	int r = co_sdo_req_start(req);
	co_sdo_req_unref(req);
	return r;
}

void on_pdo1(struct co_drv* drv, const void* data, size_t size)
{
	struct example* self = co_get_context(drv);

	uint64_t value = 0;
	co_byteorder(&value, data, sizeof(value), size);

	printf("%s: %llx\n", self->name, value);
}

EXPORT
int co_drv_init(struct co_drv* drv)
{
	struct example* self = malloc(sizeof(*self));
	if (!self)
		return -1;

	memset(self, 0, sizeof(*self));

	co_set_context(drv, self, (co_free_fn)free);

	co_set_pdo1_fn(drv, on_pdo1);

	start_read_name(drv, self);

	return 0;
}
