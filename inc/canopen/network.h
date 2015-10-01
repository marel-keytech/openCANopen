#ifndef CANOPEN_NETWORK_H_
#define CANOPEN_NETWORK_H_

#include <stdint.h>

struct can_frame;

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

int net__send_nmt(int fd, int cs, int nodeid);
int net__request_device_type(int fd, int nodeid);

int net__wait_for_bootup(int fd, char* nodes_seen, int start, int end,
       			 int timeout);

int net__wait_for_sdo(int fd, char* nodes_seen, int start, int end,
		      int timeout);

#endif /* CANOPEN_NETWORK_H_ */

