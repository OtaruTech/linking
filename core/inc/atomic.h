#ifndef __LINKING_ATOMIC_INLINE_H__
#define __LINKING_ATOMIC_INLINE_H__

#if defined(__arm__)
#include <atomic-arm.h>
#elif defined(__x86_64__)
#include <atomic-x86_64.h>
#else
#error atomic operations are unsupported
#endif

#endif
