/*
 * Copyright (C) 2007 The Android Open Source Project
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

#ifndef _LIBS_UTILS_THREAD_DEFS_H
#define _LIBS_UTILS_THREAD_DEFS_H

#include <stdint.h>
#include <sys/types.h>
// #include <system/graphics.h>
// #include <system/thread_defs.h>

// ---------------------------------------------------------------------------
// C API

#ifdef __cplusplus
extern "C" {
#endif

typedef void* linking_thread_id_t;

typedef int (*linking_thread_func_t)(void*);

#ifdef __cplusplus
} // extern "C"
#endif

// ---------------------------------------------------------------------------
// C++ API
#ifdef __cplusplus
namespace linking {
// ---------------------------------------------------------------------------

typedef linking_thread_id_t thread_id_t;
typedef linking_thread_func_t thread_func_t;

enum {
    /*
     * ***********************************************
     * ** Keep in sync with linking.os.Process.java **
     * ***********************************************
     *
     * This maps directly to the "nice" priorities we use in Android.
     * A thread priority should be chosen inverse-proportionally to
     * the amount of work the thread is expected to do. The more work
     * a thread will do, the less favorable priority it should get so that
     * it doesn't starve the system. Threads not behaving properly might
     * be "punished" by the kernel.
     * Use the levels below when appropriate. Intermediate values are
     * acceptable, preferably use the {MORE|LESS}_FAVORABLE constants below.
     */
    LINKING_PRIORITY_LOWEST         =  19,
    /* use for background tasks */
    LINKING_PRIORITY_BACKGROUND     =  10,
    /* most threads run at normal priority */
    LINKING_PRIORITY_NORMAL         =   0,
    /* threads currently running a UI that the user is interacting with */
    LINKING_PRIORITY_FOREGROUND     =  -2,
    /* the main UI thread has a slightly more favorable priority */
    LINKING_PRIORITY_DISPLAY        =  -4,
    /* ui service treads might want to run at a urgent display (uncommon) */
    LINKING_PRIORITY_URGENT_DISPLAY =  -8,
    /* all normal audio threads */
    LINKING_PRIORITY_AUDIO          = -16,
    /* service audio threads (uncommon) */
    LINKING_PRIORITY_URGENT_AUDIO   = -19,
    /* should never be used in practice. regular process might not
     * be allowed to use this level */
    LINKING_PRIORITY_HIGHEST        = -20,
    LINKING_PRIORITY_DEFAULT        = LINKING_PRIORITY_NORMAL,
    LINKING_PRIORITY_MORE_FAVORABLE = -1,
    LINKING_PRIORITY_LESS_FAVORABLE = +1,
};

enum {
    PRIORITY_LOWEST         = LINKING_PRIORITY_LOWEST,
    PRIORITY_BACKGROUND     = LINKING_PRIORITY_BACKGROUND,
    PRIORITY_NORMAL         = LINKING_PRIORITY_NORMAL,
    PRIORITY_FOREGROUND     = LINKING_PRIORITY_FOREGROUND,
    PRIORITY_DISPLAY        = LINKING_PRIORITY_DISPLAY,
    PRIORITY_URGENT_DISPLAY = LINKING_PRIORITY_URGENT_DISPLAY,
    PRIORITY_AUDIO          = LINKING_PRIORITY_AUDIO,
    PRIORITY_URGENT_AUDIO   = LINKING_PRIORITY_URGENT_AUDIO,
    PRIORITY_HIGHEST        = LINKING_PRIORITY_HIGHEST,
    PRIORITY_DEFAULT        = LINKING_PRIORITY_DEFAULT,
    PRIORITY_MORE_FAVORABLE = LINKING_PRIORITY_MORE_FAVORABLE,
    PRIORITY_LESS_FAVORABLE = LINKING_PRIORITY_LESS_FAVORABLE,
};

// ---------------------------------------------------------------------------
}; // namespace linking
#endif  // __cplusplus
// ---------------------------------------------------------------------------


#endif // _LIBS_UTILS_THREAD_DEFS_H
