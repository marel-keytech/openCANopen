#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#include "stream.h"
#include "net-util.h"

ssize_t stream__write(void* cookie, const char* buf, size_t size)
{
	return net_write((int)cookie, buf, size, -1);
}

ssize_t stream__read(void* cookie, char* buf, size_t size)
{
	return net_read((int)cookie, buf, size, -1);
}

int stream__seek(void* cookie, off64_t* offset, int whence)
{
	*offset = lseek64((int)cookie, *offset, whence);
	return (int)*offset;
}

int stream__close(void* cookie)
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
