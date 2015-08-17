#include <stdio.h>
#include <stdint.h>
#include <ftw.h>
#include <sys/resource.h>
#include <errno.h>
#include <stddef.h>

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
	for (int i = 0; i < eds__db.index / sizeof(struct canopen_eds); ++i) {
		const struct canopen_eds* eds = eds__db_get(i);
		if (vendor   > 0 && vendor   != eds->vendor)   continue;
		if (product  > 0 && product  != eds->product)  continue;
		if (revision > 0 && revision != eds->revision) continue;
		return eds;
	}

	return NULL;
}

struct eds_obj_node* eds__obj_new()
{
	struct eds_obj_node* obj = malloc(sizeof(*obj));
	if (!obj)
		return 0;

	memset(obj, 0, sizeof(*obj));

	return obj;
}

const struct eds_obj* eds_obj_find(const struct canopen_eds* eds,
				   int index, int subindex)
{
	uint32_t key = (index << 8) | subindex;
	void* cmp = (char*)&key - offsetof(struct eds_obj_node, key);
	return (void*)RB_FIND(eds_obj_tree, (void*)&eds->obj_tree, cmp);
}

void eds_obj_dump(const struct canopen_eds* eds)
{
	struct eds_obj_node* obj;
	RB_FOREACH(obj, eds_obj_tree, (void*)&eds->obj_tree)
		printf("%x %x\n", eds__get_index(obj), eds__get_subindex(obj));
}

static inline int eds__extension_matches(const char* path, const char* ext)
{
	char* ptr = strrchr(path, '.');
	return ptr ? strcmp(ext, ptr) == 0 : 0;
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

int eds__get_section_index(const char* str)
{
	const char* sub = strstr(str, "sub");
	char* end = NULL;
	int index = strtoul(str, &end, 16);
	return (str != end && (*end == '\0' || end == sub)) ? index : -1;
}

unsigned int eds__get_section_subindex(const char* str)
{
	const char* sub = strstr(str, "sub");
	if (!sub || sub[3] == '\0')
		return 0;

	sub += 3;
	char* end = NULL;
	int subindex = strtoul(sub, &end, 16);

	return sub != end && *end == '\0' ? subindex : -1;
}

enum eds_obj_access eds__get_access_type(const char* str)
{
	if (strcmp(str, "ro") == 0) return EDS_OBJ_R;
	if (strcmp(str, "wo") == 0) return EDS_OBJ_W;
	if (strcmp(str, "rw") == 0) return EDS_OBJ_RW;
	if (strcmp(str, "const") == 0) return EDS_OBJ_CONST;
	return 0;
}

static void eds__insert(struct canopen_eds* eds, struct eds_obj_node* node)
{
	struct eds_obj_node* existing;

	existing = RB_INSERT(eds_obj_tree, &eds->obj_tree, node);
	if (!existing)
		return;

	RB_REMOVE(eds_obj_tree, &eds->obj_tree, existing);
	free(existing);

	RB_INSERT(eds_obj_tree, &eds->obj_tree, node);
}

static int eds__convert_obj_tree(struct canopen_eds* eds, struct ini_file* ini)
{
	for (size_t i = 0; i < ini_get_length(ini); ++i) {
		const struct ini_section* section = ini_get_section(ini, i);

		int index = eds__get_section_index(section->section);
		if (index < 0)
			continue;

		int subindex = eds__get_section_subindex(section->section);
		if (subindex < 0)
			continue;

		const char* type = ini_find_key(section, "datatype");
		if (!type)
			continue;

		const char* access = ini_find_key(section, "accesstype");
		if (!access)
			access = "ro";

		struct eds_obj_node* obj = eds__obj_new();
		if (!obj)
			return -1;

		obj->obj.type = strtoul(type, NULL, 0);
		obj->obj.access = eds__get_access_type(access);
		obj->key = (index << 8) | subindex;
		eds__insert(eds, obj);
	}

	return 0;
}

static int eds__convert_ini(struct ini_file* ini)
{
	const char* vendor = ini_find(ini, "deviceinfo", "vendornumber");
	if (!vendor)
		return -1;

	const char* product = ini_find(ini, "deviceinfo", "productnumber");
	if (!product)
		return -1;

	const char* revision = ini_find(ini, "deviceinfo", "revisionnumber");
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

	struct ini_file ini;

	if (ini_parse(&ini, file) < 0)
		goto failure;

	if (eds__convert_ini(&ini) < 0)
		goto failure;

	fclose(file);
	ini_destroy(&ini);
	return 0;

failure:
	fclose(file);
	ini_destroy(&ini);
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
	for (int i = 0; i < eds__db.index / sizeof(struct canopen_eds); ++i)
		eds__clear(eds__db_get(i));
}

void eds_db_unload(void)
{
	eds__db_clear();
	vector_destroy(&eds__db);
}

