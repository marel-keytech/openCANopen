#ifndef SDO_REQ_ENUMS_H_
#define SDO_REQ_ENUMS_H_

enum sdo_req_type {
	SDO_REQ_UPLOAD = 1,
	SDO_REQ_DOWNLOAD
};

enum sdo_req_status {
	SDO_REQ_PENDING = 0,
	SDO_REQ_OK,
	SDO_REQ_LOCAL_ABORT,
	SDO_REQ_REMOTE_ABORT,
};

#endif /* SDO_REQ_ENUMS_H_ */
