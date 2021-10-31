#include "threadmanager.h"
#include "osutils.h"
#include "types.h"
#include "message.h"
#include "messagehub.h"
#include "stringutils.h"

using namespace std;
using namespace linking;

#define TEST_MAX_COUNT    60

void signalHandle(const char* data, size_t size)
{
    LOG(ERROR) << data;
}

sp<MessageHub::RemoteService> services[TEST_MAX_COUNT];
JobHandle jobs[TEST_MAX_COUNT];
ThreadManager* threadManager;

static void* testJobFunc(void* ptr)
{
    MessageHub* hub = &MessageHub::getInstance();
    int* count = static_cast<int*>(ptr);
//    LOG(INFO) << "count: " << *count << endl;

    Message msg;
    msg.setInt("a", 100 * *count);
    msg.setInt("b", 200 * *count);
    Response rsp;
    services[*count]->callMethod("sum", msg, rsp);
    //LOG(INFO) << "Result: " << rsp.toString() << endl;
    int result;
    rsp.getInt("result", &result);
    if(result != (100 * *count + 200 * *count)) {
        LOG(ERROR) << "Result: " << rsp.toString() << endl;
    }
    return NULL;
}

int main(int argc, char** argv)
{
    google::InstallFailureSignalHandler();
    google::InstallFailureWriter(&signalHandle);

    MessageHub* hub = &MessageHub::getInstance();
    //hub->initialize("Test", "114.117.4.123", 1883);
    hub->initialize("TestClient", "127.0.0.1", 1883);
    hub->startLooper();
    int counts[TEST_MAX_COUNT];

    ThreadManager::create(&threadManager, "TestThreadManager");
    for(int i = 0; i < TEST_MAX_COUNT; i ++) {
        jobs[i] = InvalidJobHandle;
        threadManager->registerJobFamily(testJobFunc, "testJobFunc", &jobs[i]);
        string serviceName = StringUtils::stringFormat("service-%04d", i);
        services[i] = hub->getService(serviceName.c_str());
    }

#if 1
#endif

    while(true) {
        usleep(1500000);
    for(int i = 0; i < TEST_MAX_COUNT; i ++) {
        counts[i] = i;
        threadManager->postJob(jobs[i], &counts[i], 0);
    }
    }

    return 0;
}
