#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <mloop.h>

#include "canopen/dump.h"

const char usage_[] =
"Usage: canopen-dump [options] <interface>\n"
"\n"
"Options:\n"
"    -h, --help                 Get help.\n"
"    -T, --tcp                  Connect via TCP.\n"
"\n"
"Examples:\n"
"    $ canopen-dump can0\n"
"    $ canopen-dump -T 127.0.0.1\n"
"\n";

static inline int print_usage(FILE* output, int status)
{
	fprintf(output, "%s", usage_);
	return status;
}

int main(int argc, char* argv[])
{
	static const struct option long_options[] = {
		{ "help", no_argument, 0, 'h' },
		{ "tcp",  no_argument, 0, 'T' },
		{ 0, 0, 0, 0 }
	};

	enum co_dump_options opt = 0;

	while (1) {
		int c = getopt_long(argc, argv, "hT", long_options, NULL);
		if (c < 0)
			break;

		switch (c) {
		case 'h': return print_usage(stdout, 0);
		case 'T': opt |= CO_DUMP_TCP; break;
		}
	}

	int nargs = argc - optind;
	char** args = &argv[optind];

	if (nargs < 1)
		return print_usage(stderr, 1);

	const char* iface = args[0];

	return co_dump(iface, opt);
}
