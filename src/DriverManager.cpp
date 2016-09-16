/* Copyright (c) 2014-2016, Marel
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

#include <stdlib.h>
#include <glob.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <dlfcn.h>
#include <Driver.h>
#include "DriverManager.h"
//#include "canopen_priv.h"
//#include "PlutoExcept.h"

#define MAX_PROFILE_NR 0xffff

int verbose = 0;

using namespace std;

//-----------------------------------------------------------------------------
// Private functions
//-----------------------------------------------------------------------------
int DriverManager::getdir(const string& dir, vector<string> &files)
{
    string fullpath = dir + "/*.so";
    glob_t globbuf;

    glob(fullpath.c_str(), 0, NULL, &globbuf);

    for(unsigned int i = 0; i < globbuf.gl_pathc; i++)
        files.push_back(globbuf.gl_pathv[i]);

    globfree(&globbuf);

    return 0;
}


std::string& DriverManager::getDriverPath()
{
    if(!_driverPath.empty())
        return _driverPath;

    char* env_path = getenv("CANOPEN_DRIVER_PATH");
    if(env_path)
        _driverPath = env_path;
    else
        _driverPath = "/usr/lib/canmaster/drivers";

    return _driverPath;
}

//-----------------------------------------------------------------------------
// Public functions
//-----------------------------------------------------------------------------

DriverManager::DriverManager()
{
    unsigned int i;
    string path = getDriverPath();

    vector<string> files;
    files.reserve(64);

    getdir( path, files );

    defaultDriver = NULL;

    if (files.size() == 0)
    {
        if (verbose)
        {
            printf("No CAN drivers available in %s\n", path.c_str());
        }
        return;
    }

    for (i = 0; i < files.size(); i++)
    {
        ostringstream error_ss;
        string file = files[i];
        try
        {
            if (verbose)
            {
                printf("%-15s%-55s", "Loading driver ", file.c_str());
            }

            Driver *dr = new Driver(file.c_str());
            if (!dr)
            {
                continue;
            }
            if (defaultDriver == NULL && file.find("can_iom_generic") != string::npos)
            {
                defaultDriver = dr;
            }
            else
            {
                drivers[dr->getFilename()] = dr;
            }
            if (!verbose)
            {
                continue;
            }
            printf("[  OK  ]\n");
            int supportingProfile = -1;
            int profile = dr->getFirstProfile();
            printf("Supported profile(s):");
            while (profile >= 0)
            {
                supportingProfile = 1;
                if (profile == 0)
                {
                    printf(" All");
                    break;
                }
                else
                    printf(" %d", profile);
                if (profile > MAX_PROFILE_NR)
                {
                    printf("(invalid)");
                    break;
                }
                profile = dr->getNextProfile();
            }
            if (supportingProfile == -1)
                printf(" None, using node name only\n");
            else
                printf("\n");
        }
        catch( std::exception& exp )
        {
            if (verbose)
            {
                printf("[FAILED]\n");
            }
            printf("%s", exp.what());
        }
    }
}
DriverManager::~DriverManager()
{
    if (verbose && defaultDriver)
    {
        printf("Unloading driver %-53s[  OK  ]\n", defaultDriver->getFilename().c_str());
    }
    delete defaultDriver;

    driver_t::iterator it;
    for (it = drivers.begin(); it != drivers.end(); ++it)
    {
        Driver *dr = it->second;
        if (verbose)
        {
            printf("Unloading driver %-53s[  OK  ]\n", dr->getFilename().c_str());
        }
        delete dr;
    }
}

int DriverManager::createHandler(const char* nodeName, int profileNr,
        CanIOMapDriver::CanMasterInterface *cmi,
        CanIOMapDriver::CanIOHandlerInterface** chi )
{
    driver_t::iterator it;
    for (it = drivers.begin(); it != drivers.end(); ++it)
    {
        if (0 == it->second->createHandler( nodeName, profileNr, cmi, chi ))
        {
            if (verbose)
            {
                printf("Successful using driver %s for node %s\n", it->second->getFilename().c_str(), nodeName);
            }
            chi2driver[*chi] = it->second;
            return 0;
        }
    }

    if (defaultDriver)
    {
        if (0 == defaultDriver->createHandler( nodeName, profileNr, cmi, chi ))
        {
            if (verbose)
            {
                printf("Successful using driver %s for node %s\n", defaultDriver->getFilename().c_str(), nodeName);
            }
            chi2driver[*chi] = defaultDriver;
            return 0;
        }
    }
    return -1;
}

int DriverManager::deleteHandler(int /*profileNr*/,
        CanIOMapDriver::CanIOHandlerInterface* chi )
{
    Driver *dr = chi2driver[chi];
    if (dr)
    {
        dr->deleteHandler(chi);
        chi2driver.erase(chi);
        return 0;
    }
    return -1;
}

