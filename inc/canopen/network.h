#ifndef CANOPEN_NETWORK_H_
#define CANOPEN_NETWORK_H_

#include <fcntl.h>
#include <stdint.h>

/* Reset the network and see which nodes respond to the reset signal.
 *
 * nodes_seen must be an array of length 128; prior values are not cleared.
 * timeout is in ms.
 */
int net_reset(int fd, char* nodes_seen, int timeout);

/* Reset each node individually and see if it responds to the reset signal.
 *
 * nodes_seen must be an array of length 128; prior values are not cleared.
 * start/stop is an inclusive range of node ids to probe
 * timeout is in ms.
 */
int net_reset_range(int fd, char* nodes_seen, int start, int end, int timeout);

/* Probe the network for nodes.
 *
 * nodes_seen must be an array of length 128; prior values are not cleared.
 * start/stop is an inclusive range of node ids to probe
 * timeout is in ms.
 */
int net_probe(int fd, char* nodes_seen, int start, int end, int timeout);

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
static inline int net_dont_block(int fd)
{
	return fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
}

int net__send_nmt(int fd, int cs, int nodeid);
int net__request_device_type(int fd, int nodeid);

int net__wait_for_bootup(int fd, char* nodes_seen, int start, int end,
       			 int timeout);

int net__wait_for_sdo(int fd, char* nodes_seen, int start, int end,
		      int timeout);

int net__gettime_ms();

int net_fix_sndbuf(int fd);

#endif /* CANOPEN_NETWORK_H_ */

