#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "tst.h"
#include "fff.h"
#include "rest.h"
#include "vector.h"

DEFINE_FFF_GLOBALS;

FAKE_VALUE_FUNC(int, socket, int, int, int);
FAKE_VALUE_FUNC(int, bind, int, const struct sockaddr*, size_t);
FAKE_VALUE_FUNC(int, listen, int, int);
FAKE_VALUE_FUNC(int, close, int);
FAKE_VALUE_FUNC(int, net_dont_block, int);
FAKE_VALUE_FUNC(int, net_reuse_addr, int);
FAKE_VALUE_FUNC(ssize_t, read, int, void*, size_t);

static void reset_fakes(void)
{
	RESET_FAKE(socket);
	RESET_FAKE(bind);
	RESET_FAKE(listen);
	RESET_FAKE(close);
	RESET_FAKE(net_dont_block);
	RESET_FAKE(net_reuse_addr);
	RESET_FAKE(read);
}

static struct rest_service*
make_service(enum http_method method, const char* path)
{
	static struct rest_service service = { 0 };
	service.method = method;
	service.path = path;
	return &service;
}

static struct http_req*
make_req(enum http_method method, const char* path)
{
	static struct http_req req = { 0 };
	req.method = method;
	req.url_index = path ? 1 : 0;
	req.url[0] = (char*)path;
	return &req;
}

static int test_service_is_match(void)
{
	ASSERT_FALSE(rest__service_is_match(make_service(HTTP_OPTIONS, "foo"),
					    make_req(HTTP_GET, "foo")));
	ASSERT_FALSE(rest__service_is_match(make_service(HTTP_GET, "foo"),
					    make_req(HTTP_GET, "asdf")));
	ASSERT_FALSE(rest__service_is_match(make_service(HTTP_GET, "foo"),
					    make_req(HTTP_GET, NULL)));
	ASSERT_TRUE(rest__service_is_match(make_service(HTTP_GET, "foo"),
					   make_req(HTTP_GET, "foo")));
	ASSERT_TRUE(rest__service_is_match(make_service(HTTP_GET | HTTP_PUT,
							"foo"),
					   make_req(HTTP_GET, "foo")));
	ASSERT_TRUE(rest__service_is_match(make_service(HTTP_GET | HTTP_PUT,
							"foo"),
					   make_req(HTTP_PUT, "foo")));
	ASSERT_TRUE(rest__service_is_match(make_service(HTTP_PUT,
							"foo"),
					   make_req(HTTP_PUT, "foo")));
	ASSERT_TRUE(rest__service_is_match(make_service(HTTP_GET, "foo"),
					   make_req(HTTP_GET, "FOO")));
	return 0;
}

static struct rest_service*
find_service(enum http_method method, const char* path)
{
	struct http_req match;
	memset(&match, 0, sizeof(match));

	match.method = method;
	match.url_index = path ? 1 : 0;
	match.url[0] = (char*)path;

	return rest__find_service(&match);
}

static int try_missing_services(void)
{
	ASSERT_PTR_EQ(NULL, find_service(HTTP_GET, "sillyservice"));
	ASSERT_PTR_EQ(NULL, find_service(HTTP_PUT, "foobarservice"));
	ASSERT_PTR_EQ(NULL, find_service(HTTP_OPTIONS, "trollollo"));
	ASSERT_PTR_EQ(NULL, find_service(HTTP_GET, NULL));
	ASSERT_PTR_EQ(NULL, find_service(HTTP_PUT, NULL));
	ASSERT_PTR_EQ(NULL, find_service(HTTP_OPTIONS, NULL));
	return 0;
}

static int test_find_service_empty(void)
{
	rest__init_service_list();
	return try_missing_services();
}

static int test_find_service_method(enum http_method method)
{
	int r = 0;

	rest__init_service_list();
	ASSERT_INT_EQ(0, rest_register_service(method, "foobar",
					       (rest_fn)0xdeadbeef));
	r = try_missing_services();
	if (r != 0) return r;

	struct rest_service* service = find_service(method, "foobar");
	ASSERT_UINT_EQ(method | HTTP_OPTIONS, service->method);
	ASSERT_STR_EQ("foobar", service->path);
	ASSERT_PTR_EQ((void*)0xdeadbeef, service->fn);

	struct rest_service* options = find_service(HTTP_OPTIONS, "foobar");
	ASSERT_PTR_EQ(service, options);

	rest_cleanup();
	return 0;
}

static int test_find_service_one(void)
{
	int r = 0;
	r |= test_find_service_method(HTTP_GET);
	r |= test_find_service_method(HTTP_PUT);
	r |= test_find_service_method(HTTP_OPTIONS);
	return r;
}

