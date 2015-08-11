#include "tst.h"
#include "http.h"

int test_get_empty_path()
{
	const char* text = "GET / HTTP/1.1\r\n\r\nasdf";

	struct http_req req;
	ASSERT_INT_EQ(0, http_req_parse(&req, text));

	ASSERT_INT_EQ(HTTP_GET, req.method);
	ASSERT_INT_EQ(0, req.url_index);
	ASSERT_INT_EQ(strlen(text)-4, req.header_length);

	http_req_free(&req);
	return 0;
}

int test_get_one_elem_path()
{
	const char* text = "GET /foo HTTP/1.1\r\n\r\nasdf";

	struct http_req req;
	ASSERT_INT_EQ(0, http_req_parse(&req, text));

	ASSERT_INT_EQ(HTTP_GET, req.method);
	ASSERT_INT_EQ(1, req.url_index);
	ASSERT_STR_EQ("foo", req.url[0]);
	ASSERT_INT_EQ(strlen(text)-4, req.header_length);

	http_req_free(&req);
	return 0;
}

int test_get_two_elem_path()
{
	const char* text = "GET /foo/bar HTTP/1.1\r\n\r\nasdf";

	struct http_req req;
	ASSERT_INT_EQ(0, http_req_parse(&req, text));

	ASSERT_INT_EQ(HTTP_GET, req.method);
	ASSERT_INT_EQ(2, req.url_index);
	ASSERT_STR_EQ("foo", req.url[0]);
	ASSERT_STR_EQ("bar", req.url[1]);
	ASSERT_INT_EQ(strlen(text)-4, req.header_length);

	http_req_free(&req);
	return 0;
}

int test_put_empty_path()
{
	const char* text = "PUT / HTTP/1.1\r\n\r\nasdf";

	struct http_req req;
	ASSERT_INT_EQ(0, http_req_parse(&req, text));

	ASSERT_INT_EQ(HTTP_PUT, req.method);
	ASSERT_INT_EQ(0, req.url_index);
	ASSERT_INT_EQ(strlen(text)-4, req.header_length);

	http_req_free(&req);
	return 0;
}

int test_put_with_content_length()
{
	const char* text =
	"PUT / HTTP/1.1\r\n"
	"Content-Length: 42\r\n"
	"\r\n";

	struct http_req req;
	ASSERT_INT_EQ(0, http_req_parse(&req, text));

	ASSERT_INT_EQ(HTTP_PUT, req.method);
	ASSERT_INT_EQ(0, req.url_index);
	ASSERT_INT_EQ(42, req.content_length);
	ASSERT_INT_EQ(strlen(text), req.header_length);

	http_req_free(&req);
	return 0;
}

int test_get_with_content_type()
{
	const char* text =
	"GET / HTTP/1.1\r\n"
	"Content-Type: foo/bar\r\n"
	"\r\n";

	struct http_req req;
	ASSERT_INT_EQ(0, http_req_parse(&req, text));

	ASSERT_INT_EQ(HTTP_GET, req.method);
	ASSERT_INT_EQ(0, req.url_index);
	ASSERT_STR_EQ("foo/bar", req.content_type);
	ASSERT_INT_EQ(strlen(text), req.header_length);

	http_req_free(&req);
	return 0;
}

int main()
{
	int r = 0;
	RUN_TEST(test_get_empty_path);
	RUN_TEST(test_get_one_elem_path);
	RUN_TEST(test_get_two_elem_path);
	RUN_TEST(test_put_empty_path);
	RUN_TEST(test_put_with_content_length);
	RUN_TEST(test_get_with_content_type);
	return r;
}
