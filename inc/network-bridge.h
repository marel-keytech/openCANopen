#ifndef NETWORK_BRIDGE_H_
#define NETWORK_BRIDGE_H_

int can_tcp_bridge_server(const char* can, int port);
int can_tcp_bridge_client(const char* can, const char* address, int port);

#endif /* NETWORK_BRIDGE_H_ */

