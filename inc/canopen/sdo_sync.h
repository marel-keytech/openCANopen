#ifndef SDO_SYNC_H_
#define SDO_SYNC_H_

#include "sdo_req.h"

struct sdo_req* sdo_sync_read(int nodeid, int index, int subindex);
struct sdo_req* sdo_sync_write(struct sdo_req_info* info);

int64_t sdo_sync_read_i64(int nodeid, int index, int subindex);
uint64_t sdo_sync_read_u64(int nodeid, int index, int subindex);
int32_t sdo_sync_read_i32(int nodeid, int index, int subindex);
uint32_t sdo_sync_read_u32(int nodeid, int index, int subindex);
int16_t sdo_sync_read_i16(int nodeid, int index, int subindex);
uint16_t sdo_sync_read_u16(int nodeid, int index, int subindex);
int8_t sdo_sync_read_i8(int nodeid, int index, int subindex);
uint8_t sdo_sync_read_u8(int nodeid, int index, int subindex);

#endif /* SDO_SYNC_H_ */
