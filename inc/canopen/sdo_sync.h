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

#ifndef SDO_SYNC_H_
#define SDO_SYNC_H_

#include "sdo_req.h"

struct sdo_req* sdo_sync_read(int nodeid, int index, int subindex);
int sdo_sync_write(int nodeid, struct sdo_req_info* info);

int64_t sdo_sync_read_i64(int nodeid, int index, int subindex);
uint64_t sdo_sync_read_u64(int nodeid, int index, int subindex);
int32_t sdo_sync_read_i32(int nodeid, int index, int subindex);
uint32_t sdo_sync_read_u32(int nodeid, int index, int subindex);
int16_t sdo_sync_read_i16(int nodeid, int index, int subindex);
uint16_t sdo_sync_read_u16(int nodeid, int index, int subindex);
int8_t sdo_sync_read_i8(int nodeid, int index, int subindex);
uint8_t sdo_sync_read_u8(int nodeid, int index, int subindex);

int sdo_sync_write_i64(int nodeid, struct sdo_req_info* info, int64_t value);
int sdo_sync_write_u64(int nodeid, struct sdo_req_info* info, uint64_t value);
int sdo_sync_write_i32(int nodeid, struct sdo_req_info* info, int32_t value);
int sdo_sync_write_u32(int nodeid, struct sdo_req_info* info, uint32_t value);
int sdo_sync_write_i16(int nodeid, struct sdo_req_info* info, int16_t value);
int sdo_sync_write_u16(int nodeid, struct sdo_req_info* info, uint16_t value);
int sdo_sync_write_i8(int nodeid, struct sdo_req_info* info, int8_t value);
int sdo_sync_write_u8(int nodeid, struct sdo_req_info* info, uint8_t value);

#endif /* SDO_SYNC_H_ */
