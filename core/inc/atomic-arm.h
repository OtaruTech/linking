/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef LINKING_CUTILS_ATOMIC_ARM_H
#define LINKING_CUTILS_ATOMIC_ARM_H

#include <stdint.h>

#ifndef LINKING_ATOMIC_INLINE
#define LINKING_ATOMIC_INLINE inline __attribute__((always_inline))
#endif

#define linking_atomic_cmpxchg linking_atomic_release_cas

extern LINKING_ATOMIC_INLINE void linking_compiler_barrier()
{
    __asm__ __volatile__ ("" : : : "memory");
}

extern LINKING_ATOMIC_INLINE void linking_memory_barrier()
{
#if LINKING_SMP == 0
    linking_compiler_barrier();
#else
    __asm__ __volatile__ ("dmb" : : : "memory");
#endif
}

extern LINKING_ATOMIC_INLINE void linking_memory_store_barrier()
{
#if LINKING_SMP == 0
    linking_compiler_barrier();
#else
    __asm__ __volatile__ ("dmb st" : : : "memory");
#endif
}

extern LINKING_ATOMIC_INLINE
int32_t linking_atomic_acquire_load(volatile const int32_t *ptr)
{
    int32_t value = *ptr;
    linking_memory_barrier();
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
    linking_memory_barrier();
    *ptr = value;
}

extern LINKING_ATOMIC_INLINE
int linking_atomic_cas(int32_t old_value, int32_t new_value,
                       volatile int32_t *ptr)
{
    int32_t prev, status;
    do {
        __asm__ __volatile__ ("ldrex %0, [%3]\n"
                              "mov %1, #0\n"
                              "teq %0, %4\n"
#ifdef __thumb2__
                              "it eq\n"
#endif
                              "strexeq %1, %5, [%3]"
                              : "=&r" (prev), "=&r" (status), "+m"(*ptr)
                              : "r" (ptr), "Ir" (old_value), "r" (new_value)
                              : "cc");
    } while (__builtin_expect(status != 0, 0));
    return prev != old_value;
}

extern LINKING_ATOMIC_INLINE
int linking_atomic_acquire_cas(int32_t old_value, int32_t new_value,
                               volatile int32_t *ptr)
{
    int status = linking_atomic_cas(old_value, new_value, ptr);
    linking_memory_barrier();
    return status;
}

extern LINKING_ATOMIC_INLINE
int linking_atomic_release_cas(int32_t old_value, int32_t new_value,
                               volatile int32_t *ptr)
{
    linking_memory_barrier();
    return linking_atomic_cas(old_value, new_value, ptr);
}

extern LINKING_ATOMIC_INLINE
int32_t linking_atomic_add(int32_t increment, volatile int32_t *ptr)
{
    int32_t prev, tmp, status;
    linking_memory_barrier();
    do {
        __asm__ __volatile__ ("ldrex %0, [%4]\n"
                              "add %1, %0, %5\n"
                              "strex %2, %1, [%4]"
                              : "=&r" (prev), "=&r" (tmp),
                                "=&r" (status), "+m" (*ptr)
                              : "r" (ptr), "Ir" (increment)
                              : "cc");
    } while (__builtin_expect(status != 0, 0));
    return prev;
}

extern LINKING_ATOMIC_INLINE int32_t linking_atomic_inc(volatile int32_t *addr)
{
    return linking_atomic_add(1, addr);
}

extern LINKING_ATOMIC_INLINE int32_t linking_atomic_dec(volatile int32_t *addr)
{
    return linking_atomic_add(-1, addr);
}

extern LINKING_ATOMIC_INLINE
int32_t linking_atomic_and(int32_t value, volatile int32_t *ptr)
{
    int32_t prev, tmp, status;
    linking_memory_barrier();
    do {
        __asm__ __volatile__ ("ldrex %0, [%4]\n"
                              "and %1, %0, %5\n"
                              "strex %2, %1, [%4]"
                              : "=&r" (prev), "=&r" (tmp),
                                "=&r" (status), "+m" (*ptr)
                              : "r" (ptr), "Ir" (value)
                              : "cc");
    } while (__builtin_expect(status != 0, 0));
    return prev;
}

extern LINKING_ATOMIC_INLINE
int32_t linking_atomic_or(int32_t value, volatile int32_t *ptr)
{
    int32_t prev, tmp, status;
    linking_memory_barrier();
    do {
        __asm__ __volatile__ ("ldrex %0, [%4]\n"
                              "orr %1, %0, %5\n"
                              "strex %2, %1, [%4]"
                              : "=&r" (prev), "=&r" (tmp),
                                "=&r" (status), "+m" (*ptr)
                              : "r" (ptr), "Ir" (value)
                              : "cc");
    } while (__builtin_expect(status != 0, 0));
    return prev;
}

#endif /* LINKING_CUTILS_ATOMIC_ARM_H */