static int test_open_server__socket_failure(void)
{
	reset_fakes();
	socket_fake.return_val = -1;
	ASSERT_INT_LT(0, rest__open_server(1337));
	ASSERT_INT_EQ(1, socket_fake.call_count);
	ASSERT_INT_EQ(0, net_reuse_addr_fake.call_count);
	ASSERT_INT_EQ(0, bind_fake.call_count);
	ASSERT_INT_EQ(0, listen_fake.call_count);
	ASSERT_INT_EQ(0, net_dont_block_fake.call_count);
	ASSERT_INT_EQ(0, close_fake.call_count);
	return 0;
}

static int test_open_server__bind_failure(void)
{
	reset_fakes();
	socket_fake.return_val = 42;
	bind_fake.return_val = -1;

	ASSERT_INT_LT(0, rest__open_server(1337));
	ASSERT_INT_EQ(1, socket_fake.call_count);

	ASSERT_INT_EQ(1, net_reuse_addr_fake.call_count);
	ASSERT_INT_EQ(42, net_reuse_addr_fake.arg0_val);

	ASSERT_INT_EQ(1, bind_fake.call_count);
	ASSERT_INT_EQ(42, bind_fake.arg0_val);

	ASSERT_INT_EQ(0, listen_fake.call_count);
	ASSERT_INT_EQ(0, net_dont_block_fake.call_count);
	ASSERT_INT_EQ(1, close_fake.call_count);

	return 0;
}

static int test_open_server__listen_failure(void)
{
	reset_fakes();
	socket_fake.return_val = 42;
	listen_fake.return_val = -1;

	ASSERT_INT_LT(0, rest__open_server(1337));
	ASSERT_INT_EQ(1, socket_fake.call_count);

	ASSERT_INT_EQ(1, net_reuse_addr_fake.call_count);
	ASSERT_INT_EQ(42, net_reuse_addr_fake.arg0_val);

	ASSERT_INT_EQ(1, bind_fake.call_count);
	ASSERT_INT_EQ(42, bind_fake.arg0_val);

	ASSERT_INT_EQ(1, listen_fake.call_count);
	ASSERT_INT_EQ(42, listen_fake.arg0_val);

	ASSERT_INT_EQ(0, net_dont_block_fake.call_count);
	ASSERT_INT_EQ(1, close_fake.call_count);

	return 0;
}

static int test_open_server__success(void)
{
	reset_fakes();
	socket_fake.return_val = 42;

	ASSERT_INT_EQ(42, rest__open_server(1337));
	ASSERT_INT_EQ(1, socket_fake.call_count);

	ASSERT_INT_EQ(1, net_reuse_addr_fake.call_count);
	ASSERT_INT_EQ(42, net_reuse_addr_fake.arg0_val);

	ASSERT_INT_EQ(1, bind_fake.call_count);
	ASSERT_INT_EQ(42, bind_fake.arg0_val);

	ASSERT_INT_EQ(1, listen_fake.call_count);
	ASSERT_INT_EQ(42, listen_fake.arg0_val);

	ASSERT_INT_EQ(1, net_dont_block_fake.call_count);
	ASSERT_INT_EQ(42, net_dont_block_fake.arg0_val);

	ASSERT_INT_EQ(0, close_fake.call_count);

	return 0;
}

static int test_read__empty(void)
{
	reset_fakes();
	read_fake.return_val = -1;
	errno = EAGAIN;
	struct vector vec;
	ASSERT_INT_LT(0, rest__read(&vec, 42));
	return 0;
}

static int test_read__closed(void)
{
	reset_fakes();
	read_fake.return_val = 0;
	struct vector vec;
	ASSERT_INT_EQ(0, rest__read(&vec, 42));
	return 0;
}

static ssize_t read_test_custom(int fd, void* buf, size_t count)
{
	switch (read_fake.call_count) {
	case 1: strcpy(buf, "foo"); return 3;
	case 2: strcpy(buf, "bar"); return 4;
	default:
		errno = EAGAIN;
		return -1;
	}
}

static int test_read__twice(void)
{
	reset_fakes();
	read_fake.custom_fake = read_test_custom;
	struct vector vec;
	vector_init(&vec, 16);
	ASSERT_INT_EQ(0, rest__read(&vec, 42));
	ASSERT_INT_EQ(3, read_fake.call_count);
	ASSERT_UINT_EQ(7, vec.index);
	ASSERT_STR_EQ("foobar", vec.data);
	return 0;
}

int main()
{
	int r = 0;
	RUN_TEST(test_find_service_empty);
	RUN_TEST(test_service_is_match);
	RUN_TEST(test_find_service_one);
	RUN_TEST(test_open_server__socket_failure);
	RUN_TEST(test_open_server__bind_failure);
	RUN_TEST(test_open_server__listen_failure);
	RUN_TEST(test_open_server__success);
	RUN_TEST(test_read__empty);
	RUN_TEST(test_read__closed);
	RUN_TEST(test_read__twice);
	return r;
}
