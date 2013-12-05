// Copyright 2010-2012 RethinkDB, all rights reserved.
#ifndef THREAD_LOCAL_HPP_
#define THREAD_LOCAL_HPP_

#ifdef THREADED_COROUTINES
#include <vector>
#include "config/args.hpp"
// For get_thread_id()
#include "arch/runtime/coroutines.hpp"
#endif

#include "errors.hpp"
#include "utils.hpp"

/*
 * We have to make sure that access to thread local storage (TLS) is only performed
 * from functions that cannot be inlined.
 *
 * Consider the following code:
 *     int before = TLS_get_x();
 *     on_thread_t switcher(...);
 *     int after = TLS_get_x();
 *
 * `after` should be the value of `x` on the new thread, and `before` the one on the
 * old thread.
 *
 * Now if the compiler is allowed to inline TLS_get_x(), it will internally generate
 * something like this (pseudocode):
 *     void *__tls_segment = register "%gs";
 *     int *__addr_of_x = __tls_segment + __x_tls_offset;
 *     int before = *__addr_of_x;
 *     on_thread_t switcher(...);
 *     __tls_segment = register "%gs";
 *     _addr_of_x = __tls_segment + __x_tls_offset;
 *     int after = *__addr_of_x;
 *
 * So far so good. The value of %gs will have changed after on_thread_t, and
 * `after` is going to have the value of x on the new thread.
 * Note that I'm using %gs here as the register for the TLS memory region.
 * Other architectures will use different registers (e.g. %fs).
 * Unfortunately, the compiler does not know that %gs can change in the middle
 * of this function, and for GCC as of version 4.8, there doesn't seem to be a
 * way of telling it that it can.
 * So the compiler will look for common subexpressions and optimize them away,
 * making the generated code look more like this:
 *     void *__tls_segment = register "%gs";
 *     int *__addr_of_x = __tls_segment + __x_tls_offset;
 *     int before = *__addr_of_x;
 *     on_thread_t switcher(...);
 *     int after = *__addr_of_x;
 *
 * Now `after` will have the value of x on the *old* thread. This is obviously
 * not correct.
 *
 * Also note that making x volatile is not going to solve this, because
 * we would need the compiler-generated __tls_segment to be volatile.
 *
 * So in essence, we must make sure that any function which accesses TLS directly
 * cannot be inlined, and that it does not use on_thread_t.
 */

#ifndef THREADED_COROUTINES
#define TLS_with_init(type, name, initial)                              \
    static pthread_key_t TLS_ ## name ## _key;                          \
    static pthread_once_t TLS_ ## name ## _key_once;                    \
                                                                        \
    static void TLS_ ## name ## _destructor(void *ptr) {                \
        delete static_cast<type *>(ptr);                                \
    }                                                                   \
                                                                        \
    NOINLINE static void TLS_make_ ## name ## _key() {                  \
        int res = pthread_key_create(& TLS_ ## name ## _key,            \
                                     TLS_ ## name ## _destructor);      \
        guarantee_xerr(res == 0, res, "could not pthread_key_create");  \
    }                                                                   \
                                                                        \
    NOINLINE static void TLS_intialize_ ## name () {                    \
        int res = pthread_once(& TLS_ ## name ## _key_once,             \
                               TLS_make_ ## name ## _key);              \
        guarantee_xerr(res == 0, res, "could not pthread_once");        \
                                                                        \
        if (pthread_getspecific(TLS_ ## name ## _key) == NULL) {        \
            type *ptr = new type;                                       \
            *ptr = initial;                                             \
            res = pthread_setspecific(TLS_ ## name ## _key, ptr);       \
            guarantee_xerr(res == 0, res,                               \
                           "pthread_setspecific failed");               \
        }                                                               \
    }                                                                   \
                                                                        \
    NOINLINE type TLS_get_ ## name () {                                 \
        TLS_intialize_ ## name();                                       \
            return *static_cast<type *>(pthread_getspecific(TLS_ ## name ## _key)); \
    }                                                                   \
                                                                        \
    NOINLINE void TLS_set_ ## name (type const &val) {                  \
        TLS_intialize_ ## name();                                       \
            *static_cast<type *>(pthread_getspecific(TLS_ ## name ## _key)) = val; \
    }

#else  // THREADED_COROUTINES
#define TLS_with_init(type, name, initial)              \
    static std::vector<type> TLS_ ## name(MAX_THREADS, initial); \
                                                        \
    type TLS_get_ ## name () {                          \
        return TLS_ ## name[get_thread_id().threadnum]; \
    }                                                   \
                                                        \
    void TLS_set_ ## name(type const &val) {            \
        TLS_ ## name[get_thread_id().threadnum] = val;  \
    }

#endif  // THREADED_COROUTINES

#ifndef THREADED_COROUTINES
#define TLS(type, name)                                                 \
    static pthread_key_t TLS_ ## name ## _key;                          \
    static pthread_once_t TLS_ ## name ## _key_once;                    \
                                                                        \
    static void TLS_ ## name ## _destructor(void *ptr) {                \
        delete static_cast<type *>(ptr);                                \
    }                                                                   \
                                                                        \
    NOINLINE static void TLS_make_ ## name ## _key() {                  \
        int res = pthread_key_create(& TLS_ ## name ## _key,            \
                                     TLS_ ## name ## _destructor);      \
        guarantee_xerr(res == 0, res, "pthread_key_create failed");     \
    }                                                                   \
                                                                        \
    NOINLINE static void TLS_intialize_ ## name () {                    \
        int res = pthread_once(& TLS_ ## name ## _key_once,             \
                               TLS_make_ ## name ## _key);              \
        guarantee_xerr(res == 0, res, "pthread_once failed");           \
                                                                        \
        if (pthread_getspecific(TLS_ ## name ## _key) == NULL) {        \
            type *ptr = new type;                                       \
            pthread_setspecific(TLS_ ## name ## _key, ptr);             \
        }                                                               \
    }                                                                   \
                                                                        \
    NOINLINE type TLS_get_ ## name () {                                 \
        TLS_intialize_ ## name();                                       \
            return *static_cast<type *>(pthread_getspecific(TLS_ ## name ## _key)); \
    }                                                                   \
                                                                        \
    NOINLINE void TLS_set_ ## name (type const &val) {                  \
        TLS_intialize_ ## name();                                       \
            *static_cast<type *>(pthread_getspecific(TLS_ ## name ## _key)) = val; \
    }

#else // THREADED_COROUTINES
#define TLS(type, name)                                 \
    static type TLS_ ## name[MAX_THREADS];              \
                                                        \
    type TLS_get_ ## name () {                          \
        return TLS_ ## name[get_thread_id().threadnum]; \
    }                                                   \
                                                        \
    void TLS_set_ ## name(type const &val) {            \
        TLS_ ## name[get_thread_id().threadnum] = val;  \
    }

#endif // not THREADED_COROUTINES

#endif /* THREAD_LOCAL_HPP_ */
