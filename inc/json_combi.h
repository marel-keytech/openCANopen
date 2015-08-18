#ifndef JSON_COMBI_H_
#define JSON_COMBI_H_

#include <stdio.h>
#include <stdint.h>
#include "nargs.h"
#include "vector.h"

#define SPLICE_2(a,b) a##b
#define SPLICE_1(a,b) SPLICE_2(a,b)
#define SPLICE(a,b) SPLICE_1(a,b)

#define SEPARATE vector_append(&v_, ",", 1)

#define ENUMERATE_0(...)
#define ENUMERATE_1(head) head
#define ENUMERATE_2(head, ...) head, SEPARATE, ENUMERATE_1(__VA_ARGS__)
#define ENUMERATE_3(head, ...) head, SEPARATE, ENUMERATE_2(__VA_ARGS__)
#define ENUMERATE_4(head, ...) head, SEPARATE, ENUMERATE_3(__VA_ARGS__)
#define ENUMERATE_5(head, ...) head, SEPARATE, ENUMERATE_4(__VA_ARGS__)
#define ENUMERATE_6(head, ...) head, SEPARATE, ENUMERATE_5(__VA_ARGS__)
#define ENUMERATE_7(head, ...) head, SEPARATE, ENUMERATE_6(__VA_ARGS__)
#define ENUMERATE_8(head, ...) head, SEPARATE, ENUMERATE_7(__VA_ARGS__)
#define ENUMERATE_9(head, ...) head, SEPARATE, ENUMERATE_8(__VA_ARGS__)
#define ENUMERATE_(N, ...) SPLICE(ENUMERATE_, N)(__VA_ARGS__)
#define ENUMERATE(...) ENUMERATE_(PP_NARG(__VA_ARGS__), __VA_ARGS__)

#define QUOTE(x) ({ \
	vector_append(&v_, "\"", 1); \
	x; \
	vector_append(&v_, "\"", 1); \
})

#define json_int(name, value) ({ \
	char b_[32]; \
	sprintf(b_, "%lld", (int64_t)value); \
	QUOTE(vector_append(&v_, name, strlen(name))); \
	vector_append(&v_, "=", 1); \
	vector_append(&v_, b_, strlen(b_)); \
})

#define json_uint(name, value) ({ \
	char b_[32]; \
	sprintf(b_, "%llu", (uint64_t)value); \
	QUOTE(vector_append(&v_, name, strlen(name))); \
	vector_append(&v_, "=", 1); \
	vector_append(&v_, b_, strlen(b_)); \
})

#define json_real(name, value) ({ \
	char b_[32]; \
	sprintf(b_, "%le", (double)value); \
	QUOTE(vector_append(&v_, name, strlen(name))); \
	vector_append(&v_, "=", 1); \
	vector_append(&v_, b_, strlen(b_)); \
})

#define json_str(name, value) ({ \
	QUOTE(vector_append(&v_, name, strlen(name))); \
	vector_append(&v_, "=", 1); \
	QUOTE(vector_append(&v_, value, strlen(value))); \
})

#define json_array(name, ...) ({ \
	QUOTE(vector_append(&v_, name, strlen(name))); \
	vector_append(&v_, "=[", 2); \
	ENUMERATE(__VA_ARGS__); \
	vector_append(&v_, "]", 1); \
})

#define json_obj(name, ...) ({ \
	QUOTE(vector_append(&v_, name, strlen(name))); \
	vector_append(&v_, "={", 2); \
	ENUMERATE(__VA_ARGS__); \
	vector_append(&v_, "}", 1); \
})

#define json_root(...) ({ \
	struct vector v_; \
	vector_init(&v_, 256); \
	vector_append(&v_, "{", 1); \
	ENUMERATE(__VA_ARGS__); \
	vector_append(&v_, "}", 2); \
	(char*)v_.data; \
})

#endif /* JSON_COMBI_H_ */
