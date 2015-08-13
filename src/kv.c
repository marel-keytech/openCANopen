#include "kv.h"

#include <stdlib.h>
#include <string.h>

#ifndef MIN
#define MIN(a,b) (a) < (b) ? (a) : (b)
#endif

struct kv {
	char blob[1];
};

struct kv* kv_new(const char* key, const char* value)
{
	size_t key_len = strlen(key);
	size_t value_len = strlen(value);
	struct kv* self = malloc(key_len + value_len + 2);
	if (!self)
		return NULL;
	memcpy(self, key, key_len + 1);
	memcpy(&self->blob[key_len + 1], value, value_len + 1);
	return self;
}

void kv_del(struct kv* self)
{
	free(self);
}

struct kv* kv_copy(const struct kv* other)
{
	struct kv* self = malloc(kv_raw_size(other));
	if (!self)
		return NULL;
	memcpy(self, other, kv_raw_size(other));
	return self;
}

const char* kv_key(const struct kv* self)
{
	return self->blob;
}

const char* kv_value(const struct kv* self)
{
	return &self->blob[strlen(kv_key(self)) + 1];
}

size_t kv_raw_size(const struct kv* self)
{
	return strlen(kv_key(self)) + strlen(kv_value(self)) + 2;
}

void kv_raw_copy(void* dst, const struct kv* self, size_t max_size)
{
	memcpy(dst, self, MIN(kv_raw_size(self), max_size));
}

