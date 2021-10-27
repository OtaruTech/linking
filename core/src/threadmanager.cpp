#include <glog/logging.h>
#include "threadmanager.h"

namespace linking {

using namespace std;

Result ThreadManager::create(ThreadManager** ppInstance, const char* pName)
{
    Result          result         = Ok;
    ThreadManager*  pLocalInstance = NULL;

    pLocalInstance = new ThreadManager();
    if(NULL != pLocalInstance) {
        result = pLocalInstance->initialize(pName);
        if(Ok != result) {
            delete pLocalInstance;
            pLocalInstance = NULL;
            LOG(ERROR) << "Failed to create threadmanager" << endl;
        }
    }
    if(Ok == result) {
        *ppInstance = pLocalInstance;
    }
    return result;
}

void ThreadManager::destroy() {
    delete this;
}

Result ThreadManager::registerJobFamily(JobFunc jobFuncAddr, const char* pJobFuncName,
    JobHandle* phJob)
{
    Result          result          = Ok;
    uint32_t        slot            = 0;
    uint32_t        count           = 0;
    RegisteredJob*  pRegisteredJob  = NULL;

    bool jobAlreadyRegistered = isJobAlreadyRegistered(phJob);
    bool freeSlotsAvailable   = isFreeSlotAvailable(&slot, &count);

    if((true == jobAlreadyRegistered) ||
       (false == freeSlotsAvailable))
    {
        LOG(ERROR) << "Failed to registerJobFamily, jobAlreadyRegistered: " <<
            jobAlreadyRegistered << ", freeSlotsAvailable: " <<
            freeSlotsAvailable << endl;
        result = -EFailed;
    }
    if(Ok == result) {
        pRegisteredJob                  = &m_registeredJobs[slot];
        ::memset(pRegisteredJob, 0x00, sizeof(RegisteredJob));
        pRegisteredJob->funcAddr        = jobFuncAddr;
        pRegisteredJob->slot            = slot;
        pRegisteredJob->uniqueCounter   = count;
        pRegisteredJob->isUsed          = true;

        ::strncpy(pRegisteredJob->name, pJobFuncName, MaxNameLength);
        *phJob                          = packJobHandleFromRegisteredJob(pRegisteredJob);
        pRegisteredJob->hRegister       = *phJob;

        result = startThreads(slot, phJob);
        if(Ok == result) {
            if(0 != pRegisteredJob->hRegisteredThread) {
                setFlushStatus(*phJob, Noflush);
            } else {
                LOG(ERROR) << "Failed to get thread handle" << endl;
                result = -EFailed;
            }
        }
    }
    return result;
}

Result ThreadManager::unregisterJobFamily(JobFunc jobFuncAddr, const char* pJobFuncName,
    JobHandle hJob)
{
    Result         result         = -EFailed;
    RegisteredJob* pRegisteredJob = NULL;

    m_pRegisteredJobLock->lock();

    pRegisteredJob = getJobByHandle(hJob);
    if(true != isJobAlreadyRegistered(&hJob)) {
        LOG(ERROR) << "Failed to unregister job " << pJobFuncName << " FuncAddr " <<
            jobFuncAddr << " result " << result << endl;;
    } else if(NULL != pRegisteredJob) {
        uint32_t slot = pRegisteredJob->slot;
        result = stopThreads(hJob);
        if(Ok == result) {
            m_threadWorker[slot].data.pq.clear();
            m_threadWorker[slot].data.pq.shrink_to_fit();
            ::memset(&m_registeredJobs[slot], 0x00, sizeof(RegisteredJob));
            ::memset(&m_threadWorker[slot], 0x00, sizeof(ThreadConfig));
            m_totalNumOfRegisteredJob --;
            LOG(INFO) << "Feature name " << pJobFuncName <<
                " remaining jobfamily " << m_totalNumOfRegisteredJob << endl;
        } else {
            LOG(ERROR) << "Failed to stop thread" << endl;
        }
    }
    m_pRegisteredJobLock->unlock();

    return result;
}

Result ThreadManager::startThreads(uint32_t slot, JobHandle* phJob)
{
    RegisteredJob* pRegisteredJob = getJobByHandle(*phJob);
    Result         result         = Ok;
    ThreadConfig*  pCfg           = NULL;

    pCfg = &m_threadWorker[slot];

    if((NULL == pRegisteredJob) || (pCfg->isUsed == true)) {
        LOG(INFO) << "Slot " << slot <<
            " is already occupied. totalNumOfRegisteredJob " <<
            m_totalNumOfRegisteredJob << endl;
        result = -EFailed;
    } else {
        pCfg->threadId                  = pRegisteredJob->uniqueCounter;
        pCfg->workThreadFunc            = workerThreadBody;
        pCfg->ctrl.pReadOK              = NativeCondition::create();
        pCfg->ctrl.pThreadLock          = NativeMutex::create();
        pCfg->ctrl.pFlushJobSubmitLock  = NativeMutex::create();
        pCfg->ctrl.pQueueLock           = NativeMutex::create();
        pCfg->ctrl.pFlushOK             = NativeCondition::create();
        pCfg->ctrl.pFlushLock           = NativeMutex::create();
        pCfg->pContext                  = reinterpret_cast<void*>(this);
        pCfg->isUsed                    = true;

        if((NULL == pCfg->ctrl.pReadOK) ||
           (NULL == pCfg->ctrl.pThreadLock) ||
           (NULL == pCfg->ctrl.pFlushJobSubmitLock) ||
           (NULL == pCfg->ctrl.pQueueLock))
        {
            LOG(ERROR) << "Couldn't read lock resource. slot " << slot <<
                " m_totalNumOfRegisteredJob " << m_totalNumOfRegisteredJob << endl;
            pCfg->isUsed = false;
            result = -EFailed;
        }
        if(Ok == result) {
            result = OsUtils::threadCreate(pCfg->workThreadFunc,
                pCfg, &pCfg->hWorkThread);
            if(Ok == result) {
                pRegisteredJob->hRegisteredThread = pCfg->hWorkThread;
                pCfg->ctrl.pThreadLock->lock();
                setStatus(&pCfg->ctrl, Initialized);
                pCfg->ctrl.pThreadLock->unlock();
            } else {
                LOG(ERROR) << "Couldn't create worker thread, logical threadId " <<
                    pCfg->threadId << endl;
                pCfg->isUsed = false;
                result = -EFailed;
            }
        }
        LOG(INFO) << "ThreadId " << pCfg->threadId << " workThreadFunc" <<
            pCfg->workThreadFunc << " hWorkThread" << pCfg->hWorkThread <<
            " ReadOk " << pCfg->ctrl.pReadOK << endl << "threadLock " <<
            pCfg->ctrl.pThreadLock << " flushjobsumbit lock " <<
            pCfg->ctrl.pFlushJobSubmitLock << " queuelock " <<
            pCfg->ctrl.pQueueLock << " context " << pCfg->pContext <<
            " isUsed " << pCfg->isUsed << ", slot " << slot <<
            " NumOfRegisgteredJob " << m_totalNumOfRegisteredJob << endl;
    }
    return result;
}

Result ThreadManager::addToPriorityQueue(RuntimeJob* pJob)
{
    Result         result = Ok;
    JobHandle      hJob   = 0;
    ThreadData*    pData  = NULL;
    ThreadControl* pCtrl  = NULL;

    hJob  = pJob->hJob;
    pData = getThreadDataByHandle(hJob);
    pCtrl = getThreadControlByHandle(hJob);

    if((NULL == pData) || (NULL == pCtrl)) {
        LOG(ERROR) << "Failed to get threaddata or threadcontrol" << endl;
        result = -EFailed;
    } else {
        pCtrl->pQueueLock->lock();
        pJob->status = JobStatus::Submitted;
        pData->pq.push_back(pJob);
        pCtrl->pQueueLock->unlock();
    }

    return result;
}

void* ThreadManager::workerThreadBody(void* pArg)
{
    ThreadConfig* pConfig = NULL;
    pConfig = reinterpret_cast<ThreadConfig*>(pArg);
    if(NULL != pConfig) {
        ThreadManager* pThreadManager = reinterpret_cast<ThreadManager*>(pConfig->pContext);
        pThreadManager->doWork(pArg);
    }

    return NULL;
}

void* ThreadManager::doWork(void* pArg)
{
    Result          result  = Ok;
    ThreadConfig*   pConfig = NULL;
    ThreadControl*  pCtrl   = NULL;

    //per thread
    pConfig = reinterpret_cast<ThreadConfig*>(pArg);
    pCtrl = &pConfig->ctrl;

    if(NULL == pCtrl) {
        LOG(ERROR) << "Failed to start" << endl;
    } else {
        pCtrl->pThreadLock->lock();
        while(Stopped != getStatus(pCtrl)) {
            while((Stopped != getStatus(pCtrl)) && (false == pCtrl->jobPending)) {
                pCtrl->pReadOK->wait(pCtrl->pThreadLock->getNativeHandle());
            }
            if(Stopped != getStatus(pCtrl)) {
                pCtrl->jobPending = false;
                pCtrl->pThreadLock->unlock();
            }
            result = processJobQueue(pConfig);
            // to guarantee flush is done
            if(true == getFlushBlockStatus(pCtrl)) {
                pCtrl->pFlushLock->lock();
                setFlushBlockStatus(pCtrl, false);
                pCtrl->pFlushOK->signal();
                pCtrl->pFlushLock->unlock();
            }
            pCtrl->pThreadLock->lock();
            if(Ok != result) {
                LOG(ERROR) << "ProcessJobQueue failed with result" << endl;
            }
        }
        pCtrl->pThreadLock->unlock();
    }
    return NULL;
}

Result ThreadManager::trigger(JobHandle hJob)
{
    Result         result = Ok;
    ThreadControl* pCtrl  = NULL;

    pCtrl = getThreadControlByHandle(hJob);
    if(NULL == pCtrl) {
        LOG(ERROR) << "Failed to get threadctrl " << pCtrl << endl;
        result = -EFailed;
    } else {
        pCtrl->pThreadLock->lock();
        pCtrl->jobPending = true;
        pCtrl->pThreadLock->unlock();
        pCtrl->pReadOK->signal();
    }

    return result;
}

Result ThreadManager::processJobQueue(void* pCfg)
{
    uint32_t       result  = Ok;
    ThreadConfig*  pConfig  = NULL;
    ThreadControl* pCtrl    = NULL;
    ThreadData*    pData    = NULL;
    RuntimeJob*    pJob     = NULL;
    bool           isQueued = false;

    pConfig = reinterpret_cast<ThreadConfig*>(pCfg);
    pCtrl = &pConfig->ctrl;
    pData = &pConfig->data;

    pCtrl->pQueueLock->lock();
    if(false == pData->pq.empty()) {
        isQueued = true;
    }
    pCtrl->pQueueLock->unlock();
    while(true == isQueued) {
        pCtrl->pQueueLock->lock();
        pJob = pData->pq.front();
        if(NULL != pJob) {
            switch(pJob->status) {
                case JobStatus::Submitted:
                    if(FlushRequested == getFlushStatus(pJob->hJob)) {
                        pJob->status = JobStatus::Stopped;
                    } else if(Noflush == getFlushStatus(pJob->hJob)) {
                        pJob->status = JobStatus::Ready;
                    }
                    break;
                case JobStatus::Stopped:
                    break;
                default:
                    break;
            }
            pData->pq.pop_front();
            pCtrl->pQueueLock->unlock();

            if(pJob->status == JobStatus::Ready) {
                dispatchJob(pJob);
            }
            ::free(pJob);
            pJob = NULL;
        }
        pCtrl->pQueueLock->lock();
        if(true == pData->pq.empty()) {
            isQueued = false;
        }
        pCtrl->pQueueLock->unlock();
    }
    return result;
}

void ThreadManager::dispatchJob(RuntimeJob* pJob)
{
    JobFunc func = getJobFunc(pJob->hJob);
    if(NULL != func) {
        func(pJob->pData);
    }
}

Result ThreadManager::flushJob(JobHandle hJob, bool forceFlush)
{
    Result         result = Ok;
    ThreadControl* pCtrl  = getThreadControlByHandle(hJob);

    if(NULL == pCtrl) {
        LOG(INFO) << "pCtrl is null" << endl;
        result = -EFailed;
    } else {
        if(Initialized != getStatus(pCtrl)) {
            LOG(ERROR) << "Thread is not initialized" << endl;
            result = -EFailed;
        } else {
            if(Noflush == getFlushStatus(hJob)) {
                pCtrl->pFlushJobSubmitLock->lock();
                setFlushStatus(hJob, FlushRequested);
                pCtrl->pFlushJobSubmitLock->unlock();

                setFlushBlockStatus(pCtrl, true);
                result = trigger(hJob);
                if(Ok == result) {
                    pCtrl->pFlushLock->lock();
                    if(true == getFlushBlockStatus(pCtrl)) {
                        pCtrl->pFlushOK->wait(pCtrl->pFlushLock->getNativeHandle());
                    }
                    pCtrl->pFlushLock->unlock();
                } else {
                    LOG(ERROR) << "Failed to trigger" << endl;
                }
                if(true == forceFlush) {
                    pCtrl->pFlushJobSubmitLock->lock();
                    setFlushStatus(hJob, Noflush);
                    pCtrl->pFlushJobSubmitLock->unlock();
                }
            }
        }
    }

    return result;
}

Result ThreadManager::stopThreads(JobHandle hJob)
{
    Result         result = Ok;
    ThreadConfig*  pCfg   = getThreadConfigByHandle(hJob);
    ThreadData*    pData  = getThreadDataByHandle(hJob);
    ThreadControl* pCtrl  = getThreadControlByHandle(hJob);

    if((NULL == pCfg) || (NULL == pData) || (NULL == pCtrl)) {
        LOG(ERROR) << "Failed to get threadconfig/threaddata/threadcontrol" << endl;
        result = -EFailed;
    } else {
        result = flushJob(hJob, false);
        pCtrl->pFlushJobSubmitLock->lock();
        setFlushStatus(hJob, Noflush);
        pCtrl->pFlushJobSubmitLock->unlock();

        pCtrl->pThreadLock->lock();
        setStatus(pCtrl, Stopped);
        pCtrl->pReadOK->signal();
        pCtrl->pThreadLock->unlock();

        OsUtils::threadTerminate(pCfg->hWorkThread);

        pCtrl->pReadOK->destroy();
        pCtrl->pReadOK = NULL;

        pCtrl->pThreadLock->destroy();
        pCtrl->pThreadLock = NULL;

        pCtrl->pFlushJobSubmitLock->destroy();
        pCtrl->pFlushJobSubmitLock = NULL;

        pCtrl->pQueueLock->destroy();
        pCtrl->pQueueLock = NULL;

        pCtrl->pFlushLock->destroy();
        pCtrl->pFlushLock = NULL;

        pCtrl->pFlushOK->destroy();
        pCtrl->pFlushLock = NULL;
    }

    return result;
}

Result ThreadManager::postJob(JobHandle hJob, void* pData, uint64_t requestId)
{
    RuntimeJob*    pRuntimeJob = NULL;
    Result         result      = Ok;
    ThreadControl* pCtrl       = NULL;

    pCtrl = getThreadControlByHandle(hJob);
    if(NULL == pCtrl) {
        LOG(ERROR) << "pCtrl is null" << endl;
        result = -EFailed;
    } else {
        if(Initialized != getStatus(pCtrl)) {
            LOG(ERROR) << "Thread is not initialized threadId" << endl;
            result = -EFailed;
        } else {
            void* vbuf = malloc(sizeof(RuntimeJob));//reinterpret_cast<RuntimeJob*>(::calloc(sizeof(RuntimeJob), 1));
            ::memset(vbuf, 0x00, sizeof(RuntimeJob));
            pRuntimeJob = reinterpret_cast<RuntimeJob*>(vbuf);
            if(NULL != pRuntimeJob) {
                pRuntimeJob->hJob = hJob;
                pRuntimeJob->requestId = requestId;
                pRuntimeJob->pData = pData;

                pCtrl->pFlushJobSubmitLock->lock();
                result = addToPriorityQueue(pRuntimeJob);
                pCtrl->pFlushJobSubmitLock->unlock();

                if(Ok != result) {
                    LOG(ERROR) << "Couldn't add job to Priority Queue" << endl;
                } else {
                    result = trigger(hJob);
                    if(Ok != result) {
                        LOG(ERROR) << "Filed to trigger" << endl;
                    }
                }
            } else {
                LOG(ERROR) << "Failed to create runtimejob out of memory" << endl;
                result = -ENoMemory;
            }
        }
    }
    return result;
}

Result ThreadManager::removeJob(JobHandle hJob, void* pRemoveJobData)
{
    Result         result = Ok;
    ThreadData*    pData  = NULL;
    ThreadControl* pCtrl  = NULL;

    pData = getThreadDataByHandle(hJob);
    pCtrl = getThreadControlByHandle(hJob);

    if((NULL == pData) || (NULL == pCtrl)) {
        LOG(ERROR) << "Failed to get threaddata or threadcontrol" << endl;
        result = -EFailed;
    } else {
        pCtrl->pQueueLock->lock();
        for(uint32_t i = 0; i < pData->pq.size(); ++i) {
            RuntimeJob* pJob = pData->pq[i];
            if(pRemoveJobData == pJob->pData) {
                pData->pq.erase(pData->pq.begin() + i);
            }
        }
        pCtrl->pQueueLock->unlock();
    }
    return result;
}

Result ThreadManager::getAllPostedJobs(JobHandle hJob, vector<void*>& rPostedJobs)
{
    Result          result = Ok;
    ThreadData*     pData = NULL;
    ThreadControl*  pCtrl = NULL;

    pData = getThreadDataByHandle(hJob);
    pCtrl = getThreadControlByHandle(hJob);

    if((NULL == pData) || (NULL == pCtrl)) {
        LOG(ERROR) << "Failed to get threaddata/threadcontrol" << endl;
        result = -EFailed;
    } else {
        pCtrl->pQueueLock->lock();
        for(uint32_t i = 0; i < pData->pq.size(); ++i) {
            RuntimeJob* pJob = pData->pq[i];
            if(JobStatus::Submitted == pJob->status) {
                rPostedJobs.push_back(pJob->pData);
            }
        }
        pCtrl->pQueueLock->unlock();
    }
    return result;
}

ThreadManager::ThreadManager()
{
    m_totalNumOfRegisteredJob = 0;
}

ThreadManager::~ThreadManager()
{
    Result result = Ok;

    if(m_totalNumOfRegisteredJob > 0) {
        RegisteredJob* pRegisteredJob = NULL;
        for(uint32_t i = 0; i < MaxRegisteredJobs; i++) {
            pRegisteredJob = &m_registeredJobs[i];
            if(0 != pRegisteredJob->hRegister) {
                result = unregisterJobFamily(NULL, pRegisteredJob->name,
                    pRegisteredJob->hRegister);
                if(Ok != result) {
                    LOG(ERROR) << "Failed to process unregisterJobFamily slot " <<
                        pRegisteredJob->slot << endl;
                }
            }
        }
    }
    m_pRegisteredJobLock->destroy();
    m_pRegisteredJobLock = NULL;
}

Result ThreadManager::initialize(const char* pName)
{
    Result result = Ok;
    if(NULL == pName) {
        pName = "ThreadManager";
    }
    ::strncpy(m_name, pName, sizeof(m_name));
    m_pRegisteredJobLock = NativeMutex::create();
    if(NULL == m_pRegisteredJobLock) {
        LOG(ERROR) << "Failed to initialize " << pName << endl;
        result = -EFailed;
    } else {
        ::memset(&m_registeredJobs, 0x00, sizeof(m_registeredJobs));
        for(uint32_t i = 0; i < MaxNumThread; i++) {
            m_threadWorker[i].threadId = 0;
            m_threadWorker[i].workThreadFunc = NULL;
            m_threadWorker[i].pContext = NULL;
            ::memset(&m_threadWorker[i].ctrl, 0x00, sizeof(ThreadControl));
            m_threadWorker[i].isUsed = false;
        }
        LOG(INFO) << "Initialzed " << pName << endl;
    }

    return result;
}

bool ThreadManager::isJobAlreadyRegistered(JobHandle* phJob)
{
    bool isExist = false;
    if(*phJob != InvalidJobHandle) {
        for(uint32_t i = 0; i < MaxRegisteredJobs; i++) {
            if((true == m_registeredJobs[i].isUsed) &&
                    (*phJob == m_registeredJobs[i].hRegister)) {
                isExist = true;
                break;
            }
        }
    }
    return isExist;
}

bool ThreadManager::isFreeSlotAvailable(uint32_t* pSlot, uint32_t* pCount)
{
    bool result = false;
    m_pRegisteredJobLock->lock();
    for(uint32_t i = 0; i < MaxRegisteredJobs; i++) {
        if((false == m_registeredJobs[i].isUsed) &&
            (false == m_threadWorker[i].isUsed))
        {
            *pSlot = i;
            m_totalNumOfRegisteredJob ++;
            if(0 == m_totalNumOfRegisteredJob) {
                m_totalNumOfRegisteredJob ++;
            }

            if(m_totalNumOfRegisteredJob < MaxRegisteredJobs) {
                *pCount = m_totalNumOfRegisteredJob;
                result = true;
            } else {
                LOG(ERROR) << "Exceed MaxRegisteredJobs " << m_totalNumOfRegisteredJob << endl;
            }
            break;
        }
    }
    m_pRegisteredJobLock->unlock();
    return result;
}

void ThreadManager::dump()
{

}

} // namespace linking
