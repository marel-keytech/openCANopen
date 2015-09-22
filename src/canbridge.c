#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <mloop.h>

#include "network-bridge.h"

const char usage_[] =
"Usage: canbridge [options] <interface>\n"
"\n"
"Options:\n"
"    -h, --help                 Get help.\n"
"    -L, --listen[=port]        Listen on TCP port. Default 5555.\n"
"    -c, --connect=host[:port]  Connect to TCP server. Default 5555.\n"
"    -C, --create               Try to create a virtual CAN interface.\n"
"\n"
"Examples:\n"
"    $ canbridge can0 --listen=1234\n"
"    $ canbridge vcan0 --connect=127.0.0.1:1234\n"
"\n";

static inline int print_usage(FILE* output, int status)
{
	fprintf(output, "%s", usage_);
	return status;
}


static int try_create(const char* iface)
{
	char buffer[256];
	snprintf(buffer, sizeof(buffer), "ip link add dev %s type vcan", iface);
	buffer[sizeof(buffer) - 1] = '\0';
	return system(buffer);
}

static int try_setup(const char* iface)
{
	char buffer[256];
	snprintf(buffer, sizeof(buffer), "ip link set %s up", iface);
	buffer[sizeof(buffer) - 1] = '\0';
	return system(buffer);
}

static void on_signal_event(struct mloop_signal* sig, int signo)
{
	(void)sig;
	(void)signo;
	mloop_exit(mloop_default());
}

int main(int argc, char* argv[])
{
	static const struct option long_options[] = {
		{ "help",    no_argument,       0, 'h' },
		{ "listen",  optional_argument, 0, 'L' },
		{ "connect", required_argument, 0, 'c' },
		{ "create",  no_argument,       0, 'C' },
		{ 0, 0, 0, 0 }
	};

	const char* listen_ = NULL;
	const char* connect_ = NULL;
	int create = 0;

	while (1) {
		int c = getopt_long(argc, argv, "hL::c:C", long_options, NULL);
		if (c < 0)
			break;

		switch (c) {
		case 'h': return print_usage(stdout, 0);
		case 'L': listen_ = optarg ? optarg : "5555"; break;
		case 'c': connect_ = optarg; break;
		case 'C': create = 1; break;
		}
	}

	int nargs = argc - optind;
	char** args = &argv[optind];

	if (nargs < 1)
		return print_usage(stderr, 1);

	const char* iface = args[0];

	if (listen_ && connect_) {
		fprintf(stderr, "Can't have both listen and connect arguments\n");
		return print_usage(stderr, 1);
	}

	if (!listen_ && !connect_) {
		fprintf(stderr, "You must either listen or connect\n");
		return print_usage(stderr, 1);
	}

	if (create) {
		if (try_create(iface) < 0) {
			perror("Could not create interface");
			return 1;
		}

		if (try_setup(iface) < 0) {
			perror("Could not set up interface");
			return 1;
		}
	}

	struct mloop* mloop = mloop_default();
	mloop_ref(mloop);

	if (listen_) {
		int port = atoi(listen_);
		if (can_network_bridge_server(iface, port) < 0) {
			perror("Could not create interface bridge");
			goto failure;
		}
	} else {
		int port = 5555;
		char* portptr = strchr(connect_, ':');
		if (portptr) {
			*portptr++ = '\0';
			port = atoi(portptr);
		}

		if (can_network_bridge_client(iface, connect_, port) < 0) {
			perror("Could not create interface bridge");
			goto failure;
		}
	}

	sigset_t sigset;
	sigemptyset(&sigset);
	sigaddset(&sigset, SIGINT);
	sigaddset(&sigset, SIGTERM);
	sigaddset(&sigset, SIGHUP);

	pthread_sigmask(SIG_BLOCK, &sigset, NULL);

	struct mloop_signal* sig = mloop_signal_new();
	if (!sig) {
		perror("Could not allocate signal handler");
		return 1;
	}

	mloop_signal_set_signals(sig, &sigset);
	mloop_signal_set_callback(sig, on_signal_event);
	mloop_start_signal(mloop, sig);
	mloop_signal_unref(sig);

	mloop_run(mloop);

failure:
	mloop_unref(mloop);
	return 1;
}
