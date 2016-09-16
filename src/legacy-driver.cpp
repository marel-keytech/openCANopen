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

#include <exception>
#include <stdlib.h>
#include "plog.h"

extern "C" {
#include "legacy-driver.h"
}

#include "Driver.h"
#include "DriverManager.h"
#include "CanMasterInterface.h"
#include "CanIOHandlerInterface.h"

#define STR(x) #x

#define plogx(level, fmt, ...) \
	plog(level, "%s: " fmt, __func__, ## __VA_ARGS__)

using std::exception;

class LegacyMasterInterface: public CanMasterInterface {
public:
	LegacyMasterInterface(const struct legacy_master_iface* iface)
		: self(*iface)
	{
		this->nodeId = iface->nodeid;
	}

	virtual int sendPdo1(unsigned char* data, size_t size)
	{
		return self.send_pdo(getNodeId(), 1, data, size);
	}

	virtual int sendPdo2(unsigned char* data, size_t size)
	{
		return self.send_pdo(getNodeId(), 2, data, size);
	}

	virtual int sendPdo3(unsigned char* data, size_t size)
	{
		return self.send_pdo(getNodeId(), 3, data, size);
	}

	virtual int sendPdo4(unsigned char* data, size_t size)
	{
		return self.send_pdo(getNodeId(), 4, data, size);
	}

	virtual int requestPdo1() { return -1; }
	virtual int requestPdo2() { return -1; }
	virtual int requestPdo3() { return -1; }
	virtual int requestPdo4() { return -1; }

	virtual int sendSdo(int index, int subindex, unsigned char* data,
			    size_t size)
	{
		return self.send_sdo(getNodeId(), index, subindex, data, size);
	}

	virtual int requestSdo(int index, int subindex)
	{
		return self.request_sdo(getNodeId(), index, subindex);
	}

	virtual int setNodeState(int state)
	{
		return self.set_node_state(getNodeId(), state);
	}

private:
	legacy_master_iface self;
};

extern "C" {

void* legacy_master_iface_new(struct legacy_master_iface* callbacks)
{
	try {
		return new LegacyMasterInterface(callbacks);
	} catch (exception& e) {
		plogx(LOG_ERROR, "Caught exception: %s", e.what());
		return NULL;
	}
}

void legacy_master_iface_delete(void* obj)
{
	delete (LegacyMasterInterface*)obj;
}

void* legacy_driver_manager_new()
{
	try {
		return new DriverManager;
	} catch (exception& e) {
		plogx(LOG_ERROR, "Caught exception: %s", e.what());
		return NULL;
	}
}

void legacy_driver_manager_delete(void* obj)
{
	delete (DriverManager*)obj;
}

int legacy_driver_manager_create_handler(void* obj, const char* name,
					 int profile_number,
					 void* master_iface,
					 void** driver_iface)
{
	auto man = (DriverManager*)obj;
	try {
		return man->createHandler(name, profile_number,
					  (CanMasterInterface*)master_iface,
					  (CanIOHandlerInterface**)driver_iface);
	} catch (exception& e) {
		plogx(LOG_ERROR, "Caught exception: %s", e.what());
		return -1;
	}
}

int legacy_driver_delete_handler(void* obj, int profile_number,
				 void* driver_interface)
{
	auto man = (DriverManager*)obj;
	auto iface = (CanIOHandlerInterface*)driver_interface;
	try {
		return man->deleteHandler(profile_number, iface);
	} catch (exception& e) {
		plogx(LOG_ERROR, "Caught exception: %s", e.what());
		return -1;
	}
}

int legacy_driver_iface_initialize(void* obj)
{
	auto iface = (CanIOHandlerInterface*)obj;
	try {
		return iface->initialize();
	} catch (exception& e) {
		plogx(LOG_ERROR, "Caught exception: %s", e.what());
		return -1;
	}
}

int legacy_driver_iface_process_emr(void* obj, int code, int reg,
				    uint64_t manufacturer_error)
{
	auto iface = (CanIOHandlerInterface*)obj;
	try {
		return iface->processEmr(code, reg, manufacturer_error);
	} catch (exception& e) {
		plogx(LOG_ERROR, "Caught exception: %s", e.what());
		return -1;
	}
}

int legacy_driver_iface_process_sdo(void* obj, int index, int subindex,
				    unsigned char* data, size_t size)
{
	auto iface = (CanIOHandlerInterface*)obj;
	try {
		return iface->processSdo(index, subindex, data, size);
	} catch (exception& e) {
		plogx(LOG_ERROR, "Caught exception: %s", e.what());
		return -1;
	}
}

int legacy_driver_iface_process_pdo(void* obj, int n, const unsigned char* data,
				    size_t size)
{
	auto iface = (CanIOHandlerInterface*)obj;

	try {
		switch (n) {
		case 1: return iface->processPdo1((unsigned char*)data, size);
		case 2: return iface->processPdo2((unsigned char*)data, size);
		case 3: return iface->processPdo3((unsigned char*)data, size);
		case 4: return iface->processPdo4((unsigned char*)data, size);
		}
	} catch (exception& e) {
		plogx(LOG_ERROR, "Caught exception: %s", e.what());
		return -1;
	}

	abort();
	return -1;
}

int legacy_driver_iface_process_node_state(void* obj, int state)
{
	auto iface = (CanIOHandlerInterface*)obj;
	try {
		return iface->processNodeState(state);
	} catch (exception& e) {
		plogx(LOG_ERROR, "Caught exception: %s", e.what());
		return -1;
	}
}

} /* end of extern "C" */
