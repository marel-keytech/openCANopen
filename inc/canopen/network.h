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

#ifndef CANOPEN_NETWORK_H_
#define CANOPEN_NETWORK_H_

#include <stdint.h>

struct can_frame;
struct sock;

/* Reset the network and see which nodes respond to the reset signal.
 *
 * nodes_seen must be an array of length 128; prior values are not cleared.
 * timeout is in ms.
 */
int co_net_reset(const struct sock* sock, char* nodes_seen, int timeout);

/* Reset each node individually and see if it responds to the reset signal.
 *
 * nodes_seen must be an array of length 128; prior values are not cleared.
 * start/stop is an inclusive range of node ids to probe
 * timeout is in ms.
 */
int co_net_reset_range(const struct sock* sock, char* nodes_seen, int start,
		       int end, int timeout);

/* Probe the network for nodes.
 *
 * nodes_seen must be an array of length 128; prior values are not cleared.
 * start/stop is an inclusive range of node ids to probe
 * timeout is in ms.
 */
int co_net_probe(const struct sock* sock, char* nodes_seen, int start, int end,
		 int timeout);

int co_net_probe_sdo(const struct sock* sock, char* nodes_seen, int start,
		     int end, int timeout);

int co_net_send_nmt(const struct sock* sock, int cs, int nodeid);
int co_net__request_device_type(const struct sock* sock, int nodeid);

int co_net__wait_for_bootup(const struct sock* sock, char* nodes_seen,
			    int start, int end, int timeout);

int co_net__wait_for_sdo(const struct sock* sock, char* nodes_seen, int start,
			 int end, int timeout);

#endif /* CANOPEN_NETWORK_H_ */

