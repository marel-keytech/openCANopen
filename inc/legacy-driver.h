/* Copyright (c) 2014-2016, Marel
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef LEGACY_DRIVER_H_
#define LEGACY_DRIVER_H_

#include <unistd.h>
#include <stdint.h>

struct legacy_master_iface {
	int nodeid;
	int (*send_pdo)(int nodeid, int n, unsigned char* data, size_t size);
	int (*send_sdo)(int nodeid, int index, int subindex,
			unsigned char* data, size_t size);
	int (*request_sdo)(int nodeid, int index, int subindex);
	int (*set_node_state)(int nodeid, int state);
};

void* legacy_master_iface_new(struct legacy_master_iface*);
void legacy_master_iface_delete(void*);

void* legacy_driver_manager_new();
void legacy_driver_manager_delete(void*);

int legacy_driver_manager_create_handler(void* obj, const char* name,
					 int profile_number,
					 void* master_iface,
					 void** driver_iface);

int legacy_driver_delete_handler(void* obj, int profile_number,
				 void* driver_interface);

int legacy_driver_iface_initialize(void* iface);

int legacy_driver_iface_process_emr(void* iface, int code, int reg,
				    uint64_t manufacturer_error);

int legacy_driver_iface_process_sdo(void* obj, int index, int subindex,
				    unsigned char* data, size_t size);

int legacy_driver_iface_process_pdo(void* obj, int n, const unsigned char* data,
				    size_t size);

int legacy_driver_iface_process_node_state(void* obj, int state);

#endif /* LEGACY_DRIVER_H_ */

