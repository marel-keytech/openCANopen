#ifndef NET_UTIL_H_
#define NET_UTIL_H_

#include <unistd.h>
#include <stdint.h>

struct can_frame;

/* A wrapper around 'write' with ms timeout
 */
ssize_t net_write(int fd, const void* src, size_t size, int timeout);

/* A wrapper around 'read' with ms timeout
 */
ssize_t net_read(int fd, void* dst, size_t size, int timeout);

ssize_t net_write_frame(int fd, const struct can_frame* cf, int timeout);
ssize_t net_read_frame(int fd, struct can_frame* cf, int timeout);
ssize_t net_filtered_read_frame(int fd, struct can_frame* cf, int timeout,
				uint32_t can_id);

/* Make socket non-blocking
 */
int net_dont_block(int fd);
int net_fix_sndbuf(int fd);
int net_reuse_addr(int fd);
int net_dont_delay(int fd);

#endif /* NET_UTIL_H_ */
