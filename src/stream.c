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

#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"

#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#include "stream.h"
#include "net-util.h"

static ssize_t stream__write(void* cookie, const char* buf, size_t size)
{
	ssize_t wsize = -1;
	ssize_t total = 0;

	while (total < (ssize_t)size) {
		wsize = net_write((int)cookie, buf + total, size - total, -1);
		if (wsize < 0)
			break;

		total += wsize;
	}

	return total > 0 || wsize >= 0 ? total : -1;
}

static ssize_t stream__read(void* cookie, char* buf, size_t size)
{
	return net_read((int)cookie, buf, size, -1);
}

static int stream__seek(void* cookie, off64_t* offset, int whence)
{
	*offset = lseek64((int)cookie, *offset, whence);
	return (int)*offset;
}

static int stream__close(void* cookie)
{
	return close((int)cookie);
}

static cookie_io_functions_t stream_funcs_ = {
	.read = stream__read,
	.write = stream__write,
	.seek = stream__seek,
	.close = stream__close,
};

FILE* stream_open(int fd, const char* mode)
{
	return fopencookie((void*)fd, mode, stream_funcs_);
}
