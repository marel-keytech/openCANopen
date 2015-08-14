#include <stdio.h>
#include <stdint.h>
#include <ftw.h>
#include <sys/resource.h>
#include <errno.h>

#include "vector.h"
#include "sys/tree.h"

#include "canopen/eds.h"
#include "ini_parser.h"

#ifndef __unused
#define __unused __attribute__((unused))
#endif

struct eds_obj_node {
	struct eds_obj obj;

	RB_ENTRY(eds_obj_node) rb_entry;
	uint32_t key;
};

RB_HEAD(eds_obj_tree, eds_obj_node);

static inline int eds_obj_cmp(const struct eds_obj_node* o1,
			      const struct eds_obj_node* o2)
{
	uint32_t k1 = o1->key;
	uint32_t k2 = o2->key;

	return k1 < k2 ? -1 : k1 > k2;
}

RB_GENERATE_STATIC(eds_obj_tree, eds_obj_node, rb_entry, eds_obj_cmp);

struct canopen_eds {
	uint32_t vendor;
	uint32_t product;
	uint32_t revision;

	struct eds_obj_tree obj_tree;
};

static inline int eds__get_index(const struct eds_obj_node* node)
{
	return node->key >> 8;
}

static inline int eds__get_subindex(const struct eds_obj_node* node)
{
	return node->key & 0xff;
}

static inline void eds__set_index(struct eds_obj_node* node, int index)
{
	node->key &= ~0xffff00;
	node->key |= index << 8;
}

static inline void eds__set_subindex(struct eds_obj_node* node, int subindex)
{
	node->key &= ~0xff;
	node->key |= subindex;
}

static struct vector eds__db;

static inline struct canopen_eds* eds__db_get(int index)
{
	return &((struct canopen_eds*)eds__db.data)[index];
}

const struct canopen_eds* eds_db_find(int vendor, int product, int revision)
{
	const struct canopen_eds* eds;

	for (int i = 0; i < eds__db.index; ++i) {
		eds = eds__db_get(i);
		if (vendor   > 0 && vendor   == eds->vendor)   continue;
		if (product  > 0 && product  == eds->product)  continue;
		if (revision > 0 && revision == eds->revision) continue;
		return eds;
	}

	return NULL;
}

const struct eds_obj* eds_obj_find(const struct canopen_eds* eds,
				   int index, int subindex)
{
	uint32_t key = (index << 8) | subindex;
	void* cmp = (char*)&key - offsetof(struct eds_obj_node, key);
	return (void*)RB_FIND(eds_obj_tree, (void*)&eds->obj_tree, cmp);
}

static inline int eds__extension_matches(const char* path, const char* ext)
{
	char* ptr = strrchr(path, '.');
	return ptr ? strcmp(ext, ptr) : 0;
}

static inline const char* eds__ini_find(struct kv_dict* ini, const char* key)
{
	const struct kv* kv = kv_dict_find(ini, key);
	return kv ? kv_value(kv) : NULL;
}

static void eds__clear(struct canopen_eds* eds)
{
	struct eds_obj_node* node;
	while (!RB_EMPTY(&eds->obj_tree)) {
		node = RB_MIN(eds_obj_tree, &eds->obj_tree);
		RB_REMOVE(eds_obj_tree, &eds->obj_tree, node);
		free(node);
	}
}

static const char* eds__get_section(const char* str)
{
	static char buffer[256];
	strncpy(buffer, str, sizeof(buffer));
	buffer[sizeof(buffer)-1] = '\0';
	char* end = strchr(buffer, '.');
	if (!end)
		return NULL;
	*end = '\0';
	return buffer;
}

static inline const char* eds__get_subkey(const char* str)
{
	const char* start = strchr(str, '.');
	return start ? start + 1 : NULL;
}

