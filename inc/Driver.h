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

#ifndef __Driver_h__
#define __Driver_h__
#include <string>
#include <stdexcept>
#include <dlfcn.h>
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

    template<typename T> T tryLoadSymbol(void* handle, const char* name)
    {
        T sym = reinterpret_cast<T>(dlsym(handle, name));
	const char* err = dlerror();
	if (err)
            throw std::runtime_error("Could not load symbol '"
                  + std::string(name) + "': " + std::string(err));
        return sym;
    }

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
