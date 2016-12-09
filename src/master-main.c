/* Copyright (c) 2014-2016, Marel
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#ifndef NO_MAREL_CODE
#include <appcbase.h>
#endif

#include "socketcan.h"
#include "canopen/master.h"
#include "cfg.h"

#ifndef CFG_FILE_PATH
#define CFG_FILE_PATH "/etc/canopen.cfg"
#endif

#define is_in_range(x, min, max) ((min) <= (x) && (x) <= (max))

size_t strlcpy(char*, const char*, size_t);

const char usage_[] =
"Usage: canopen-master [options] <interface>\n"
"\n"
"Options:\n"
"    -h, --help                Get help.\n"
"    -W, --worker-threads      Set the number of worker threads (default 4).\n"
"    -s, --worker-stack-size   Set worker thread stack size.\n"
"    -j, --job-queue-length    Set length of the job queue (default 256).\n"
"    -S, --sdo-queue-length    Set length of the sdo queue (default 1024).\n"
"    -R, --rest-port           Set TCP port of the rest service (default 9191).\n"
"    -f, --strict              Force strict communication patterns.\n"
"    -T, --use-tcp             Interface argument is a TCP service address.\n"
"    -n, --range               Set node id range (inclusive) to be managed.\n"
"    -p, --heartbeat-period    Set heartbeat period (default 10000ms).\n"
"    -P, --heartbeat-timeout   Set heartbeat timeout (default 1000ms).\n"
"    -x, --ntimeouts-max       Set maximum number of timeouts (default 2).\n"
"\n";

#ifndef NO_MAREL_CODE
const char appbase_usage_[] =
"Appbase Options:\n"
"    -v, --version             Get version info.\n"
"    -V, --longversion         Get more detailed version info.\n"
"    -i, --instance            Set instance id.\n"
"    -r, --num-of-response     Set maximum skipped heartbeats (default 3).\n"
"    -q, --queue-length        Set mqs queue length.\n"
"    -m, --queue-msg-size      Set mqs queue message size.\n"
"    -c, --nice-level          Set the nice level.\n"
"\n"
"Examples:\n"
"    $ canopen-master can0 -i0\n"
"    $ canopen-master can1 -i1 -R9192\n"
"    $ canopen-master can0 -n65-127\n"
"\n";
#endif /* NO_MAREL_CODE */

static inline int print_usage(FILE* output, int status)
{
	fprintf(output, "%s", usage_);

#ifndef NO_MAREL_CODE
	fprintf(output, "%s", appbase_usage_);
#endif

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

static int parse_range(char* range)
{
	char* start = range;
	char* stop = strchr(range, '-');
	if (!stop)
		return -1;

	*stop++ = '\0';

	if (!*stop)
		return -1;

	cfg.range_start = strtoul(start, NULL, 0);
	cfg.range_stop = strtoul(stop, NULL, 0);

	if (!is_in_range(cfg.range_start, 1, 127)
	 || !is_in_range(cfg.range_stop, 1, 127))
		return -1;

	return 0;
}

int main(int argc, char* argv[])
{
	cfg_load_defaults();

	if (cfg_load_file(CFG_FILE_PATH) >= 0)
		cfg_load_globals();

	/* Override appbase help */
	if (have_help_argument(argc, argv))
		return print_usage(stdout, 0);

#ifndef NO_MAREL_CODE
	if (appbase_cmdline(&argc, argv) != 0)
		return 1;
#endif /* NO_MAREL_CODE */

	static const struct option long_options[] = {
		{ "worker-threads",    required_argument, 0, 'W' },
		{ "worker-stack-size", required_argument, 0, 's' },
		{ "job-queue-length",  required_argument, 0, 'j' },
		{ "sdo-queue-length",  required_argument, 0, 'S' },
		{ "rest-port",         required_argument, 0, 'R' },
		{ "strict",            no_argument,       0, 'f' },
		{ "use-tcp",           no_argument,       0, 'T' },
		{ "range",             required_argument, 0, 'n' },
		{ "heartbeat-period",  required_argument, 0, 'p' },
		{ "heartbeat-timeout", required_argument, 0, 'P' },
		{ "ntimeouts-max",     required_argument, 0, 'x' },
		{ 0, 0, 0, 0 }
	};

	while (1) {
		int c = getopt_long(argc, argv, "W:s:j:S:R:fTn:p:P:x:",
				    long_options, NULL);
		if (c < 0)
			break;

		switch (c) {
		case 'W': cfg.n_workers = atoi(optarg); break;
		case 's': cfg.worker_stack_size = strtoul(optarg, NULL, 0);
			  break;
		case 'j': cfg.job_queue_length = strtoul(optarg, NULL, 0);
			  break;
		case 'S': cfg.sdo_queue_length = strtoul(optarg, NULL, 0);
			  break;
		case 'p': cfg.heartbeat_period = strtoul(optarg, NULL, 0);
			  break;
		case 'P': cfg.heartbeat_timeout = strtoul(optarg, NULL, 0);
			  break;
		case 'x': cfg.n_timeouts_max = strtoul(optarg, NULL, 0); break;
		case 'R': cfg.rest_port = atoi(optarg); break;
		case 'f': cfg.be_strict = 1; break;
		case 'T': cfg.use_tcp = 1; break;
		case 'n': if (parse_range(optarg) < 0)
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

	strlcpy(cfg.iface, args[0], sizeof(cfg.iface));

	int r = co_master_run();

	cfg_unload_file();

	return r == 0 ? 0 : 1;
}
