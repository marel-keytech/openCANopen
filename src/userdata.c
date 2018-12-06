/* Copyright (c) 2018, Marel
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

#include "canopen.h"
#include "userdata.h"

#include <pst.h>
#include <digitalio_pin.h>
#include <stdio.h>
#include <string.h>
#include <appcbase.h>
#include <assert.h>

#define PST_CFG_PATH "/var/marel/pst/config"

const char* userdata__make_name(char* dst, unsigned int i, const char* suffix,
				size_t size)
{
	snprintf(dst, size, "%u.%s", i, suffix);
	dst[size - 1] = '\0';
	return dst;
}

struct pst_set* userdata__create_config(const char* path)
{
	struct pst_set* pst = pst_create_exclusive("data.ppst", path, 1, 0);
	if (!pst)
		return NULL;

	pst_lock_exclusive(pst);
	for (int i = CANOPEN_NODEID_MIN; i <= CANOPEN_NODEID_MAX; i++) {
		char name[128];

		userdata__make_name(name, i, "required", sizeof(name));
		pst_add_int(pst, name, i, 0, 0, 1, PST_FLAG_RW, "");

		userdata__make_name(name, i, "label", sizeof(name));
		pst_add_string(pst, name, i * 2, "", 80, PST_FLAG_RW, "");
	}

	pst_finalize(pst);
	pst_unlock(pst);
	return pst;
}

void userdata__load_required(struct userdata* self)
{
	pst_lock_shared(self->pst);

	for (struct pst_par* par = pst_get_first_par(self->pst); par;
	     par = pst_get_next_par(self->pst, par)) {
		struct pst_par_info_t info = { 0 };
		if (pst_get_par_info(par, &info) < 0)
			continue;

		unsigned int id = strtoul(info.name, NULL, 0);
		if (id > CANOPEN_NODEID_MAX)
			continue;

		int is_required = 0;
		pst_read_int(par, &is_required);

		unsigned int index = id / 64;
		uint64_t mask = 1ULL << (id % 64);

		if (is_required)
			self->required[index] |= mask;
		else
			self->required[index] &= ~mask;
	}

	pst_unlock(self->pst);
}

int userdata_init(struct userdata* self)
{
	memset(self, 0, sizeof(*self));

	self->missing[0] = UINT64_MAX;
	self->missing[1] = UINT64_MAX;

	int instance = appbase_get_instance();

	char path[256];
	snprintf(path, sizeof(path), PST_CFG_PATH "/canopen.userdata/%d",
		 instance);
	path[sizeof(path) - 1] = '\0';

	self->pst = pst_open_exclusive("data.ppst", path);
	if (!self->pst)
		self->pst = userdata__create_config(path);

	if (!self->pst)
		return -1;

	userdata__load_required(self);

	struct dio_pin_change_cbs pin_cbs = { 0 };
	self->pin = dio_register_pin("canopen", instance, "alarm",
				     iomc_id_invalid, iomc_dir_signal,
				     &pin_cbs);

	if (self->pin == iomc_id_invalid)
		goto failure;

	return 0;

failure:
	pst_close(self->pst);
	self->pst = NULL;
	return -1;
}

void userdata_destroy(struct userdata* self)
{
	if (!self->pst)
		return;

	pst_close(self->pst);
	dio_remove_pin(self->pin);
}

int userdata__is_required(struct userdata* self, unsigned int id)
{
	char name[128];
	userdata__make_name(name, id, "required", sizeof(name));
	int* p = pst_get_int_ptr(self->pst, name);
	return !!*p;
}

int userdata__have_missing_nodes(struct userdata* self)
{
	return (self->missing[0] & self->required[0])
	    || (self->missing[1] & self->required[1]);
}

void userdata_set_missing(struct userdata* self, unsigned int id)
{
	if (self->pin == iomc_invalid)
		return;

	assert(id < 128);

	self->missing[id / 64] |= 1ULL << (id % 64);

	if (userdata__have_missing_nodes(self))
		dio_set_signal(self->pin, dio_high, -1);
}

void userdata_clear_missing(struct userdata* self, unsigned int id)
{
	if (self->pin == iomc_invalid)
		return;

	self->missing[id / 64] &= ~(1ULL << (id % 64));

	if (!userdata__have_missing_nodes(self))
		dio_set_signal(self->pin, dio_low, -1);
}

void userdata_check_missing(struct userdata* self)
{
	if (self->pin == iomc_invalid)
		return;

	dio_set_signal(self->pin, userdata__have_missing_nodes(self), -1);
}

void userdata_reload(struct userdata* self)
{
	userdata__load_required(self);
}
