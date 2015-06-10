#include <string>
#include <sstream>
#include <iostream>  // for cout
#include <dlfcn.h>
#include <stdexcept>

#include "Driver.h"
//#include "canopen_priv.h"
#include "CanIOHandlerInterface.h"

using namespace std;

string storage_prefix = "canopen.0";

int Driver::inst = 0;

int
Driver::getFirstProfile()
{
    profilePos = 0;
    if (candriver_profile)
        return candriver_profile[0];
    return -1;
}

int
Driver::getNextProfile()
{
    if (candriver_profile)
    {
        if (candriver_profile[profilePos] == 0)
            return -1;
        profilePos++;
        return candriver_profile[profilePos];
    }
    return -1;
}

Driver::Driver( const char *filename )
{
    /*
	state = StateFactory::create(storage_prefix.c_str(), "canopen.state.recipe");
	try
	{
		ostringstream oss;
		oss << "driver." << Driver::inst;
		state->link(&fileName, oss.str().c_str());
		Driver::inst++;
	}
	catch (std::exception& e)
	{
		cout << "Error: " << e.what() << endl;
	}
    */

    profilePos = 0;
    const char* dlsym_error;
    string tmp = filename;

    unsigned int pos = tmp.find_last_of('/') + 1;
    if (pos == tmp.npos)
        pos = 0;
    fileName = tmp.substr(pos);
    //state->commit();

    candriver_profile = NULL;
    candriver_version = NULL;
    candriver_set_parent = NULL;
    candriver_create_handler = NULL;
    candriver_delete_handler = NULL;
    candriver_delete_all_handlers = NULL;
    candriver_close_driver = NULL;

    handle = dlopen( filename, RTLD_NOW);
    dlsym_error = dlerror();
    if (handle==NULL)
    {
        ostringstream error_ss;
        error_ss << "Cannot load driver [" << filename << "] => " << dlsym_error << endl;
        throw std::runtime_error( error_ss.str() );
    }
    candriver_profile = ((int*)dlsym( handle, "candriver_profile" ));
    dlsym_error = dlerror();
    if (dlsym_error)
    {
        ostringstream error_ss;
        error_ss << "Could not load symbol candriver_profile" << endl;
        throw std::runtime_error( error_ss.str() );
    }
    candriver_version = ((int*)dlsym( handle, "candriver_version" ));
    dlsym_error = dlerror();
    if (dlsym_error)
    {
        ostringstream error_ss;
        error_ss << "Could not load symbol candriver_version" << endl;
        throw std::runtime_error( error_ss.str() );
    }
    candriver_set_parent = (candriver_set_parent_t*)dlsym( handle, "candriver_set_parent" );
    dlsym_error = dlerror();
    if (dlsym_error)
    {
        ostringstream error_ss;
        error_ss << "Could not load symbol candriver_set_parent" << endl;
        throw std::runtime_error( error_ss.str() );
    }
    candriver_create_handler = (candriver_create_handler_t*)dlsym( handle, "candriver_create_handler" );
    dlsym_error = dlerror();
    if (dlsym_error)
    {
        ostringstream error_ss;
        error_ss << "Could not load symbol candriver_create_handler" << endl;
        throw std::runtime_error( error_ss.str() );
    }
    candriver_delete_handler = (candriver_delete_handler_t*)dlsym( handle, "candriver_delete_handler" );
    dlsym_error = dlerror();
    if (dlsym_error)
    {
        ostringstream error_ss;
        error_ss << "Could not load symbol candriver_delete_handler" << endl;
        throw std::runtime_error( error_ss.str() );
    }
    candriver_delete_all_handlers = (candriver_delete_all_handlers_t*)dlsym( handle, "candriver_delete_all_handlers" );
    dlsym_error = dlerror();
    if (dlsym_error)
    {
        ostringstream error_ss;
        error_ss << "Could not load symbol candriver_delete_all_handlers" << endl;
        throw std::runtime_error( error_ss.str() );
    }
    candriver_close_driver = (candriver_close_driver_t*)dlsym( handle, "candriver_close_driver" );
    dlsym_error = dlerror();
    if (dlsym_error)
    {
        ostringstream error_ss;
        error_ss << "Could not load symbol candriver_close_driver" << endl;
        throw std::runtime_error( error_ss.str() );
    }
    candriver_set_parent( "canopen", 0 );
}

int
Driver::createHandler( const char* nodeName, int32_t profileNr,
                       CanIOMapDriver::CanMasterInterface *cmi,
                       CanIOMapDriver::CanIOHandlerInterface** chi )
{
    if (candriver_create_handler)
        return candriver_create_handler( nodeName, profileNr, cmi, chi );
    return -1;
}

int Driver::deleteHandler(CanIOMapDriver::CanIOHandlerInterface* chi )
{
    if (candriver_delete_handler)
        return candriver_delete_handler( chi );
    return -1;
}

Driver::~Driver()
{
    candriver_delete_all_handlers();
    candriver_close_driver();
    dlclose( handle );
    fileName = "";
    //state->commit();
    //delete state;
}
