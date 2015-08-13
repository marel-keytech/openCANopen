#ifndef _TST_H
#define _TST_H


#include <stdio.h>
#include <string.h>

#define xstr(s) str(s)
#define str(s) #s

#define ASSERT_TRUE(expr) do { \
	if (!(expr)) { \
		fprintf(stderr, "FAILED " xstr(__LINE__) ": Expected " xstr(expr) " to be true\n"); \
		return 1; \
	} \
} while(0)

#define ASSERT_FALSE(expr) do { \
	if (expr) { \
		fprintf(stderr, "FAILED " xstr(__LINE__) ": Expected " xstr(expr) " to be false\n"); \
		return 1; \
	} \
} while(0)

#define TST_ASSERT_EQ_(value, expr, type, fmt) do { \
	type expr_ = (expr); \
	if (expr_ != (value)) { \
		fprintf(stderr, "FAILED " xstr(__LINE__) ": Expected " xstr(expr) " to be equal to " xstr(value) "; was " fmt "\n", \
			expr_); \
		return 1; \
	} \
} while(0)

#define ASSERT_INT_EQ(value, expr) TST_ASSERT_EQ_(value, expr, int, "%d")
#define ASSERT_UINT_EQ(value, expr) TST_ASSERT_EQ_(value, expr, unsigned int, "%d")
#define ASSERT_DOUBLE_EQ(value, expr) TST_ASSERT_EQ_(value, expr, double, "%f")
#define ASSERT_PTR_EQ(value, expr) TST_ASSERT_EQ_(value, expr, void*, "%p")

#define TST_ASSERT_GT_(value, expr, type, fmt) do { \
	type expr_ = (expr); \
	if (!(expr_ > (value))) { \
		fprintf(stderr, "FAILED " xstr(__LINE__) ": Expected " xstr(expr) " to be greater than " xstr(value) "; was " fmt "\n", \
			expr_); \
		return 1; \
	} \
} while(0)

#define ASSERT_INT_GT(value, expr) TST_ASSERT_GT_(value, expr, int, "%d")
#define ASSERT_UINT_GT(value, expr) TST_ASSERT_GT_(value, expr, unsigned int, "%d")
#define ASSERT_DOUBLE_GT(value, expr) TST_ASSERT_GT_(value, expr, double, "%f")

#define TST_ASSERT_GE_(value, expr, type, fmt) do { \
	type expr_ = (expr); \
	if (!(expr_ >= (value))) { \
		fprintf(stderr, "FAILED " xstr(__LINE__) ": Expected " xstr(expr) " to be greater than or equal to " xstr(value) "; was " fmt "\n", \
			expr_); \
		return 1; \
	} \
} while(0)

#define ASSERT_INT_GE(value, expr) TST_ASSERT_GE_(value, expr, int, "%d")
#define ASSERT_UINT_GE(value, expr) TST_ASSERT_GE_(value, expr, unsigned int, "%d")
#define ASSERT_DOUBLE_GE(value, expr) TST_ASSERT_GE_(value, expr, double, "%f")

#define TST_ASSERT_LT_(value, expr, type, fmt) do { \
	type expr_ = (expr); \
	if (!(expr_ < (value))) { \
		fprintf(stderr, "FAILED " xstr(__LINE__) ": Expected " xstr(expr) " to be less than " xstr(value) "; was " fmt "\n", \
			expr_); \
		return 1; \
	} \
} while(0)

#define ASSERT_INT_LT(value, expr) TST_ASSERT_LT_(value, expr, int, "%d")
#define ASSERT_UINT_LT(value, expr) TST_ASSERT_LT_(value, expr, unsigned int, "%d")
#define ASSERT_DOUBLE_LT(value, expr) TST_ASSERT_LT_(value, expr, double, "%f")

#define TST_ASSERT_LE_(value, expr, type, fmt) do { \
	type expr_ = (expr); \
	if (!(expr_ <= (value))) { \
		fprintf(stderr, "FAILED " xstr(__LINE__) ": Expected " xstr(expr) " to be less than or equal to " xstr(value) "; was " fmt "\n", \
			expr_); \
		return 1; \
	} \
} while(0)

#define ASSERT_INT_LE(value, expr) TST_ASSERT_LE_(value, expr, int, "%d")
#define ASSERT_UINT_LE(value, expr) TST_ASSERT_LE_(value, expr, unsigned int, "%d")
#define ASSERT_DOUBLE_LE(value, expr) TST_ASSERT_LE_(value, expr, double, "%f")

#define ASSERT_STR_EQ(value, expr) do { \
	const char* expr_ = (expr); \
	if (strcmp(expr_, (value)) != 0) { \
		fprintf(stderr, "FAILED " xstr(__LINE__) ": Expected " xstr(expr) " to be " xstr(value) "; was \"%s\"\n", \
			expr_); \
		return 1; \
	} \
} while(0)

#define ASSERT_NEQ(value, expr) do { \
	if ((expr) != (value)) { \
		fprintf(stderr, "FAILED " xstr(__LINE__) ": Expected " xstr(expr) " to NOT be " xstr(value) "\n"); \
		return 1; \
	} \
} while(0)

#define ASSERT_STR_NEQ(value, expr) do { \
	if (strcmp((expr), (value)) == 0) { \
		fprintf(stderr, "FAILED " xstr(__LINE__) ": Expected " xstr(expr) " to NOT be " xstr(value) "\n"); \
		return 1; \
	} \
} while(0)

#define RUN_TEST(test) do { \
	if(!(test())) \
		fprintf(stderr, xstr(test) " passed\n"); \
	else \
		r = 1; \
} while(0);



#endif /* _TST_H */

