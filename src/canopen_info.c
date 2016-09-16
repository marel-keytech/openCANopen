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

#include <stdio.h>
#include <sharedmalloc.h>
#include "canopen_info.h"

struct canopen_info* canopen_info_ = NULL;

static const char canopen_info_name[] = "canopen2";
static const char canopen_info_description[] = "canopen2.xml";

int canopen_info_init(const char* iface)
{
	char buffer[256];
	snprintf(buffer, sizeof(buffer), "%s.%s", canopen_info_name, iface);
	buffer[sizeof(buffer) - 1] = '\0';

	canopen_info_ = s_malloc(sizeof(struct canopen_info) * 127, buffer,
				 canopen_info_description);
	if (!canopen_info_)
		return -1;

	for (size_t i = 0; i < 127; ++i)
		canopen_info_[i].is_active = 0;

	return 0;
}

void canopen_info_cleanup(void)
{
	s_free(canopen_info_);
}

