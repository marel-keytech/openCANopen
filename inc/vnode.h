#ifndef CANOPEN_VNODE_H_
#define CANOPEN_VNODE_H_

#include "sock.h"

int co_vnode_init(enum sock_type type, const char* iface,
		  const char* config_path, int nodeid);
void co_vnode_destroy(void);

#endif /* CANOPEN_VNODE_H_ */
