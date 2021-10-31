#include "threadmanager.h"
#include "osutils.h"
#include "types.h"
#include "message.h"
#include "messagehub.h"
#include "stringutils.h"

using namespace std;
using namespace linking;

#define TEST_MAX_COUNT    80

void signalHandle(const char* data, size_t size)
{
    LOG(ERROR) << data;
}

void methodSum(Message& msg, Response& rsp)
{
//    LOG(INFO) << "methodSum: " << msg.toString() << endl;
    int a, b;
    msg.getInt("a", &a);
    msg.getInt("b", &b);
    usleep(10000);
    rsp.setInt("result", (a+b));
//    LOG(INFO) << "methodSum: done" << endl;
}

int main(int argc, char** argv)
{
    google::InstallFailureSignalHandler();
    google::InstallFailureWriter(&signalHandle);

    MessageHub* hub = &MessageHub::getInstance();
    hub->initialize("TestServer", "127.0.0.1", 1883);
    hub->startLooper();

    for(int i = 0; i < TEST_MAX_COUNT; i ++) {
        string serviceName = StringUtils::stringFormat("service-%04d", i);
        sp<MessageHub::LocalService> service = hub->createLocalService(serviceName.c_str());
        service->registerMethod("sum", methodSum);
        Result ret = hub->addService(service);
        if(ret != Ok) {
            LOG(ERROR) << "Add service " << i << " failed" << endl;
        }
    }

    while(true) {
        usleep(1000000);
    }

    return 0;
}
