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
