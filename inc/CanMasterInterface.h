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

