#include "message.h"

namespace linking {

#if 0
Message::Message()
    : m_lock(NULL)
{
    m_root = json::object();
    m_lock = NativeMutex::create();
}
#endif

Message::Message(MESSAGE_TYPE type)
    : m_lock(NULL)
    , m_name("")
{
    if(type == MESSAGE_OBJECT) {
        m_root = json::object();
    } else if(type == MESSAGE_ARRAY) {
        m_root = json::array();
    }
    m_lock = NativeMutex::create();
}

Message::Message(MESSAGE_TYPE type, string& buffer)
    : m_lock(NULL)
    , m_name("")
{
    if(type == MESSAGE_OBJECT) {
        m_root = json::object();
        m_root = json::parse(buffer);
    } else if(type == MESSAGE_ARRAY) {
        m_root = json::array();
        m_root = json::parse(buffer);
    } else if(type == MESSAGE_BASE64) {
        string decode = base64_decode(buffer);
        m_root = json::parse(decode);
    }
    m_lock = NativeMutex::create();
}

Message::~Message()
{
    if(m_lock != NULL) {
        m_lock->destroy();
        m_lock = NULL;
    }
}

void Message::setName(const char* name)
{
    m_name = name;
}

const char* Message::getName()
{
    return m_name.c_str();
}

void Message::reload(string& buffer)
{
    m_root = buffer;
}

json& Message::getJson()
{
    return m_root;
}

void Message::setJson(json& js)
{
    m_root = js;
}

void Message::setInt(string key, int val)
{
    m_lock->lock();
    m_root[key] = val;
    m_lock->unlock();
}

void Message::setBool(string key, bool val)
{
    m_lock->lock();
    m_root[key] = val;
    m_lock->unlock();
}

void Message::setFloat(string key, float val)
{
    m_lock->lock();
    m_root[key] = val;
    m_lock->unlock();
}

void Message::setString(string key, string val)
{
    m_lock->lock();
    m_root[key] = val;
    m_lock->unlock();
}

void Message::setMessage(string key, Message& val)
{
    m_lock->lock();
    m_root[key] = val.getJson();
    m_lock->unlock();
}

void Message::setIntArray(string key, vector<int>& array)
{
    m_lock->lock();
    json vec(array);
    m_root[key] = vec;
    m_lock->unlock();
}

void Message::setBoolArray(string key, vector<bool>& array)
{
    m_lock->lock();
    json vec(array);
    m_root[key] = vec;
    m_lock->unlock();
}

void Message::setFloatArray(string key, vector<float>& array)
{
    m_lock->lock();
    json vec(array);
    m_root[key] = vec;
    m_lock->unlock();
}

void Message::setStringArray(string key, vector<string>& array)
{
    m_lock->lock();
    json vec(array);
    m_root[key] = vec;
    m_lock->unlock();
}

Result Message::getInt(string key, int* val)
{
    Result ret = Ok;
    m_lock->lock();
    if(m_root[key].is_number()) {
        *val = m_root[key];
    } else {
        LOG(ERROR) << "key '" << key << "' is not number" << endl;
        ret = -EFailed;
    }
    m_lock->unlock();
    return ret;
}

Result Message::getBool(string key, bool* val)
{
    Result ret = Ok;
    m_lock->lock();
    if(m_root[key].is_boolean()) {
        *val = m_root[key];
    } else {
        LOG(ERROR) << "key '" << key << "' is not boolean" << endl;
        ret = -EFailed;
    }
    m_lock->unlock();
    return ret;
}

Result Message::getFloat(string key, float* val)
{
    Result ret = Ok;
    m_lock->lock();
    if(m_root[key].is_number_float()) {
        *val = m_root[key];
    } else {
        LOG(ERROR) << "key '" << key << "' is not float" << endl;
        ret = -EFailed;
    }
    m_lock->unlock();
    return ret;
}

Result Message::getString(string key, string& val)
{
    Result ret = Ok;
    m_lock->lock();
    if(m_root[key].is_string()) {
        val = m_root[key];
    } else {
        LOG(ERROR) << "key '" << key << "' is not float" << endl;
        ret = -EFailed;
    }
    m_lock->unlock();
    return ret;
}

Result Message::getMessage(string key, Message& val)
{
    Result ret = Ok;
    m_lock->lock();
    if(m_root[key].is_object()) {
        val.setJson(m_root[key]);
    } else {
        LOG(ERROR) << "key '" << key << "' is not float" << endl;
        ret = -EFailed;
    }
    m_lock->unlock();
    return ret;
}

Result Message::getIntArray(string key, vector<int>& array)
{
    Result ret = Ok;
    m_lock->lock();
    if(m_root[key].is_array()) {
        for(auto& item : m_root[key]) {
            array.push_back(item);
        }
    } else {
        LOG(ERROR) << "key '" << key << "' is not array" << endl;
        ret = -EFailed;
    }
    m_lock->unlock();

    return ret;
}

Result Message::getBoolArray(string key, vector<bool>& array)
{
    Result ret = Ok;
    m_lock->lock();
    if(m_root[key].is_array()) {
        for(auto& item : m_root[key]) {
            array.push_back(item);
        }
    } else {
        LOG(ERROR) << "key '" << key << "' is not array" << endl;
        ret = -EFailed;
    }
    m_lock->unlock();

    return ret;
}

Result Message::getFloatArray(string key, vector<float>& array)
{
    Result ret = Ok;
    m_lock->lock();
    if(m_root[key].is_array()) {
        for(auto& item : m_root[key]) {
            array.push_back(item);
        }
    } else {
        LOG(ERROR) << "key '" << key << "' is not array" << endl;
        ret = -EFailed;
    }
    m_lock->unlock();

    return ret;
}

Result Message::getStringArray(string key, vector<string>& array)
{
    Result ret = Ok;
    m_lock->lock();
    if(m_root[key].is_array()) {
        for(auto& item : m_root[key]) {
            array.push_back(item);
        }
    } else {
        LOG(ERROR) << "key '" << key << "' is not array" << endl;
        ret = -EFailed;
    }
    m_lock->unlock();

    return ret;
}

string& Message::toString()
{
    m_lock->lock();
    m_string = m_root.dump();
    m_lock->unlock();
    return m_string;
}

string& Message::toBase64()
{
    m_base64 = base64_encode(toString());
    return m_base64;
}

void Message::dump()
{
    m_lock->lock();
    LOG(INFO) << m_name << ": " << m_root << endl;
    string encode = base64_encode(toString());
    LOG(INFO) << m_name << ": " << encode << endl;
    m_lock->unlock();
}

} //namespace linking
