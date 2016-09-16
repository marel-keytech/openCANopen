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

#ifndef _CANOPEN_HEARTBEAT_H
#define _CANOPEN_HEARTBEAT_H

#include <linux/can.h>
#include "canopen/nmt.h"

static inline int heartbeat_is_valid(const struct can_frame* frame)
{
	return frame->can_dlc == 1;
}

static inline enum nmt_state heartbeat_get_state(const struct can_frame* frame)
{
	return frame->data[0] & 0x7f;
}

static inline int heartbeat_is_bootup(const struct can_frame* frame)
{
	return heartbeat_get_state(frame) == NMT_STATE_BOOTUP;
}

static inline void heartbeat_set_state(struct can_frame* frame,
				       enum nmt_state state)
{
	frame->data[0] = state & 0x7f;
}

#endif /* _CANOPEN_HEARTBEAT_H */

