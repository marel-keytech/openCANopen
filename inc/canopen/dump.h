#ifndef CANOPEN_DUMP_H_
#define CANOPEN_DUMP_H_

enum co_dump_options {
	CO_DUMP_TCP = 1,
};

int co_dump(const char* addr, enum co_dump_options options);

#endif /*  CANOPEN_DUMP_H_ */
