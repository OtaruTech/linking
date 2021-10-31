#ifndef __LINKING_MESSAGE_HUB_H__
#define __LINKING_MESSAGE_HUB_H__

#include <mosquittopp.h>
#include <refbase.h>
#include <singleton.h>
#include <thread.h>
#include <uuid.h>
#include "types.h"
#include "message.h"
#include "threadmanager.h"

namespace linking {

using namespace std;

#define MESSAGE_MAX_JOBS    64
typedef Message Response;

typedef void (*MethodHandle)(Message& msg, Response& rsp);

typedef enum {
    ACTION_CLIENT_ONLINE,
    ACTION_SERVER_PING,
    ACTION_SERVICE_ADD,
    ACTION_SERVICE_GET,
    ACTION_CALL_METHOD,
    ACTION_INVALID,
} ACTION_T;

typedef enum {
    STATUS_FAILED = -1,
    STATUS_OK = 0,
} STATUS_T;

struct MethodData {
    MethodHandle handle;
};

typedef map<string, MethodData*> MethodMap;

class MessageHub;
class MqClientManager;
struct MqClient;

#define MQ_MAX_JOBS         64
#define TOPIC_MAX_LEN       64
struct MessageThreadData {
    void*    context;
    size_t   length;
    uint8_t* buffer;
    char     topic[TOPIC_MAX_LEN];
};

struct MessageControl {
    Condition cond;
    Mutex     lock;
    Response* rsp;
};

typedef map<int, MessageControl*> MessageControlMap;

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

        bool isConnected();
        Result sendMessage(const char* id, Message& msg);
        Result sendMessage(const char* id, Message& msg, Response& rsp, uint32_t timeout = 2000);
        Result rspOk(const char* id, int mid);
        Result rspOkWithMsg(const char* id, int mid, Response& rsp);
        Result rspFailed(const char* id, int mid);

    private:
        Result sub(const char* topic);
        Result unsub(const char* topic);
        Result pub(const char* topic, const void* payload, int len);
        static void* handleMessageFunc(void* data);

    protected:
        MessageControlMap   m_ctrlMap;
        Mutex               m_rspLock;
    private:
        friend class MessageHub;
        bool                m_isConnected;
        Mutex               m_lock;
        MessageHub*         m_hub;
        ThreadManager*      m_pThreadManager;
        JobHandle           m_jobs[MQ_MAX_JOBS];
    }; // class MessageQueue
public:
    class LocalService: public RefBase {
    public:
        LocalService(const char* name, sp<MessageHub::MessageQueue> mq);
        ~LocalService();
        Result registerMethod(const char* methodName, MethodHandle handle);
    private:
        bool isMethodExist(const char* name);
    protected:
        string                       m_name;
        MethodMap                    m_methodMap;
    private:
        friend class MessageHub;
        MessageHub*                  m_hub;
        sp<MessageHub::MessageQueue> m_spMq;
    }; // class LocalService

    class RemoteService: public RefBase {
    public:
        RemoteService(const char* name);
        ~RemoteService();
        Result callMethod(const char* methodName, Message& msg, Response& rsp,
                uint32_t timeout = 2000);
    protected:
        string         m_name;
        vector<string> m_methodVec;
        string         m_ownerId;
    private:
        friend class MessageHub;
        
    }; //class RemoteService
protected:
    void handleConnect();
    void handleMessage(Message& msg);
    void handleResponse(Response& rsp);

public:
    typedef map<string, sp<LocalService>>           LocalServiceMap;
    typedef map<string, vector<sp<RemoteService>>>  RemoteServiceMap;
    ~MessageHub();
    virtual bool threadLoop();
    Result initialize(const char* id, const char* host, int port, bool isDaemon = false);
    void destroy();
    void startLooper();
    bool isConnected();
    sp<LocalService> createLocalService(const char* name);
    Result addService(sp<LocalService> service, uint32_t timeout = 2000);
    sp<RemoteService> getService(const char* name, uint32_t timeout = 2000);
    Result callMethod(RemoteService* service, const char* methodName,
            Message& msg, Response& rsp, uint32_t timeout = 2000);

    uint32_t generateUuid();
    void removeRemoteClient(string& id);

    sp<MessageQueue>        m_spMq;

private:
    friend class Singleton<MessageHub>;
    MessageHub();
    bool isLocalServiceExist(const char* name);
    bool isRemoteServiceExist(const char* name);
    sp<RemoteService> getRemoteServiceByName(const char* name);

    static void* processMethodCall(void* data);

private:
    friend class MessageQueue;
    LocalServiceMap         m_localServiceMap;
    RemoteServiceMap        m_remoteServiceMap;
    string                  m_clientId;
    bool                    m_isDaemon;
    sp<MqClientManager>     m_spClientManager;
    ThreadManager*          m_pThreadManager;
    JobHandle               m_jobs[MESSAGE_MAX_JOBS];
    Mutex                   m_jobLock;
}; // class Messagehub

struct MqClient {
    string       id;
    JobHandle    jobHandle;
    bool         exit;
    void*        context;
};

typedef map<string, MqClient*> MqClientMap;

class MqClientManager: public RefBase {
public:
    MqClientManager(MessageHub* hub);
    ~MqClientManager();
    void addClient(string& id);
    void removeClient(MqClient* client);
private:
    static void* clientJobFunc(void* data);
    static void* clientDestroyJobFunc(void* data);
private:
    MessageHub*      m_hub;
    ThreadManager*   m_pThreadManager;
    MqClientMap      m_clientMap;
    JobHandle        m_destroyClientJob;
}; // class MqClientManager

} // namespace linking

#endif
