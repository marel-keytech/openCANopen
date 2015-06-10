
#ifndef _can_master_interface__h
#define _can_master_interface__h

#include <stddef.h>
/**
 * @class CanIoMapDriver CanMasterInteface.h
 */
namespace CanIOMapDriver
{

class CanMasterInterface
{
protected:
    int nodeId;

    public:
        CanMasterInterface(){};
        virtual ~CanMasterInterface(){};
        virtual int sendPdo1(unsigned char *data, size_t size) = 0;
        virtual int sendPdo2(unsigned char *data, size_t size) = 0;
        virtual int sendPdo3(unsigned char *data, size_t size) = 0;
        virtual int sendPdo4(unsigned char *data, size_t size) = 0;
        virtual int requestPdo1() = 0;
        virtual int requestPdo2() = 0;
        virtual int requestPdo3() = 0;
        virtual int requestPdo4() = 0;
        virtual int sendSdo(int index, int subindex, unsigned char *data, size_t size) = 0;
        virtual int requestSdo(int index, int subindex) = 0;
        virtual int setNodeState(int state) = 0;
        int getNodeId() { return nodeId; }
};

} /* End of namespace */

#endif /* _can_master_interface__h */

