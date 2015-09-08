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
