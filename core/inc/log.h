/*
 * Copyright (C) 2005 The Android Open Source Project
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

//
// C/C++ logging functions.  See the logging documentation for API details.
//
// We'd like these to be available from C code (in case we import some from
// somewhere), so this has a C interface.
//
// The output will be correct when the log file is shared between multiple
// threads and/or multiple processes so long as the operating system
// supports O_APPEND.  These calls have mutex-protected data structures
// and so are NOT reentrant.  Do not use LOG in a signal handler.
//
#ifndef _LIBS_UTILS_LOG_H
#define _LIBS_UTILS_LOG_H

#include <log.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdarg.h>

#ifdef __cplusplus

#define LOG_BUF_SIZE 1024

typedef enum linking_LogPriority {
    LINKING_LOG_UNKNOWN = 0,
    LINKING_LOG_DEFAULT,    /* only for SetMinPriority() */
    LINKING_LOG_VERBOSE,
    LINKING_LOG_DEBUG,
    LINKING_LOG_INFO,
    LINKING_LOG_WARN,
    LINKING_LOG_ERROR,
    LINKING_LOG_FATAL,
    LINKING_LOG_SILENT,     /* only for SetMinPriority(); must be last */
} linking_LogPriority;

// int __linking_log_print(int prio, const char *tag, const char *fmt, ...)
// {
//     va_list ap;
//     char buf[LOG_BUF_SIZE];
 
//     va_start(ap, fmt);
//     vsnprintf(buf, LOG_BUF_SIZE, fmt, ap);
//     va_end(ap);
 
//     return fprintf(stderr, "%s", buf);
// }

// #define linking_printLog(prio, tag, x...) \
//     __linking_log_print(prio, tag, x)

#define linking_printLog(prio, tag, x...) ((void)0)

#ifndef LOG_NDEBUG
#define LOG_NDEBUG 1
#endif

#ifndef LOG_TAG
#define LOG_TAG NULL
#endif

#ifndef __predict_false
#define __predict_false(exp) __builtin_expect((exp) != 0, 0)
#endif

#ifndef ALOG
#define ALOG(priority, tag, ...) \
    LOG_PRI(LINKING_##priority, tag, __VA_ARGS__)
#endif

#ifndef LOG_PRI
#define LOG_PRI(priority, tag, ...) \
    linking_printLog(priority, tag, __VA_ARGS__)
#endif

#ifndef ALOGV
#define __ALOGV(...) ((void)ALOG(LOG_VERBOSE, LOG_TAG, __VA_ARGS__))
#if LOG_NDEBUG
#define ALOGV(...) do { if (0) { __ALOGV(__VA_ARGS__); } } while (0)
#else
#define ALOGV(...) __ALOGV(__VA_ARGS__)
#endif
#endif

#ifndef ALOGD
#define ALOGD(...) ((void)ALOG(LOG_DEBUG, LOG_TAG, __VA_ARGS__))
#endif

#ifndef ALOGI
#define ALOGI(...) ((void)ALOG(LOG_INFO, LOG_TAG, __VA_ARGS__))
#endif

#ifndef ALOGW
#define ALOGW(...) ((void)ALOG(LOG_WARN, LOG_TAG, __VA_ARGS__))
#endif

#ifndef ALOGE
#define ALOGE(...) ((void)ALOG(LOG_ERROR, LOG_TAG, __VA_ARGS__))
#endif

#ifndef SLOGV
#define SLOGV(...) ((void)0)
#endif

#ifndef SLOGD
#define SLOGD(...) ((void)0)
#endif

#ifndef SLOGI
#define SLOGI(...) ((void)0)
#endif

#ifndef SLOGW
#define SLOGW(...) ((void)0)
#endif

#ifndef SLOGE
#define SLOGE(...) ((void)0)
#endif

#ifndef ALOGV_IF
#define ALOGV_IF(cond, ...) ((void)0)
#endif

#ifndef ALOGD_IF
#define ALOGD_IF(cond, ...) ((void)0)
#endif

#ifndef ALOGI_IF
#define ALOGI_IF(cond, ...) ((void)0)
#endif

#ifndef ALOGW_IF
#define ALOGW_IF(cond, ...) ((void)0)
#endif

#ifndef ALOGE_IF
#define ALOGE_IF(cond, ...) ((void)0)
#endif

#ifndef ALOG_ASSERT
#define ALOG_ASSERT(cond, ...) ((void)0)
#endif

#ifndef LOG_FATAL_IF
#define LOG_FATAL_IF(cond, ...) ((void)0)
#endif

#ifndef LOG_ALWAYS_FATAL_IF
#define LOG_ALWAYS_FATAL_IF(cond, ...) (void)0
#endif

#ifndef LOG_ALWAYS_FATAL
#define LOG_ALWAYS_FATAL(...) ((void)0)
#endif

#ifndef IF_ALOG
#define IF_ALOG(priority, tag) ((void)0)
#endif

#ifndef IF_ALOGV
#define IF_ALOGV() if (false)
#endif

#define linking_errorWriteWithInfoLog(tag, subTag, uid, data, dataLen) ((void)0)

namespace linking {

/*
 * A very simple utility that yells in the log when an operation takes too long.
 */
class LogIfSlow {
public:
    LogIfSlow(const char* tag, linking_LogPriority priority,
            int timeoutMillis, const char* message);
    ~LogIfSlow();

private:
    const char* const mTag;
    const linking_LogPriority mPriority;
    const int mTimeoutMillis;
    const char* const mMessage;
    const int64_t mStart;
};

/*
 * Writes the specified debug log message if this block takes longer than the
 * specified number of milliseconds to run.  Includes the time actually taken.
 *
 * {
 *     ALOGD_IF_SLOW(50, "Excessive delay doing something.");
 *     doSomething();
 * }
 */
#define ALOGD_IF_SLOW(timeoutMillis, message) \
    linking::LogIfSlow _logIfSlow(LOG_TAG, LINKING_LOG_DEBUG, timeoutMillis, message);

} // namespace linking

#endif // __cplusplus

#endif // _LIBS_UTILS_LOG_H
