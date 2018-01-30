/* Copyright (c) 2014-2017, Marel
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
#include <mloop.h>

#include "canopen/dump.h"

const char usage_[] =
"Usage: canopen-dump [options] <interface>\n"
"\n"
"Options:\n"
"    -h, --help                 Get help.\n"
"    -u, --time                 Show time of arrival.\n"
"    -T, --tcp                  Connect via TCP.\n"
"    -f, --file                 Dump from trace buffer file.\n"
"    -n, --nmt                  Show NMT.\n"
"    -S, --sync                 Show SYNC.\n"
"    -e, --emcy                 Show EMCY.\n"
"    -p, --pdo[=mask]           Show PDO.\n"
"    -s, --sdo                  Show SDO.\n"
"    -H, --heartbeat            Show heartbeat.\n"
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

unsigned int apply_pdo_option(const char* arg)
{
	return arg
	     ? (strtoul(arg, NULL, 0) << CO_DUMP_PDO_FILTER_SHIFT) & CO_DUMP_FILTER_PDO
	     : CO_DUMP_FILTER_PDO;
}

int main(int argc, char* argv[])
{
	static const struct option long_options[] = {
		{ "help",      no_argument,       0, 'h' },
		{ "time",      no_argument,       0, 'u' },
		{ "tcp",       no_argument,       0, 'T' },
		{ "file",      no_argument,       0, 'f' },
		{ "nmt",       no_argument,       0, 'n' },
		{ "sync",      no_argument,       0, 'S' },
		{ "emcy",      no_argument,       0, 'e' },
		{ "pdo",       optional_argument, 0, 'p' },
		{ "sdo",       no_argument,       0, 's' },
		{ "heartbeat", no_argument,       0, 'H' },
		{ 0, 0, 0, 0 }
	};

	enum co_dump_options opt = 0;

	while (1) {
		int c = getopt_long(argc, argv, "huTfnSepsiH", long_options, NULL);
		if (c < 0)
			break;

		switch (c) {
		case 'h': return print_usage(stdout, 0);
		case 'u': opt |= CO_DUMP_TIMESTAMP; break;
		case 'T': opt |= CO_DUMP_TCP; break;
		case 'f': opt |= CO_DUMP_FILE; break;
		case 'n': opt |= CO_DUMP_FILTER_NMT; break;
		case 'S': opt |= CO_DUMP_FILTER_SYNC; break;
		case 'e': opt |= CO_DUMP_FILTER_EMCY; break;
		case 'p': opt |= apply_pdo_option(optarg); break;
		case 's': opt |= CO_DUMP_FILTER_SDO; break;
		case 'H': opt |= CO_DUMP_FILTER_HEARTBEAT; break;
		default: return print_usage(stderr, 1);
		}
	}

	int nargs = argc - optind;
	char** args = &argv[optind];

	if (nargs < 1)
		return print_usage(stderr, 1);

	const char* iface = args[0];

	setvbuf(stdout, NULL, _IOLBF, 0);

	return co_dump(iface, opt);
}
