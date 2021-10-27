#include <glog/logging.h>
#include <nlohmann/json.hpp>

#include "threadmanager.h"
#include "osutils.h"
#include "types.h"
#include "message.h"
#include "messagehub.h"

using namespace std;
using namespace linking;
using json = nlohmann::json;

void test_json()
{

}

void test_message()
{
    Message msg(MESSAGE_OBJECT);
    msg.setName("Test");
    msg.setInt("age", 32);
    vector<int> intArr = {1, 2, 3, 4, 5, 6};
    msg.setIntArray("intArr", intArr);
    vector<int> iArr;
    auto ret = msg.getIntArray("intArr", iArr);
    LOG(INFO) << "ret: " << ret << endl;
    for(int i = 0; i < iArr.size(); i++) {
        LOG(INFO) << " " << iArr[i] << endl;
    }

    vector<string> strArr = {
        "AAA", "BBB", "CCC", "DDD"
    };
    msg.setStringArray("strArr", strArr);
    vector<string> sArr;
    ret = msg.getStringArray("strArr", sArr);
    LOG(INFO) << "ret: " << ret << endl;
    for(int i = 0; i < sArr.size(); i++) {
        LOG(INFO) << " " << sArr[i] << endl;
    }
    string n = msg.getName();
    LOG(INFO) << n << ": " << msg.toString() << endl;

    int age;
    msg.getInt("age", &age);
    LOG(INFO) << "age: " << age << endl;
    msg.dump();

    string buf = R"(
        {"name": "jay", "score": 99}
    )";
    Message msg2(MESSAGE_OBJECT, buf);
    msg2.dump();
    int score;
    msg2.getInt("score", &score);
    LOG(INFO) << "Score: " << score << endl;

    Message msg3(MESSAGE_OBJECT);
    msg3.setString("name", "jayzhang");
    string name;
    msg3.getString("name_not_exist", name);

    LOG(INFO) << "name: " << name << endl;
    msg3.setMessage("info", msg2);
    msg3.dump();
    Message info(MESSAGE_OBJECT);
    msg3.getMessage("info", info);
    info.dump();

    string raw = "eyJhZ2UiOjMyLCJpbnRBcnIiOlsxLDIsMyw0LDUsNl0sInN0ckFyciI6WyJBQUEiLCJCQkIiLCJDQ0MiLCJEREQiXX0=";
    Message base64Msg(MESSAGE_BASE64, raw);

    vector<string> sArr2;
    ret = msg.getStringArray("strArr", sArr2);
    for(int i = 0; i < sArr2.size(); i++) {
        LOG(INFO) << " " << sArr2[i] << endl;
    }
    LOG(INFO) << "base64Msg: " << base64Msg.toString() << endl;
}

void test_mqtt()
{
    MessageHub* hub = &MessageHub::getInstance();
    //hub->initialize("Test", "114.117.4.123", 1883);
    hub->initialize("Test", "127.0.0.1", 1883);
    hub->startLooper();

    while(true) {
        usleep(1000000);
    }
}

int main() {
    test_message();
    test_mqtt();
    return 0;
}
