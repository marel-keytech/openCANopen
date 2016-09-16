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

#ifndef __driverManager_h__
#define __driverManager_h__

#include <map>
#include <string>
#include <vector>
#include "CanMasterInterface.h"
#include "CanIOHandlerInterface.h"

class Driver;

typedef std::map<std::string, Driver*> driver_t;
typedef std::map<CanIOHandlerInterface*, Driver*> chi2driver_t;

class DriverManager {
private:
    driver_t drivers;
    chi2driver_t chi2driver;
    Driver *defaultDriver;
    std::string _driverPath;

    int getdir(const std::string& dir, std::vector<std::string> &files);
    std::string& getDriverPath();
    DriverManager(const DriverManager &);
    DriverManager & operator=(const DriverManager&);

public:
    DriverManager();
    virtual ~DriverManager();
    int createHandler(const char* nodeName, int profileNr,
            CanIOMapDriver::CanMasterInterface *cmi,
            CanIOMapDriver::CanIOHandlerInterface** chi);
    int deleteHandler(int profileNr, CanIOMapDriver::CanIOHandlerInterface* chi);


};
#endif
