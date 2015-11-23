#include <string.h>

#include "tst.h"
#include "rest.h"

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

int main()
{
	int r = 0;
	RUN_TEST(test_find_service_empty);
	RUN_TEST(test_service_is_match);
	RUN_TEST(test_find_service_one);
	return r;
}