int eds__get_section_index(const char* str)
{
	char buffer[256];
	strncpy(buffer, str, sizeof(buffer));
	buffer[sizeof(buffer)-1] = '\0';

	char* end = strstr(buffer, "sub");
	if (end)
		*end = '\0';

	char* endptr = 0;
	int index = strtoul(buffer, &endptr, 16);
	return *endptr == '\0' ? index : -1;
}

int eds__get_section_subindex(const char* str)
{
	char* start = strstr(str, "sub");
	if (start)
		start += 3;

	char* endptr = 0;
	int subindex = strtoul(start, &endptr, 16);
	return *endptr == '\0' ? subindex : -1;
}

static int eds__convert_obj_tree(struct canopen_eds* eds, struct kv_dict* ini)
{
	const struct kv_dict_iter* it;

	for (it = kv_dict_find_ge(ini, ""); it; it = kv_dict_iter_next(ini, it))
	{
		const struct kv* kv = kv_dict_iter_kv(it);
		const char* key = kv_key(kv);
		const char* value = kv_value(kv);

		const char* section = eds__get_section(key);
		if (!section)
			continue;

		const char* subkey = eds__get_subkey(key);
		if (!subkey)
			continue;

		int index = eds__get_section_index(section);
		if (index < 0)
			continue;

		int subindex = eds__get_section_subindex(section);
		if (subindex < 0)
			continue;

		/* To be continued ... */

	}

	return 0;
}

static int eds__convert_ini(struct kv_dict* ini)
{
	const char* vendor = eds__ini_find(ini, "deviceinfo.vendornumber");
	if (!vendor)
		return -1;

	const char* product = eds__ini_find(ini, "deviceinfo.productnumber");
	if (!product)
		return -1;

	const char* revision = eds__ini_find(ini, "deviceinfo.revisionnumber");
	if (!revision)
		return -1;

	struct canopen_eds eds;
	memset(&eds, 0, sizeof(eds));

	RB_INIT(&eds.obj_tree);

	eds.vendor = strtoul(vendor, NULL, 0);
	eds.product = strtoul(product, NULL, 0);
	eds.revision = strtoul(revision, NULL, 0);

	if (eds__convert_obj_tree(&eds, ini) < 0)
		goto failure;

	if (vector_append(&eds__db, &eds, sizeof(eds)) < 0)
		goto failure;

	return 0;

failure:
	eds__clear(&eds);
	return -1;
}

static int eds__load_file(const char* path)
{
	FILE* file = fopen(path, "r");
	if (!file)
		return -1;

	struct kv_dict* ini = kv_dict_new();
	if (!ini)
		goto failure;

	if (ini_parse(ini, file) < 0)
		goto failure;

	if (eds__convert_ini(ini) < 0)
		goto failure;

	kv_dict_del(ini);
	return 0;

failure:
	kv_dict_del(ini);
	return -1;
}

static int eds__walker(const char* fpath, const struct stat* sb,
		       int type, struct FTW* ftwbuf)
{
	(void)sb;
	(void)ftwbuf;

	if (type != FTW_F)
		return 0;

	if (!eds__extension_matches(fpath, ".eds"))
		return 0;

	eds__load_file(fpath);

	return 0;
}

static inline int eds__get_maxfiles(void)
{
	struct rlimit rlim;
	return getrlimit(RLIMIT_NOFILE, &rlim) >= 0 ? rlim.rlim_cur / 2 : 3;
}

static inline const char* eds__get_path(void)
{
	return "/var/marel/canmaster/eds.d";
}

static inline int eds__load_all_files(void)
{
	return nftw(eds__get_path(), eds__walker, eds__get_maxfiles(),
		    FTW_DEPTH);
}

int eds_db_load(void)
{
	vector_reserve(&eds__db, 16);

	if (eds__load_all_files() < 0)
		goto failure;

	return 0;

failure:
	eds_db_unload();
	return -1;
}

static void eds__db_clear(void)
{
	for (int i = 0; i < eds__db.index; ++i)
		eds__clear(eds__db_get(i));
}

void eds_db_unload(void)
{
	eds__db_clear();
	vector_destroy(&eds__db);
}

