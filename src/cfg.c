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

#include "cfg.h"
#include "ini_parser.h"
#include "canopen/master.h"

#ifndef CFG_PATH
#define CFG_PATH "/etc/canopen.cfg"
#endif

static int cfg__is_initialised = 0;
static struct ini_file ini;

int cfg__load_stream(FILE* stream)
{
	int r = ini_parse(&ini, stream);
	if (r >= 0)
		cfg__is_initialised = 1;
	return r;
}

int cfg__load_file(const char* path)
{
	int r = -1;

	FILE* stream = fopen(path, "r");
	if (!stream)
		return -1;

	if (cfg__load_stream(stream) < 0)
		goto failure;

	r = 0;
failure:
	fclose(stream);
	return r;
}

int cfg_load(void)
{
	return cfg__load_file(CFG_PATH);
}

void cfg_unload(void)
{
	if (!cfg__is_initialised)
		return;

	ini_destroy(&ini);
	cfg__is_initialised = 0;
}

const char* cfg__get_by_nodeid(int nodeid, const char* key)
{
	char section[256];
	snprintf(section, sizeof(section) - 1, "#%d", nodeid);
	section[sizeof(section) - 1] = '\0';

	return ini_find(&ini, section, key);
}

const char* cfg__get_by_name(int nodeid, const char* key)
{
	struct co_master_node* node = co_master_get_node(nodeid);
	if (!node)
		return NULL;

	char section[256];
	snprintf(section, sizeof(section) - 1, "=%s", node->name);
	section[sizeof(section) - 1] = '\0';

	return ini_find(&ini, section, key);
}

const char* cfg__get_string(int nodeid, const char* key)
{
	if (!cfg__is_initialised)
		return NULL;

     	const char* result = NULL;

	result = cfg__get_by_nodeid(nodeid, key);
	if (result) goto done;

	result = cfg__get_by_name(nodeid, key);
	if (result) goto done;

	result = ini_find(&ini, "all", key);
	if (result) goto done;

done:
	return result;
}

