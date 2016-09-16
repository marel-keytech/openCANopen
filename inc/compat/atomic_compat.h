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

#ifndef ATOMIC_COMPAT_H_
#define ATOMIC_COMPAT_H_

#ifndef __ATOMIC_SEQ_CST
#define __ATOMIC_SEQ_CST

#define __atomic_compare_exchange_n(ptr, expected_ptr, desired, ...) \
	__sync_bool_compare_and_swap(ptr, *(expected_ptr), desired)

#define __atomic_load_n(ptr, ...) \
({ \
	__sync_synchronize(); \
	__typeof__(*ptr) result = *(ptr); \
	__sync_synchronize(); \
	result; \
})

#define __atomic_store_n(ptr, val, ...) \
({ \
	__sync_synchronize(); \
	*(ptr) = val; \
	__sync_synchronize(); \
})


#define __atomic_sub_fetch(ptr, value, ...) \
	__sync_sub_and_fetch(ptr, value)

#define __atomic_add_fetch(ptr, value, ...) \
	__sync_add_and_fetch(ptr, value)
#endif

#endif /* ATOMIC_COMPAT_H_ */
