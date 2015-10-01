#ifndef CAN_TCP_H_
#define CAN_TCP_H_

int can_tcp_bridge_server(const char* can, int port);
int can_tcp_bridge_client(const char* can, const char* address, int port);

#endif /* CAN_TCP_H_ */

