#ifndef __LINKING_MESSAGE_HUB_H__
#define __LINKING_MESSAGE_HUB_H__

#include <mosquittopp.h>
#include <refbase.h>
#include <singleton.h>
#include <thread.h>
#include "types.h"
#include "message.h"
#include "threadmanager.h"

namespace linking {

using namespace std;

typedef void (*MethodHandle)(Message& in, Message& out);

struct MethodData {
    string       topic;
    MethodHandle handle;
};

typedef map<string, MethodData*> MethodMap;

class MessageHub;

class MessageHub: public Thread, public Singleton<MessageHub> {
private:
    class MessageQueue: public mosqpp::mosquittopp, public RefBase {
    public:
        MessageQueue(const char* id, const char* host, int port);
        ~MessageQueue();

        /* callback functions */
        void on_connect(int rc);
        void on_disconnect(int rc);
        void on_message(const struct mosquitto_message* message);
        void on_subscribe(int mid, int qos_count, const int* granted_qos);
        void on_unsubscribe(int mid);

        Result sub(const char* topic);
        Result unsub(const char* topic);
        Result pub(const char* topic, const void* payload, int len);
        bool isConnected();

    private:
        bool        m_isConnected;
        Mutex       m_lock;
        MessageHub* m_hub;
    }; // class MessageQueue
public:
    class LocalService: public RefBase {
    public:
        LocalService(const char* name, sp<MessageHub::MessageQueue> mq);
        ~LocalService();
        Result registerMethod(const char* methodName, MethodHandle handle);
    private:
        bool isMethodExist(const char* name);
    private:
        MessageHub*                  m_hub;
        sp<MessageHub::MessageQueue> m_spMq;
        string                       m_name;
        MethodMap                    m_methodMap;
    };
protected:
    void handleConnect();
    void handleMessage(const struct mosquitto_message* message);

public:
    typedef map<string, sp<LocalService>>           LocalServiceMap;
    ~MessageHub();
    virtual bool threadLoop();
    Result initialize(const char* id, const char* host, int port, bool isDaemon = false);
    void destroy();
    void startLooper();
    bool isConnected();
    sp<LocalService> createLocalService(const char* name);
    Result addService(sp<LocalService> service, uint32_t timeout = 2000);

private:
    friend class Singleton<MessageHub>;
    MessageHub();
    bool isLocalServiceExist(const char* name);

private:
    friend class       MessageQueue;
    LocalServiceMap    m_localServiceMap;

    sp<MessageQueue>   m_spMq;
    string             m_clientId;
    bool               m_isDaemon;

}; // class Messagehub

struct MqClient {
    string       id;
    JobHandle    jobHandle;
    Mutex        lock;
    Condition    condition;
    bool         exit;
    void*        context;
};

typedef map<string, MqClient*> MqClientMap;

class MqClientManager: public RefBase {
public:
    MqClientManager();
    ~MqClientManager();
    void addClient(string& id);
    void removeClient(MqClient* client);
    void pong(string& id);
private:
    static void* clientJobFunc(void* data);
    static void* clientDestroyJobFunc(void* data);
private:
    ThreadManager*   m_pThreadManager;
    MqClientMap      m_clientMap;
    JobHandle        m_destroyClientJob;
}; // class MqClientManager

} // namespace linking

#endif
