#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <mloop.h>

#include "vnode.h"

const char usage_[] =
"Usage: canopen-vnode [options] <interface> <nodeid> [nodeid] [...]\n"
"\n"
"Options:\n"
"    -h, --help                 Get help.\n"
"    -T, --tcp                  Connect via TCP.\n"
"    -c, --config               Set path to config file.\n"
"\n"
"Examples:\n"
"    $ canopen-vnode can0\n"
"    $ canopen-vnode -T 127.0.0.1\n"
"\n";

static struct vnode* node[127];

static inline int print_usage(FILE* output, int status)
{
	fprintf(output, "%s", usage_);
	return status;
}

int init_nodes(enum sock_type type, const char* config, const char* iface,
	       char* ids[], int n_ids)
{
	memset(node, 0, sizeof(node));

	int i;
	for (i = 0; i < n_ids; ++i) {
		int nodeid = atoi(ids[i]);

		struct vnode* vnode = co_vnode_new(type, iface, config, nodeid);
		if (!vnode)
			goto failure;

		node[nodeid - 1] = vnode;
	}

	return 0;

failure:
	for (--i; i >= 0; --i) {
		int nodeid = atoi(ids[i]);
		co_vnode_destroy(node[nodeid - 1]);
	}

	return -1;
}

void destroy_nodes(void)
{
	for (int i = 0; i < 127; ++i)
		if (node[i])
			co_vnode_destroy(node[i]);
}

int main(int argc, char* argv[])
{
	static const struct option long_options[] = {
		{ "help",   no_argument,       0, 'h' },
		{ "tcp",    no_argument,       0, 'T' },
		{ "config", required_argument, 0, 'c' },
		{ 0, 0, 0, 0 }
	};

	int use_tcp = 0;
	const char* config = NULL;

	while (1) {
		int c = getopt_long(argc, argv, "hTc:", long_options, NULL);
		if (c < 0)
			break;

		switch (c) {
		case 'h': return print_usage(stdout, 0);
		case 'T': use_tcp = 1; break;
		case 'c': config = optarg; break;
		}
	}

	int nargs = argc - optind;
	char** args = &argv[optind];

	if (nargs < 2)
		return print_usage(stderr, 1);

	const char* iface = args[0];

	enum sock_type type = use_tcp ? SOCK_TYPE_TCP : SOCK_TYPE_CAN;

	struct mloop* mloop = mloop_default();
	mloop_ref(mloop);

	if (init_nodes(type, config, iface, &args[1], nargs - 1) < 0)
		return 1;

	mloop_run(mloop);

	destroy_nodes();
	mloop_unref(mloop);
	return 0;
}
