#ifndef KV_H_INCLUDED_
#define KV_H_INCLUDED_

#include <sys/types.h>

struct kv;

struct kv* kv_new(const char* key, const char* value);
void kv_del(struct kv* self);

struct kv* kv_copy(const struct kv* other);

const char* kv_key(const struct kv* self);
const char* kv_value(const struct kv* self);

size_t kv_raw_size(const struct kv* self);
void kv_raw_copy(void* dst, const struct kv* self, size_t max_size);

#endif /* KV_H_INCLUDED_ */

