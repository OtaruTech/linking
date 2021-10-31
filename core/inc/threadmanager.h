#ifndef __LINKING_THREAD_MANAGER_H__
#define __LINKING_THREAD_MANAGER_H__

#include <queue>
#include "osutils.h"
#include "types.h"

namespace linking {

typedef uint64_t JobHandle;
typedef void* (*JobFunc)(void* pArg);

static const JobHandle InvalidJobHandle = 0;
static const uint32_t MaxNameLength = 255;
static const uint32_t MaxRegisteredJobs = 100;
static const uint32_t MaxNumThread = 100;

typedef uint32_t CoreStatus;
typedef uint32_t JobFlushStatus;

static const CoreStatus Error           = 0;
static const CoreStatus Initialized     = 1;
static const CoreStatus Stopped         = 2;

static const JobFlushStatus Noflush         = 0;
static const JobFlushStatus FlushRequested  = 1;
static const JobFlushStatus Flushed         = 2;

enum class JobStatus {
    Submitted,
    Ready,
    OnHold,
    Stopped,
    Invalid,
};

struct RegisteredJob {
    JobFunc         funcAddr;
    char            name[MaxNameLength];
    bool            isUsed;
    uint64_t        hRegister;
    uint32_t        slot;
    uint32_t        uniqueCounter;
    OSThreadHandle  hRegisteredThread;
    JobFlushStatus  flushStatus;
};

struct RuntimeJob {
    uint64_t    hJob;
    void*       pData;
    uint64_t    requestId;
    JobStatus   status;
};

class Compare {
public:
    bool operator() (RuntimeJob* pJob1, RuntimeJob* pJob2) {
        bool isPriority = false;
        if(pJob1->requestId > pJob2->requestId) {
            isPriority = true;
        }
        return isPriority;
    }
};

struct ThreadControl {
    NativeMutex*              pThreadLock;
    NativeMutex*              pFlushJobSubmitLock;
    NativeMutex*              pQueueLock;
    NativeMutex*              pFlushLock;
    NativeCondition*          pReadOK;
    NativeCondition*          pFlushOK;
    CoreStatus                status;
    volatile bool             jobPending;
    volatile uint32_t         blockingStatus;
    bool                      isAvailable;
};

struct ThreadData {
    std::deque<RuntimeJob*> pq;
};

struct ThreadConfig {
    uint32_t            threadId;
    OSThreadHandle      hWorkThread;
    JobFunc             workThreadFunc;
    void*               pContext;
    ThreadData          data;
    ThreadControl       ctrl;
    bool                isUsed;
};

class ThreadManager;

class ThreadManager {
public:
    static Result create(ThreadManager** ppInstance, const char* pName);
    void destroy();
    Result registerJobFamily(JobFunc jobFuncAddr, const char* pJobFuncName,
        JobHandle* phJob);
    Result unregisterJobFamily(JobFunc jobFuncAddr, const char* pJobFuncName,
        JobHandle phJob);
    Result postJob(JobHandle hJob, void* pData, uint64_t requestId);
    Result removeJob(JobHandle hJob, void* pData);
    Result getAllPostedJobs(JobHandle hJob, std::vector<void*>& rPostedJobs);
    Result flushJob(JobHandle hJob, bool forceFlush);
    bool isJobAvailable(JobHandle hJob);
private:
    ThreadManager();
    ~ThreadManager();
    Result initialize(const char* pName);
    bool isJobAlreadyRegistered(JobHandle* phJob);
    bool isFreeSlotAvailable(uint32_t* pSlot, uint32_t* pCount);

    static inline uint32_t getCounterByHandle(JobHandle hJob)
    {
        return static_cast<uint32_t>(hJob >> 32);
    }

    static inline uint32_t getSlotByHandle(JobHandle hJob)
    {
        uint32_t last32Bits = 0xFFFFFFFF;
        uint32_t slot       = (hJob & last32Bits);
        return slot;
    }

    inline RegisteredJob* getJobByHandle(JobHandle hJob)
    {
        uint32_t slot = getSlotByHandle(hJob);
        return &m_registeredJobs[slot];
    }

    inline ThreadConfig* getThreadConfigByHandle(JobHandle hJob)
    {
        OSThreadHandle hThreadConfig  = 0;
        OSThreadHandle hRegisteredJob = 0;
        uint32_t       slot           = getSlotByHandle(hJob);

        hRegisteredJob = m_registeredJobs[slot].hRegisteredThread;
        hThreadConfig  = m_threadWorker[slot].hWorkThread;

        if((0 != hRegisteredJob) && (hRegisteredJob == hThreadConfig)) {
            return &m_threadWorker[slot];
        } else {
            return NULL;
        }
    }

    inline ThreadData* getThreadDataByHandle(JobHandle hJob)
    {
        ThreadConfig* pCfg;
        pCfg = getThreadConfigByHandle(hJob);
        if(NULL != pCfg) {
            return &pCfg->data;
        } else {
            return NULL;
        }
    }

    inline ThreadControl* getThreadControlByHandle(JobHandle hJob)
    {
        ThreadConfig* pCfg;
        pCfg = getThreadConfigByHandle(hJob);
        if(NULL != pCfg) {
            return &pCfg->ctrl;
        } else {
            return NULL;
        }
    }

    inline JobFunc getJobFunc(JobHandle hJob)
    {
        RegisteredJob* pRegisteredJob = getJobByHandle(hJob);
        return pRegisteredJob->funcAddr;
    }

    JobHandle packJobHandleFromRegisteredJob(RegisteredJob* pRegisteredJob)
    {
        JobHandle handle = InvalidJobHandle;
        handle = pRegisteredJob->uniqueCounter;
        handle = handle << 32;
        handle |= pRegisteredJob->slot;
        return handle;
    }

    inline JobFlushStatus getFlushStatus(JobHandle hJob)
    {
        RegisteredJob* pRegisteredJob = getJobByHandle(hJob);
        return pRegisteredJob->flushStatus;
    }

    inline void setFlushStatus(JobHandle hJob, JobFlushStatus val)
    {
        RegisteredJob* pRegisteredJob = getJobByHandle(hJob);
        pRegisteredJob->flushStatus = val;
    }

    inline bool getFlushBlockStatus(ThreadControl* pCtrl)
    {
        return static_cast<bool>(pCtrl->blockingStatus);
    }

    inline void setFlushBlockStatus(ThreadControl* pCtrl, bool blockingStatus)
    {
        pCtrl->blockingStatus = blockingStatus;
    }

    inline CoreStatus getStatus(ThreadControl* pCtrl) const
    {
        return pCtrl->status;
    }

    inline void setStatus(ThreadControl* pCtrl, CoreStatus status)
    {
        pCtrl->status = status;
    }

    Result startThreads(uint32_t slot, JobHandle* phJob);
    Result addToPriorityQueue(RuntimeJob* pJob);
    static void* workerThreadBody(void* pArg);
    void* doWork(void* pArg);
    Result trigger(JobHandle hJob);
    Result processJobQueue(void* pCfg);
    void dispatchJob(RuntimeJob* pJob);
    Result stopThreads(JobHandle hJob);
    void dump();

    ThreadManager(const ThreadManager&) = delete;
    ThreadManager& operator=(const ThreadManager&) = delete;

    char            m_name[MaxNameLength];
    uint32_t        m_totalNumOfRegisteredJob;
    RegisteredJob   m_registeredJobs[MaxRegisteredJobs];
    ThreadConfig    m_threadWorker[MaxNumThread];
    NativeMutex*    m_pRegisteredJobLock;
}; // class ThreadManager

} // namespace linking
#endif
