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

#ifndef CANOPEN_USERDATA_
#define CANOPEN_USERDATA_

#ifdef NO_MAREL_CODE
struct userdata { };
#define userdata_init(...) (0)
#define userdata_destroy(...)
#define userdata_reload(...)
#define userdata_set_missing(...)
#define userdata_clear_missing(...)
#define userdata_check_missing(...)
#else
#include <stdint.h>

struct pst_set;

struct userdata {
	struct pst_set* pst;
	int pin;
	uint64_t required[2];
	uint64_t missing[2];
};

int userdata_init(struct userdata* self);
void userdata_destroy(struct userdata* self);

void userdata_reload(struct userdata* self);

void userdata_set_missing(struct userdata* self, unsigned int id);
void userdata_clear_missing(struct userdata* self, unsigned int id);

void userdata_check_missing(struct userdata* self);
#endif

#endif /* CANOPEN_USERDATA_ */
