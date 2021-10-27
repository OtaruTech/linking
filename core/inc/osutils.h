#ifndef __LINKING_OS_UTILS_H__
#define __LINKING_OS_UTILS_H__

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>

#include <dirent.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "types.h"

namespace linking {

typedef void* (*OSThreadFunc)(void* pArg);

typedef pthread_t OSThreadHandle;
typedef pthread_attr_t OSThreadAttr;
typedef pthread_mutex_t OSMutexHandle;

class OsUtils {
public:
    static Result threadCreate(OSThreadFunc threadEntryFunction,
        void* pThreadData, OSThreadHandle* phThread);
    static Result threadSetName(OSThreadHandle hThread, const char* pName);
    static void threadWait(OSThreadHandle hThread);
    static uint32_t getThreadId();
    static void threadTerminate(OSThreadHandle hThread);

private:
    OsUtils() = default;
}; // class OsUtils

class NativeCondition final {
public:
    static NativeCondition* create();
    void destroy();
    Result wait(OSMutexHandle* phMutex);
    Result timedWait(OSMutexHandle* phMutex, uint32_t timeoutMs);
    void signal();
    void broadcast();

private:
    NativeCondition() = default;
    ~NativeCondition() = default;
    Result initialize();
    NativeCondition(const NativeCondition&) = delete;
    NativeCondition& operator=(const NativeCondition&) = delete;

    pthread_cond_t      m_conditionVar;
    bool                m_validNativeConditionVar;
}; // class NativeCondition

class NativeMutex final {
public:
    static NativeMutex* create();
    void destroy();
    void lock();
    Result tryLock();
    void unlock();
    OSMutexHandle* getNativeHandle();

private:
    NativeMutex() = default;
    ~NativeMutex() = default;
    Result initialize();
    NativeMutex(const NativeMutex&) = delete;
    NativeMutex& operator=(const NativeMutex&) = delete;

    pthread_mutex_t     m_mutex;
    bool                m_validNativeMutex;
}; // class NativeMutex

class NativeSema final {
public:
    static NativeSema* create();
    void destroy();
    void wait();
    Result timedWait(uint32_t timeoutMs);
    void signal();

private:
    NativeSema() = default;
    ~NativeSema() = default;
    Result initialize();
    NativeSema(const NativeSema&) = delete;
    NativeSema& operator=(const NativeSema&) = delete;

    sem_t       m_semaphore;
    bool        m_validNativeSema;
}; // class NativeSema

} // namespace linking

#endif
