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

#define EXPORT __attribute__((visibility("default")))

#define XSTR(s) STR(s)
#define STR(s) #s

static int cfg__is_initialised = 0;
static struct ini_file ini;

size_t strlcpy(char*, const char*, size_t);

EXPORT
struct cfg cfg;

EXPORT
void cfg_load_defaults(void)
{
#define X(type, name, default_) CFG__SET_(type, cfg.name, default_);
	CFG__PARAMETERS
#undef X

}

void cfg__load_node_defaults(int id)
{
#define X(type, name, default_) CFG__SET_(type, cfg.node[id].name, default_);
	CFG__NODE_PARAMETERS
#undef X
}

void cfg__load_node_config(int id)
{
	const char* v;
#define X(type, name, default_) \
		v = cfg__file_read(id, XSTR(name)); \
		if (v) { \
			CFG__SET_(type, cfg.node[id].name, CFG__STRTO_(type, v)); \
		}

	CFG__NODE_PARAMETERS
#undef X
}

void cfg_load_node(int id)
{
	cfg__load_node_defaults(id);
	cfg__load_node_config(id);

	if (cfg.be_strict)
		cfg.node[id].ignore_sdo_multiplexer = 0;

	if (cfg.heartbeat_period)
		cfg.node[id].heartbeat_period = cfg.heartbeat_period;

	if (cfg.heartbeat_timeout)
		cfg.node[id].heartbeat_timeout = cfg.heartbeat_timeout;

	if (cfg.n_timeouts_max)
		cfg.node[id].n_timeouts_max = cfg.n_timeouts_max;
}

EXPORT
void cfg_load_globals(void)
{
	const char* v;

	const struct ini_section* section = ini_find_section(&ini, "master");
	if (!section)
		return;

#define X(type, name, default_) \
		v = ini_find_key(section, XSTR(name)); \
		if (v) { \
			CFG__SET_(type, cfg.name, CFG__STRTO_(type, v)); \
		}

	CFG__PARAMETERS
#undef X
}

int cfg__load_stream(FILE* stream)
{
	int r = ini_parse(&ini, stream);
	if (r >= 0)
		cfg__is_initialised = 1;
	return r;
}

EXPORT
int cfg_load_file(const char* path)
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

EXPORT
void cfg_unload_file(void)
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

const char* cfg__file_read(int nodeid, const char* key)
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

