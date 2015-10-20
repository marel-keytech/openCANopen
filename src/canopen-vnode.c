#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <mloop.h>

#include "vnode.h"

const char usage_[] =
"Usage: canopen-vnode [options] <interface> <nodeid>\n"
"\n"
"Options:\n"
"    -h, --help                 Get help.\n"
"    -T, --tcp                  Connect via TCP.\n"
"\n"
"Examples:\n"
"    $ canopen-vnode can0\n"
"    $ canopen-vnode -T 127.0.0.1\n"
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

	int use_tcp = 0;

	while (1) {
		int c = getopt_long(argc, argv, "hT", long_options, NULL);
		if (c < 0)
			break;

		switch (c) {
		case 'h': return print_usage(stdout, 0);
		case 'T': use_tcp = 1; break;
		}
	}

	int nargs = argc - optind;
	char** args = &argv[optind];

	if (nargs < 2)
		return print_usage(stderr, 1);

	const char* iface = args[0];
	const char* nodeidstr = args[1];

	int nodeid = atoi(nodeidstr);

	enum sock_type type = use_tcp ? SOCK_TYPE_TCP : SOCK_TYPE_CAN;

	struct mloop* mloop = mloop_default();
	mloop_ref(mloop);

	if (co_vnode_init(type, iface, NULL, nodeid) < 0)
		return 1;

	mloop_run(mloop);

	co_vnode_destroy();
	mloop_unref(mloop);
	return 0;
}
