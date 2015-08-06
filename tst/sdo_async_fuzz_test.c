#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>
#include "canopen/sdo_async.h"
#include "canopen/sdo_srv.h"
#include "canopen/network.h"
#include "canopen.h"
#include "fff.h"

DEFINE_FFF_GLOBALS;

struct mloop;

struct mloop_timer {
	int dummy;
};

FAKE_VALUE_FUNC(struct mloop*, mloop_default);
FAKE_VALUE_FUNC(struct mloop_timer*, mloop_timer_new);
FAKE_VALUE_FUNC(int, mloop_timer_start, struct mloop_timer*);
FAKE_VALUE_FUNC(int, mloop_timer_stop, struct mloop_timer*);
FAKE_VALUE_FUNC(int, mloop_start_timer, struct mloop*, struct mloop_timer*);
FAKE_VALUE_FUNC(int, mloop_timer_unref, struct mloop_timer*);
FAKE_VOID_FUNC(mloop_timer_set_time, struct mloop_timer*, uint64_t);
FAKE_VOID_FUNC(mloop_timer_set_context, struct mloop_timer*, void*,
	       mloop_free_fn);
FAKE_VOID_FUNC(mloop_timer_set_callback, struct mloop_timer*, mloop_timer_fn);
FAKE_VALUE_FUNC(void*, mloop_timer_get_context, const struct mloop_timer*);
FAKE_VOID_FUNC(on_done, struct sdo_async*);
FAKE_VALUE_FUNC(ssize_t, net_write_frame, int, const struct can_frame*, int);

struct mloop_timer timer;

static struct sdo_async client;

static void initialize()
{
	mloop_timer_new_fake.return_val = &timer;
	sdo_async_init(&client, 10, 42);
}

static void cleanup()
{
	sdo_async_destroy(&client);
}

int upload_fuzz()
{
	struct sdo_async_info info = {
		.type = SDO_ASYNC_UL,
		.index = 0x1234,
		.subindex = 42,
		.timeout = 1000,
	};

	assert(sdo_async_start(&client, &info) == 0);

	while (client.state == SDO_ASYNC_RUNNING) {
		struct can_frame cf;
		sdo_clear_frame(&cf);
		cf.can_id = R_TSDO + 42;
		cf.can_dlc = rand() % 8;
		*(int*)cf.data = rand();
		int cs = rand() % 5;
		if (cs == SDO_SCS_UL_INIT_RES) {
			sdo_set_index(&cf, 0x1234);
			sdo_set_subindex(&cf, 42);
		}
		sdo_set_cs(&cf, cs);
		sdo_async_feed(&client, &cf);
	}

	return 0;
}

static char dl_data[4096] = { 0 };

int download_fuzz()
{
	struct sdo_async_info info = {
		.type = SDO_ASYNC_DL,
		.index = 0x1234,
		.subindex = 42,
		.timeout = 1000,
		.data = dl_data,
		.size = sizeof(dl_data)
	};

	assert(sdo_async_start(&client, &info) == 0);

	while (client.state == SDO_ASYNC_RUNNING) {
		struct can_frame cf;
		sdo_clear_frame(&cf);
		cf.can_id = R_TSDO + 42;
		cf.can_dlc = rand() % 8;
		*(int*)cf.data = rand();
		int cs = rand() % 5;
		if (cs == SDO_SCS_DL_INIT_RES) {
			sdo_set_index(&cf, 0x1234);
			sdo_set_subindex(&cf, 42);
		}
		sdo_set_cs(&cf, cs);
		sdo_async_feed(&client, &cf);
	}

	return 0;
}

int main()
{
	initialize();

	for (int i = 0; i < 100000; ++i)
		upload_fuzz();

	for (int i = 0; i < 100000; ++i)
		download_fuzz();

	cleanup();
	return 0;
}
