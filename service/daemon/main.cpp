#include <glog/logging.h>
#include <nlohmann/json.hpp>

#include "threadmanager.h"
#include "osutils.h"
#include "types.h"
#include "message.h"
#include "messagehub.h"

using namespace std;
using namespace linking;

void signalHandle(const char* data, size_t size)
{
    LOG(ERROR) << data;
}

int main() {
    google::InstallFailureSignalHandler();
    google::InstallFailureWriter(&signalHandle);

    MessageHub* hub = &MessageHub::getInstance();
    //hub->initialize("Test", "114.117.4.123", 1883);
    hub->initialize("daemon", "127.0.0.1", 1883, true);
    hub->startLooper();

    while(true) {
        usleep(1000000);
    }
}
