/* Copyright (c) 2014-2017, Marel
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <string>
#include <dlfcn.h>
#include <stdexcept>

#include "Driver.h"
#include "CanIOHandlerInterface.h"

using namespace std;

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
    profilePos = 0;
    const char* dlsym_error;
    string tmp = filename;

    unsigned int pos = tmp.find_last_of('/') + 1;
    if (pos == tmp.npos)
        pos = 0;
    fileName = tmp.substr(pos);

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
        throw std::runtime_error("Cannot load driver " + string(filename)
                                 + ": " + dlsym_error);

    candriver_profile = tryLoadSymbol<int*>(handle, "candriver_profile");
    candriver_version = tryLoadSymbol<int*>(handle, "candriver_version");
    candriver_set_parent = tryLoadSymbol<candriver_set_parent_t*>(handle, "candriver_set_parent");
    candriver_create_handler = tryLoadSymbol<candriver_create_handler_t*>(handle, "candriver_create_handler");
    candriver_delete_handler = tryLoadSymbol<candriver_delete_handler_t*>(handle, "candriver_delete_handler");
    candriver_delete_all_handlers = tryLoadSymbol<candriver_delete_all_handlers_t*>(handle, "candriver_delete_all_handlers");

    candriver_close_driver = tryLoadSymbol<candriver_close_driver_t*>(handle, "candriver_close_driver");

    candriver_set_parent("canopen", 0);
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
}
