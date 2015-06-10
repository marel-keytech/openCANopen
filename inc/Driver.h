#ifndef __Driver_h__
#define __Driver_h__
#include <string>
#include "CanIOHandlerInterface.h"
#include "StateFactory.h"

class Driver
{
	static int inst;
	StateManagerInterface *state;
    int profilePos;
    int *candriver_profile;
    int *candriver_version;
    candriver_set_parent_t* candriver_set_parent;
    candriver_create_handler_t* candriver_create_handler;
    candriver_delete_handler_t* candriver_delete_handler;
    candriver_delete_all_handlers_t* candriver_delete_all_handlers;
    candriver_close_driver_t* candriver_close_driver;
    void *handle;
    std::string fileName;

public:
	Driver(const char *filename);
	~Driver();
	int createHandler(const char* nodeName, int32_t profileNr,
            CanIOMapDriver::CanMasterInterface *cmi,
            CanIOMapDriver::CanIOHandlerInterface** chi);
	int deleteHandler(CanIOMapDriver::CanIOHandlerInterface* chi );
	int getFirstProfile();
	int getNextProfile();
    std::string getFilename() { return fileName; }
};

#endif
