#include "osutils.h"

namespace linking {

Result OsUtils::threadCreate(OSThreadFunc threadEntryFunction,
    void* pThreadData, OSThreadHandle* phThread)
{
    Result result = Ok;
    if(pthread_create(phThread, 0, threadEntryFunction, pThreadData) != 0) {
        result = -EFailed;
    }
    return result;
}

Result OsUtils::threadSetName(OSThreadHandle hThread, const char* pName) {
    Result result = Ok;
    if(pthread_setname_np(hThread, pName) != 0) {
        result = -EFailed;
    }
    return result;
}

void OsUtils::threadWait(OSThreadHandle hThread) {
    pthread_join(hThread, NULL);
}

uint32_t OsUtils::getThreadId() {
    return (uint32_t)pthread_self();//gettid();
}

void OsUtils::threadTerminate(OSThreadHandle hThread) {
    pthread_join(hThread, NULL);
}

NativeCondition* NativeCondition::create() {
    NativeCondition* pNativeCondition = NULL;
    pNativeCondition = new NativeCondition();
    if(NULL != pNativeCondition) {
        if(Ok != pNativeCondition->initialize()) {
            delete pNativeCondition;
            pNativeCondition = NULL;
        }
    }
    return pNativeCondition;
}

void NativeCondition::destroy() {
    if(true == m_validNativeConditionVar) {
        pthread_cond_destroy(&m_conditionVar);
    }
    delete this;
}

Result NativeCondition::initialize() {
    Result result = Ok;
    if(pthread_cond_init(&m_conditionVar, NULL) == 0) {
        m_validNativeConditionVar = true;
    } else {
        result = -EFailed;
    }
    return result;
}

Result NativeCondition::wait(OSMutexHandle* phNativeMutex) {
    int rc = 0;
    Result result = -EFailed;

    rc = pthread_cond_wait(&m_conditionVar, phNativeMutex);
    if(0 == rc) {
        result = Ok;
    }
    return result;
}

Result NativeCondition::timedWait(OSMutexHandle* phNativeMutex, uint32_t timeoutMs) {
    Result          result          = Ok;
    int             waitResult      = 0;
    uint32_t        timeoutSec      = (timeoutMs / 1000UL);
    uint32_t        timeoutNanosec  = (timeoutMs % 1000UL) * 1000000UL;
    struct timespec timeout     = {0};

    //get timeout
    clock_gettime(CLOCK_REALTIME, &timeout);
    timeoutSec += (static_cast<uint32_t>(timeout.tv_nsec) + timeoutNanosec) / 1000000000UL;
    timeoutNanosec = (static_cast<uint32_t>(timeout.tv_nsec) + timeoutNanosec) % 1000000000UL;
    timeout.tv_sec += static_cast<int32_t>(timeoutSec);
    timeout.tv_nsec = static_cast<int32_t>(timeoutNanosec);

    waitResult = pthread_cond_timedwait(&m_conditionVar, phNativeMutex, &timeout);
    if(waitResult != 0) {
        if(ETIMEDOUT == waitResult) {
            result = -ETimeout;
        } else {
            result = -EFailed;
        }
    }
    return result;
}

void NativeCondition::signal() {
    pthread_cond_signal(&m_conditionVar);
}

void NativeCondition::broadcast() {
    pthread_cond_broadcast(&m_conditionVar);
}

NativeMutex* NativeMutex::create() {
    NativeMutex* pNativeMutex = NULL;
    pNativeMutex = new NativeMutex();
    if(NULL != pNativeMutex) {
        if(Ok != pNativeMutex->initialize()) {
            delete pNativeMutex;
            pNativeMutex = NULL;
        }
    }
    return pNativeMutex;
}

Result NativeMutex::initialize() {
    Result result = Ok;
    pthread_mutexattr_t attr;
    bool bValidAttr = false;

    if(pthread_mutexattr_init(&attr) == 0) {
        bValidAttr = true;
    } else {
        result = -EFailed;
    }
    // Using re-entrant mutexes
    if((Ok == result) &&
            (pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE_NP) != 0)) {
        result = -EFailed;
    }

    if((Ok == result) &&
            (pthread_mutex_init(&m_mutex, &attr) == 0)) {
        m_validNativeMutex = true;
    } else {
        result = -EFailed;
    }

    if(true == bValidAttr) {
        pthread_mutexattr_destroy(&attr);
    }
    return result;
}

void NativeMutex::destroy() {
    if(true == m_validNativeMutex) {
        pthread_mutex_destroy(&m_mutex);
    }
    delete this;
}

void NativeMutex::lock() {
    pthread_mutex_lock(&m_mutex);
}

Result NativeMutex::tryLock() {
    Result result = Ok;
    int returnCode = 0;

    returnCode = pthread_mutex_trylock(&m_mutex);
    if(0 != returnCode) {
        if(EBUSY == returnCode) {
            result = -EBusy;
        } else {
            result = -EFailed;
        }
    } else {
        //
    }
    return result;
}

void NativeMutex::unlock() {
    pthread_mutex_unlock(&m_mutex);
}

OSMutexHandle* NativeMutex::getNativeHandle() {
    return ((true == m_validNativeMutex) ? &m_mutex : NULL);
}

NativeSema* NativeSema::create() {
    NativeSema* pNativeSema = NULL;
    pNativeSema = new NativeSema();
    if(NULL != pNativeSema) {
        if(Ok != pNativeSema->initialize()) {
            delete pNativeSema;
            pNativeSema = NULL;
        }
    }

    return pNativeSema;
}

Result NativeSema::initialize() {
    Result result = Ok;

    m_validNativeSema = false;
    if(sem_init(&m_semaphore, 0, 0) == 0) {
        m_validNativeSema = true;
    } else {
        result = -EFailed;
    }
    return result;
}

void NativeSema::destroy() {
    if(true == m_validNativeSema) {
        sem_destroy(&m_semaphore);
    }

    delete this;
}

void NativeSema::wait() {
    if(true == m_validNativeSema) {
        sem_wait(&m_semaphore);
    }
}

void NativeSema::signal() {
    if(true == m_validNativeSema) {
        sem_post(&m_semaphore);
    }
}

Result NativeSema::timedWait(uint32_t timeoutMs) {
    Result          result          = Ok;
    int             waitResult      = 0;
    uint32_t        timeoutSec      = (timeoutMs / 1000UL);
    uint32_t        timeoutNanosec  = (timeoutMs % 1000UL) * 1000000UL;
    struct timespec timeout     = {0};

    //get timeout
    clock_gettime(CLOCK_REALTIME, &timeout);
    timeoutSec += (static_cast<uint32_t>(timeout.tv_nsec) + timeoutNanosec) / 1000000000UL;
    timeoutNanosec = (static_cast<uint32_t>(timeout.tv_nsec) + timeoutNanosec) % 1000000000UL;
    timeout.tv_sec += static_cast<int32_t>(timeoutSec);
    timeout.tv_nsec = static_cast<int32_t>(timeoutNanosec);

    waitResult = sem_timedwait(&m_semaphore, &timeout);
    if(waitResult != 0) {
        if(ETIMEDOUT == errno) {
            result = -ETimeout;
        } else {
            result = -EFailed;
        }
    }
    return result;
}

} // namespace linking

