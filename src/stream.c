#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#include "stream.h"
#include "net-util.h"

static ssize_t stream__write(void* cookie, const char* buf, size_t size)
{
	ssize_t wsize = -1;
	size_t total = 0;

	while (total < size) {
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

const static cookie_io_functions_t stream_funcs_ = {
	.read = stream__read,
	.write = stream__write,
	.seek = stream__seek,
	.close = stream__close,
};

FILE* stream_open(int fd, const char* mode)
{
	return fopencookie((void*)fd, mode, stream_funcs_);
}
