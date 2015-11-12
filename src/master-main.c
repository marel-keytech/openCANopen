#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <appcbase.h>

#include "socketcan.h"
#include "canopen/master.h"

#define SDO_FIFO_MAX_LENGTH 1024
#define REST_DEFAULT_PORT 9191
#define HEARTBEAT_PERIOD 10000 /* ms */
#define HEARTBEAT_TIMEOUT 1000 /* ms */

#define is_in_range(x, min, max) ((min) <= (x) && (x) <= (max))

const char usage_[] =
"Usage: canopen-master [options] <interface>\n"
"\n"
"Options:\n"
"    -h, --help                Get help.\n"
"    -W, --worker-threads      Set the number of worker threads (default 4).\n"
"    -s, --worker-stack-size   Set worker thread stack size.\n"
"    -j, --job-queue-length    Set length of the job queue (default 64).\n"
"    -S, --sdo-queue-length    Set length of the sdo queue (default 1024).\n"
"    -R, --rest-port           Set TCP port of the rest service (default 9191).\n"
"    -Q, --with-quirks         Try to work with buggy old hardware.\n"
"    -T, --use-tcp             Interface argument is a TCP service address.\n"
"    -n, --range               Set node id range (inclusive) to be managed.\n"
"    -p, --heartbeat-period    Set heartbeat period (default 10000ms).\n"
"    -P, --heartbeat-timeout   Set heartbeat timeout (default 1000ms).\n"
"    -x, --ntimeouts-max       Set maximum number of timeouts (default 0).\n"
"\n"
"Appbase Options:\n"
"    -v, --version             Get version info.\n"
"    -V, --longversion         Get more detailed version info.\n"
"    -i, --instance            Set instance id.\n"
"    -r, --num-of-response     Set maximum skipped heartbeats (default 3).\n"
"    -q, --queue-length        Set mqs queue length.\n"
"    -m, --queue-msg-size      Set mqs queue message size.\n"
"    -e, --realtime-priority   Set the realtime priority of the main process.\n"
"    -c, --nice-level          Set the nice level.\n"
"\n"
"Examples:\n"
"    $ canopen-master can0 -i0\n"
"    $ canopen-master can1 -i1 -R9192\n"
"    $ canopen-master can0 -n65-127\n"
"\n";

static inline int print_usage(FILE* output, int status)
{
	fprintf(output, "%s", usage_);
	return status;
}

static inline int have_help_argument(int argc, char* argv[])
{
	for (int i = 1; i < argc; ++i)
		if (strcmp(argv[i], "-h") == 0
		 || strcmp(argv[i], "--help") == 0)
			return 1;

	return 0;
}

static int parse_range(struct co_master_options* opt, char* range)
{
	char* start = range;
	char* stop = strchr(range, '-');
	if (!stop)
		return -1;

	*stop++ = '\0';

	if (!*stop)
		return -1;

	opt->range.start = strtoul(start, NULL, 0);
	opt->range.stop = strtoul(stop, NULL, 0);

	if (!is_in_range(opt->range.start, 1, 127)
	 || !is_in_range(opt->range.stop, 1, 127))
		return -1;

	return 0;
}

int main(int argc, char* argv[])
{
	int rc = 0;

	/* Override appbase help */
	if (have_help_argument(argc, argv))
		return print_usage(stdout, 0);

	if (appbase_cmdline(&argc, argv) != 0)
		return 1;

	struct co_master_options mopt = {
		.nworkers = 4,
		.worker_stack_size = 0,
		.job_queue_length = 64,
		.sdo_queue_length = SDO_FIFO_MAX_LENGTH,
		.rest_port = REST_DEFAULT_PORT,
		.heartbeat_period = HEARTBEAT_PERIOD,
		.heartbeat_timeout = HEARTBEAT_TIMEOUT,
		.flags = 0
	};

	static const struct option long_options[] = {
		{ "worker-threads",    required_argument, 0, 'W' },
		{ "worker-stack-size", required_argument, 0, 's' },
		{ "job-queue-length",  required_argument, 0, 'j' },
		{ "sdo-queue-length",  required_argument, 0, 'S' },
		{ "rest-port",         required_argument, 0, 'R' },
		{ "with-quirks",       no_argument,       0, 'Q' },
		{ "use-tcp",           no_argument,       0, 'T' },
		{ "range",             required_argument, 0, 'n' },
		{ "heartbeat-period",  required_argument, 0, 'p' },
		{ "heartbeat-timeout", required_argument, 0, 'P' },
		{ "ntimeouts-max",     required_argument, 0, 'x' },
		{ 0, 0, 0, 0 }
	};

	while (1) {
		int c = getopt_long(argc, argv, "W:s:j:S:R:QTn:p:P:x:",
				    long_options, NULL);
		if (c < 0)
			break;

		switch (c) {
		case 'W': mopt.nworkers = atoi(optarg); break;
		case 's': mopt.worker_stack_size = strtoul(optarg, NULL, 0);
			  break;
		case 'j': mopt.job_queue_length = strtoul(optarg, NULL, 0);
			  break;
		case 'S': mopt.sdo_queue_length = strtoul(optarg, NULL, 0);
			  break;
		case 'p': mopt.heartbeat_period = strtoul(optarg, NULL, 0);
			  break;
		case 'P': mopt.heartbeat_timeout = strtoul(optarg, NULL, 0);
			  break;
		case 'x': mopt.ntimeouts_max = strtoul(optarg, NULL, 0); break;
		case 'R': mopt.rest_port = atoi(optarg); break;
		case 'Q': mopt.flags |= CO_MASTER_OPTION_WITH_QUIRKS; break;
		case 'T': mopt.flags |= CO_MASTER_OPTION_USE_TCP; break;
		case 'n': if (parse_range(&mopt, optarg) < 0)
				  return print_usage(stderr, 1);
			  break;
		case 'h': return print_usage(stdout, 0);
		case '?': break;
		default:  return print_usage(stderr, 1);
		}
	}

	int nargs = argc - optind;
	char** args = &argv[optind];

	if (nargs < 1)
		return print_usage(stderr, 1);

	mopt.iface = args[0];

	return co_master_run(&mopt) == 0 ? 0 : 1;
}
