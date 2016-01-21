#ifndef CANOPEN_VNODE_H_
#define CANOPEN_VNODE_H_

#include "sock.h"

struct vnode;

struct vnode* co_vnode_new(enum sock_type type, const char* iface,
			   const char* config_path, int nodeid);
void co_vnode_destroy(struct vnode* self);

#endif /* CANOPEN_VNODE_H_ */
