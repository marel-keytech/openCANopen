#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#include "tst.h"
#include "ini_parser.h"

FILE* rstream;
FILE* wstream;

static void setup()
{
	int fds[2];
	pipe(fds);
	fcntl(fds[0], F_SETFL, fcntl(fds[0], F_GETFL, 0) | O_NONBLOCK);
	fcntl(fds[1], F_SETFL, fcntl(fds[1], F_GETFL, 0) | O_NONBLOCK);
	rstream = fdopen(fds[0], "r");
	wstream = fdopen(fds[1], "w");
	setvbuf(rstream, NULL, _IONBF, 0);
	setvbuf(wstream, NULL, _IONBF, 0);
}

static void cleanup()
{
	fclose(rstream);
	fclose(wstream);
}

static int test_simple_file()
{
	const char* text =
	"; a simple test file\n"
	"\n"
	"[Section]\n"
	"Foo=bar\n"
	"x=y";

	fprintf(wstream, "%s", text);
	fflush(wstream);

	struct ini_file file;
	ASSERT_INT_EQ(0, ini_parse(&file, rstream));

	ASSERT_STR_EQ("bar", ini_find(&file, "section", "foo"));
	ASSERT_STR_EQ("y", ini_find(&file, "section", "x"));

	ini_destroy(&file);

	return 0;
}

int main()
{
    int r = 0;
    setup();

    RUN_TEST(test_simple_file);

    cleanup();
    return r;
}

