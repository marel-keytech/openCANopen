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

#ifndef CO_ATOMIC_H_
#define CO_ATOMIC_H_

#ifdef __ATOMIC_SEQ_CST
#define HAVE_NEW_ATOMICS
#endif

#ifdef HAVE_NEW_ATOMICS

#define co_atomic_cas(ptr, expected, desired) \
({ \
	__typeof__(expected) expected_ = expected; \
	__atomic_compare_exchange_n(ptr, &expected_, desired, 0, \
				    __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); \
})

#define co_atomic_load(ptr) __atomic_load_n(ptr, __ATOMIC_SEQ_CST)

#define co_atomic_store(ptr, value) \
	__atomic_store_n(ptr, value, __ATOMIC_SEQ_CST)

#define co_atomic_sub_fetch(ptr, value) \
	__atomic_sub_fetch(ptr, value, __ATOMIC_SEQ_CST)

#define co_atomic_add_fetch(ptr, value) \
	__atomic_add_fetch(ptr, value, __ATOMIC_SEQ_CST)

#else

#define co_atomic_cas(ptr, expected, desired) \
	__sync_bool_compare_and_swap(ptr, expected, desired)

#define co_atomic_load(ptr) \
({ \
	__sync_synchronize(); \
	__typeof__(*ptr) result = (*ptr); \
	__sync_synchronize(); \
	result; \
})

#define co_atomic_store(ptr, value) \
({ \
	__sync_synchronize(); \
	*ptr = value; \
	__sync_synchronize(); \
})

#define co_atomic_sub_fetch(ptr, value) __sync_sub_and_fetch(ptr, value)
#define co_atomic_add_fetch(ptr, value) __sync_add_and_fetch(ptr, value)

#endif /* HAVE_NEW_ATOMICS */

#undef HAVE_NEW_ATOMICS

#endif /* CO_ATOMIC_H_ */
