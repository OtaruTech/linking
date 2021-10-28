/*
 * Copyright (C) 2014 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#ifndef LINKING_CUTILS_ATOMIC_X86_64_H
#define LINKING_CUTILS_ATOMIC_X86_64_H
#include <stdint.h>
#ifndef LINKING_ATOMIC_INLINE
#define LINKING_ATOMIC_INLINE inline __attribute__((always_inline))
#endif
extern LINKING_ATOMIC_INLINE
void linking_compiler_barrier(void)
{
    __asm__ __volatile__ ("" : : : "memory");
}
extern LINKING_ATOMIC_INLINE
void linking_memory_barrier(void)
{
    __asm__ __volatile__ ("mfence" : : : "memory");
}
extern LINKING_ATOMIC_INLINE
int32_t linking_atomic_acquire_load(volatile const int32_t *ptr)
{
    int32_t value = *ptr;
    linking_compiler_barrier();
    return value;
}
extern LINKING_ATOMIC_INLINE
int32_t linking_atomic_release_load(volatile const int32_t *ptr)
{
    linking_memory_barrier();
    return *ptr;
}
extern LINKING_ATOMIC_INLINE
void linking_atomic_acquire_store(int32_t value, volatile int32_t *ptr)
{
    *ptr = value;
    linking_memory_barrier();
}
extern LINKING_ATOMIC_INLINE
void linking_atomic_release_store(int32_t value, volatile int32_t *ptr)
{
    linking_compiler_barrier();
    *ptr = value;
}
extern LINKING_ATOMIC_INLINE
int linking_atomic_cas(int32_t old_value, int32_t new_value,
                       volatile int32_t *ptr)
{
    int32_t prev;
    __asm__ __volatile__ ("lock; cmpxchgl %1, %2"
                          : "=a" (prev)
                          : "q" (new_value), "m" (*ptr), "0" (old_value)
                          : "memory");
    return prev != old_value;
}
extern LINKING_ATOMIC_INLINE
int linking_atomic_acquire_cas(int32_t old_value, int32_t new_value,
                               volatile int32_t *ptr)
{
    /* Loads are not reordered with other loads. */
    return linking_atomic_cas(old_value, new_value, ptr);
}
extern LINKING_ATOMIC_INLINE
int linking_atomic_release_cas(int32_t old_value, int32_t new_value,
                               volatile int32_t *ptr)
{
    /* Stores are not reordered with other stores. */
    return linking_atomic_cas(old_value, new_value, ptr);
}
extern LINKING_ATOMIC_INLINE
int32_t linking_atomic_add(int32_t increment, volatile int32_t *ptr)
{
    __asm__ __volatile__ ("lock; xaddl %0, %1"
                          : "+r" (increment), "+m" (*ptr)
                          : : "memory");
    /* increment now holds the old value of *ptr */
    return increment;
}
extern LINKING_ATOMIC_INLINE
int32_t linking_atomic_inc(volatile int32_t *addr)
{
    return linking_atomic_add(1, addr);
}
extern LINKING_ATOMIC_INLINE
int32_t linking_atomic_dec(volatile int32_t *addr)
{
    return linking_atomic_add(-1, addr);
}
extern LINKING_ATOMIC_INLINE
int32_t linking_atomic_and(int32_t value, volatile int32_t *ptr)
{
    int32_t prev, status;
    do {
        prev = *ptr;
        status = linking_atomic_cas(prev, prev & value, ptr);
    } while (__builtin_expect(status != 0, 0));
    return prev;
}
extern LINKING_ATOMIC_INLINE
int32_t linking_atomic_or(int32_t value, volatile int32_t *ptr)
{
    int32_t prev, status;
    do {
        prev = *ptr;
        status = linking_atomic_cas(prev, prev | value, ptr);
    } while (__builtin_expect(status != 0, 0));
    return prev;
}

#define linking_atomic_write linking_atomic_release_store
#define linking_atomic_cmpxchg linking_atomic_release_cas

#endif /* LINKING_CUTILS_ATOMIC_X86_64_H */
