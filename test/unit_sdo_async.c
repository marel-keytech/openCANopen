#include <unistd.h>
#include <assert.h>
#include "canopen/sdo_async.h"
#include "canopen/sdo_srv.h"
#include "net-util.h"
#include "canopen.h"
#include "tst.h"
#include "fff.h"

#ifndef CAN_MAX_DLC
#define CAN_MAX_DLC 8
#endif

DEFINE_FFF_GLOBALS;

struct mloop;

struct mloop_timer {
	int dummy;
};

FAKE_VALUE_FUNC(struct mloop*, mloop_default);
FAKE_VALUE_FUNC(struct mloop_timer*, mloop_timer_new, struct mloop*);
FAKE_VALUE_FUNC(int, mloop_timer_start, struct mloop_timer*);
FAKE_VALUE_FUNC(int, mloop_timer_stop, struct mloop_timer*);
FAKE_VALUE_FUNC(int, mloop_timer_unref, struct mloop_timer*);
FAKE_VOID_FUNC(mloop_timer_set_time, struct mloop_timer*, uint64_t);
FAKE_VOID_FUNC(mloop_timer_set_context, struct mloop_timer*, void*,
	       mloop_free_fn);
FAKE_VOID_FUNC(mloop_timer_set_callback, struct mloop_timer*, mloop_timer_fn);
FAKE_VALUE_FUNC(void*, mloop_timer_get_context, const struct mloop_timer*);
FAKE_VOID_FUNC(on_done, struct sdo_async*);

struct mloop_timer timer;

static struct sdo_srv server;
static struct sdo_async client;

static int rfd, wfd;

static char srv_data[4096];
static size_t srv_size;
static int srv_index, srv_subindex;

static const char loremipsum[] = " \
Lorem ipsum dolor sit amet, consectetur adipiscing elit. Ut in mi faucibus, \
lacinia lectus vel, feugiat diam. Etiam nec placerat sem, at luctus arcu. \
Phasellus maximus viverra porta. In vehicula, eros vitae rutrum bibendum, dui \
ante lobortis erat, ac volutpat est velit suscipit lacus. Quisque nec congue \
ipsum. Nulla augue leo, commodo congue egestas id, mollis sit amet neque. \
Maecenas enim leo, aliquet quis sollicitudin suscipit, bibendum in neque. Donec \
pellentesque mauris mi, ut gravida velit lobortis facilisis. Sed eget sapien \
libero. Nullam consequat enim nec risus commodo convallis. Fusce vitae massa \
tellus. Pellentesque sit amet cursus nisl. Suspendisse id volutpat enim, eu \
mollis nulla. Ut a augue ut eros scelerisque convallis in nec lacus. Cras \
dictum risus id rhoncus egestas. \
";

static void reset_srv_data()
{
	srv_index = -1;
	srv_subindex = -1;
	srv_size = sizeof(srv_data);
	memset(srv_data, '.', sizeof(srv_data));
}

static void set_srv_data(const char* str)
{
	srv_index = -1;
	srv_subindex = -1;
	strcpy(srv_data, str);
	srv_size = strlen(str) + 1;
}

static int srv_get_obj(struct sdo_obj* obj, int index, int subindex)
{
	obj->addr = srv_data;
	obj->size = srv_size;
	obj->flags = SDO_OBJ_RW | SDO_OBJ_LT;
	srv_index = index;
	srv_subindex = subindex;
	return 0;
}

static void initialize()
{
	int fds[2];
	pipe(fds);
	rfd = fds[0];
	wfd = fds[1];
	static struct sock sock = { .type = SOCK_TYPE_CAN };
	sock.fd = wfd;

	net_dont_block(rfd);

	sdo_get_obj = srv_get_obj;

	sdo_srv_init(&server);

	mloop_timer_new_fake.return_val = &timer;
	sdo_async_init(&client, &sock, 42);
}

static void cleanup()
{
	sdo_async_destroy(&client);
	close(wfd);
	close(rfd);
}

static int feed_server(struct can_frame* cf);

static int push_to_server()
{
	struct can_frame out;
	ssize_t size = read(rfd, &out, sizeof(out));
	if (size != sizeof(out))
		return 0;

	return feed_server(&out);
}

static int feed_client(struct can_frame* cf)
{
	if (sdo_async_feed(&client, cf) < 0)
		return -1;

	return push_to_server();
}

static int feed_server(struct can_frame* cf)
{
	if (sdo_srv_feed(&server, cf) < 0)
		return -1;

	struct can_frame* out = sdo_srv_next(&server);
	if (!out)
		return 0;

	out->can_dlc = CAN_MAX_DLC; /* TODO: fix */
	out->can_id = R_TSDO + 42;

	return feed_client(out);
}

static int download(const char* str)
{
	size_t size = strlen(str) + 1;

	struct sdo_async_info info = {
		.type = SDO_REQ_DOWNLOAD,
		.index = 0x1234,
		.subindex = 42,
		.timeout = 1000,
		.data = str,
		.size = size,
		.on_done = on_done
	};

	RESET_FAKE(on_done);

	ASSERT_INT_EQ(0, sdo_async_start(&client, &info));
	reset_srv_data();
	push_to_server();
	ASSERT_STR_EQ(str, srv_data);
	ASSERT_UINT_EQ(size, client.buffer.index);
	ASSERT_INT_EQ(0x1234, srv_index);
	ASSERT_INT_EQ(42, srv_subindex);
	ASSERT_INT_EQ(1, on_done_fake.call_count);

	return 0;
}

static int upload(const char* str)
{
	struct sdo_async_info info = {
		.type = SDO_REQ_UPLOAD,
		.index = 0x1234,
		.subindex = 42,
		.timeout = 1000,
		.on_done = on_done,
	};

	RESET_FAKE(on_done);

	set_srv_data(str);
	ASSERT_INT_EQ(0, sdo_async_start(&client, &info));
	push_to_server();
	ASSERT_STR_EQ(str, client.buffer.data);
	ASSERT_UINT_EQ(strlen(str) + 1, client.buffer.index);
	ASSERT_INT_EQ(0x1234, srv_index);
	ASSERT_INT_EQ(42, srv_subindex);
	ASSERT_INT_EQ(1, on_done_fake.call_count);

	return 0;
}

static int test_download()
{
	return download("")
	    && download("f")
	    && download("fo")
	    && download("foo")
	    && download("foob")
	    && download("fooba")
	    && download("foobar")
	    && download("foobarx");
}

static int test_download_big()
{
	return download(loremipsum);
}

static int test_upload()
{
	return upload("")
	    && upload("f")
	    && upload("fo")
	    && upload("foo")
	    && upload("foob")
	    && upload("fooba")
	    && upload("foobar")
	    && upload("foobarx");
}

static int test_upload_big()
{
	return upload(loremipsum);
}

int main()
{
	int r = 0;
	initialize();
	RUN_TEST(test_download);
	RUN_TEST(test_download_big);
	RUN_TEST(test_upload);
	RUN_TEST(test_upload_big);
	cleanup();
	return r;
}
