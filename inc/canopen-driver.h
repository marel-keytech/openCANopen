#ifndef _CANOPEN_DRIVER_H
#define _CANOPEN_DRIVER_H

#include <stdint.h>

struct co_drv;
struct co_sdo_req;

enum co_sdo_type {
	CO_SDO_DOWNLOAD = 1,
	CO_SDO_UPLOAD
};

struct co_emcy {
	uint16_t code;
	uint8_t reg;
	uint64_t manufacturer_error;
};

typedef void (*co_free_fn)(void*);
typedef void (*co_pdo_fn)(struct co_drv*, const void* data, size_t size);
typedef void (*co_sdo_done_fn)(struct co_drv*, struct co_sdo_req* req);
typedef void (*co_emcy_fn)(struct co_drv*, struct co_emcy*);

const char* co_get_network_name(const struct co_drv* self);
int co_get_nodeid(const struct co_drv* self);
uint32_t co_get_vendor_id(const struct co_drv* self);
uint32_t co_get_product_code(const struct co_drv* self);
uint32_t co_get_revision_number(const struct co_drv* self);
const char* co_get_name(const struct co_drv* self);

void co_set_context(struct co_drv* self, void* context, co_free_fn fn);
void* co_get_context(struct co_drv* self);

void co_set_pdo1_fn(struct co_drv* self, co_pdo_fn fn);
void co_set_pdo2_fn(struct co_drv* self, co_pdo_fn fn);
void co_set_pdo3_fn(struct co_drv* self, co_pdo_fn fn);
void co_set_pdo4_fn(struct co_drv* self, co_pdo_fn fn);
void co_set_emcy_fn(struct co_drv* self, co_emcy_fn fn);

struct co_sdo_req* co_sdo_req_new(struct co_drv* drv);
void co_sdo_req_ref(struct co_sdo_req* self);
int co_sdo_req_unref(struct co_sdo_req* self);
void co_sdo_req_set_indices(struct co_sdo_req* self, int index, int subindex);
void co_sdo_req_set_type(struct co_sdo_req* self, enum co_sdo_type type);
void co_sdo_req_set_data(struct co_sdo_req* self, const void* data, size_t sz);
void co_sdo_req_set_done_fn(struct co_sdo_req* self, co_sdo_done_fn fn);
int co_sdo_req_start(struct co_sdo_req* self);

#endif /* _CANOPEN_DRIVER_H */
