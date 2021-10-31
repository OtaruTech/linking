#ifndef __LINKING_MESSAGE_H__
#define __LINKING_MESSAGE_H__

#include <nlohmann/json.hpp>
#include <string>
#include <glog/logging.h>
#include "osutils.h"
#include "base64.h"

namespace linking {
using namespace std;
using json = nlohmann::json;

typedef enum {
    MESSAGE_OBJECT,
    MESSAGE_ARRAY,
    MESSAGE_BASE64,
} MESSAGE_TYPE;

class Message {
public:
    Message(MESSAGE_TYPE type = MESSAGE_OBJECT);
    Message(MESSAGE_TYPE type, string& buffer);
    Message(MESSAGE_TYPE type, const char* buffer);
    ~Message();

    void setName(const char* name);
    const char* getName();
    void reload(string& buffer);

    void setInt(string key, int val);
    void setBool(string key, bool val);
    void setFloat(string key, float val);
    void setString(string key, string val);
    void setMessage(string key, Message& val);
    void setIntArray(string key, vector<int>& array);
    void setBoolArray(string key, vector<bool>& array);
    void setFloatArray(string key, vector<float>& array);
    void setStringArray(string key, vector<string>& array);

    Result getInt(string key, int* val);
    Result getBool(string key, bool* val);
    Result getFloat(string key, float* val);
    Result getString(string key, string& val);
    Result getMessage(string key, Message& val);
    Result getIntArray(string key, vector<int>& array);
    Result getBoolArray(string key, vector<bool>& array);
    Result getFloatArray(string key, vector<float>& array);
    Result getStringArray(string key, vector<string>& array);

    string& toString();
    string& toBase64();
    void dump();

private:
    json& getJson();
    void setJson(json& js);

    NativeMutex* m_lock;
    json   m_root;
    string m_string;
    string m_base64;
    string m_name;

}; // class Message

} // namespace linking

#endif
