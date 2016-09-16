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

#ifndef _CANOPEN_SOCKETCAN_H
#define _CANOPEN_SOCKETCAN_H

#include <sys/socket.h>
#include <linux/can.h>

#define CANOPEN_SLAVE_FILTER_LENGTH 9
#define CANOPEN_MASTER_FILTER_LENGTH 10

void socketcan_make_slave_filters(struct can_filter* filters, int nodeid);
void socketcan_make_master_filters(struct can_filter* filters, int nodeid);
int socketcan_open(const char* iface);
int socketcan_apply_filters(int fd, struct can_filter* filters, int n);

int socketcan_open_slave(const char* iface, int nodeid);
int socketcan_open_master(const char* iface, int nodeid);

#endif /* _CANOPEN_SOCKETCAN_H */

