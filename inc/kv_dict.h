#ifndef KV_DICT_H_INCLUDED_
#define KV_DICT_H_INCLUDED_

#include "kv.h"

struct kv_dict;
struct kv_dict_iter;

struct kv_dict* kv_dict_new();
void kv_dict_del(struct kv_dict* self);

int kv_dict_insert(struct kv_dict* self, const struct kv* kv);
int kv_dict_remove(struct kv_dict* self, const char* key);

const struct kv* kv_dict_find(const struct kv_dict* self, const char* key);
const struct kv_dict_iter* kv_dict_find_ge(const struct kv_dict* self,
					   const char* key);

const struct kv_dict_iter* kv_dict_iter_next(const struct kv_dict* self,
					     const struct kv_dict_iter* iter);

const struct kv* kv_dict_iter_kv(const struct kv_dict_iter* iter);

#endif /* KV_DICT_H_INCLUDED_ */

