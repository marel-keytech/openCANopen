#include "kv.h"
#include "kv_dict.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#include "sys/tree.h"

#ifndef __unused
#define __unused __attribute__((unused))
#endif

#define MIN_BUFFER_SIZE 4096

struct kv {
	char blob[1];
};

struct kv_dict_iter {
	RB_ENTRY(kv_dict_iter) rbent;
	struct kv kv;
};

RB_HEAD(kv_dict, kv_dict_iter);

static inline const char* kv_dict_iter_get_key(struct kv_dict_iter* node)
{
	return kv_key(&node->kv);
}

static inline const char* kv_dict_iter_get_value(struct kv_dict_iter* node)
{
	return kv_value(&node->kv);
}

static inline int kv_dict_cmp(struct kv_dict_iter* a, struct kv_dict_iter* b)
{
	return strcmp(kv_key(&a->kv), kv_key(&b->kv));
}

RB_GENERATE_STATIC(kv_dict, kv_dict_iter, rbent, kv_dict_cmp);

struct kv_dict* kv_dict_new()
{
	struct kv_dict* self = malloc(sizeof(*self));
	if (!self)
		return NULL;

	memset(self, 0, sizeof(*self));

	return self;
}

void kv_dict_del(struct kv_dict* self)
{
	struct kv_dict_iter *node;

	while ((node = RB_MIN(kv_dict, self)) != NULL) {
		RB_REMOVE(kv_dict, self, node);
		free(node);
	}

	free(self);
}

int kv_dict_insert(struct kv_dict* self, const struct kv* kv)
{
	struct kv_dict_iter* existing;
	struct kv_dict_iter* node = malloc(offsetof(struct kv_dict_iter, kv)
					   + kv_raw_size(kv));
	if (!node)
		return -1;

	kv_raw_copy(&node->kv, kv, kv_raw_size(kv));

	existing = RB_INSERT(kv_dict, self, node);
	if (!existing)
		return 0;

	RB_REMOVE(kv_dict, self, existing);
	free(existing);

	RB_INSERT(kv_dict, self, node);

	return 0;
}

static const struct kv_dict_iter* dict_find_(const struct kv_dict* self,
					     const char* key)
{
	struct kv_dict_iter* elm;
	elm = (struct kv_dict_iter*)(key - offsetof(struct kv_dict_iter, kv));
	return RB_FIND(kv_dict, (struct kv_dict*)self, elm);
}

const struct kv* kv_dict_find(const struct kv_dict* self, const char* key)
{
	const struct kv_dict_iter* node = dict_find_(self, key);
	return node ? &node->kv : NULL;
}

int kv_dict_remove(struct kv_dict* self, const char* key)
{
	struct kv_dict_iter* node = (struct kv_dict_iter*)dict_find_(self, key);
	if (!node)
		return 0;

	RB_REMOVE(kv_dict, self, node);
	free(node);

	return 0;
}

const struct kv_dict_iter* kv_dict_find_ge(const struct kv_dict* self,
					   const char* key)
{
	struct kv_dict_iter* elm;
	struct kv_dict_iter* node;

	elm = (struct kv_dict_iter*)(key - offsetof(struct kv_dict_iter, kv));
	node = RB_NFIND(kv_dict, (struct kv_dict*)self, elm);

	return node;
}

const struct kv_dict_iter* kv_dict_iter_next(const struct kv_dict* self,
					     const struct kv_dict_iter* iter)
{
	return RB_NEXT(kv_dict, (struct kv_dict*)self,
		       (struct kv_dict_iter*)iter);
}

const struct kv* kv_dict_iter_kv(const struct kv_dict_iter* iter)
{
	return &iter->kv;
}

