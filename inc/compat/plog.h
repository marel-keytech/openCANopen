#ifndef PLOG_H_
#define PLOG_H_

#include <syslog.h>

#define LOG_ERROR LOG_ERR
#define plog(prio, fmt, ...) syslog(prio, fmt, ## __VA_ARGS__)

#endif /* PLOG_H_ */
