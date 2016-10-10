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

#ifndef CFG_H_
#define CFG_H_

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define CFG__PARAMETERS \
	X(string, iface, "") \
	X(uint, n_workers, 4) \
	X(uint, worker_stack_size, 0) \
	X(uint, job_queue_length, 256) \
	X(uint, sdo_queue_length, 1024) \
	X(uint, rest_port, 9191) \
	X(bool, be_strict, 0) \
	X(bool, use_tcp, 0) \
	X(uint, heartbeat_period, 0 /* ms */) \
	X(uint, heartbeat_timeout, 0 /* ms */) \
	X(uint, n_timeouts_max, 0) \
	X(uint, range_start, 0) \
	X(uint, range_stop, 0) \

#define CFG__NODE_PARAMETERS \
	X(bool, has_zero_guard_status, 0) \
	X(bool, ignore_sdo_multiplexer, 1) \
	X(bool, send_full_sdo_frame, 0) \
	X(uint, heartbeat_period, 10000 /* ms */) \
	X(uint, heartbeat_timeout, 1000 /* ms */) \
	X(uint, n_timeouts_max, 0) \

#define CFG__DEFINE_bool(name) int name
#define CFG__DEFINE_uint(name) uint64_t name
#define CFG__DEFINE_int(name) int64_t name
#define CFG__DEFINE_string(name) char name[256]
#define CFG__DEFINE_(type, ...) CFG__DEFINE_ ## type(__VA_ARGS__)

#define CFG__SET_bool(name, value) name = value
#define CFG__SET_uint(name, value) name = value
#define CFG__SET_int(name, value) name = value
#define CFG__SET_string(name, value) strlcpy(name, value, 256)
#define CFG__SET_(type, ...) CFG__SET_ ## type(__VA_ARGS__)

#define CFG__STRTO_bool(value) (strcmp(value, "yes") == 0)
#define CFG__STRTO_uint(value) strtoll(value, NULL, 0)
#define CFG__STRTO_int(value) strtoull(value, NULL, 0)
#define CFG__STRTO_string(value) value
#define CFG__STRTO_(type, ...) CFG__STRTO_ ## type(__VA_ARGS__)

struct cfg {
#define X(type, name, default_) CFG__DEFINE_(type, name);
	CFG__PARAMETERS
#undef X

	struct {
#define X(type, name, default_) CFG__DEFINE_(type, name);
		CFG__NODE_PARAMETERS
#undef X
	} node[128];
};

extern struct cfg cfg;

void cfg_load_defaults(void);
void cfg_load_globals(void);

int cfg_load_file(const char* path);
void cfg_unload_file(void);

void cfg_load_node(int id);

const char* cfg__file_read(int nodeid, const char* key);

#endif /* CFG_H_ */
