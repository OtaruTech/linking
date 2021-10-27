#include <glog/logging.h>

#include "messagehub.h"
#include "stringutils.h"

#define TOPIC_PING_CLIENT       "/ping"
#define TOPIC_PONG_SERVER       "/pong"
#define TOPIC_CLIENT_ONLINE     "/client/online"
#define TOPIC_ADD_SERVICE       "/service/add"
#define TOPIC_GET_SERVICE       "/service/get"
#define TOPIC_METHOD_FMT        "/method/%s/%s"

#define CLIENT_KEEPALIVE_TIMEOUT_MS         10000
#define CLIENT_KEEPALIVE_INTERNAL_MS        10000

LINKING_SINGLETON_STATIC_INSTANCE(linking::MessageHub);
namespace linking {

//////////////////////LocalService//////////////////////
MessageHub::LocalService::LocalService(const char* name, sp<MessageHub::MessageQueue> mq)
{
    m_name = name;
    m_spMq = mq;
    m_hub  = &MessageHub::getInstance();
}

MessageHub::LocalService::~LocalService()
{
}

Result MessageHub::LocalService::registerMethod(const char* methodName, MethodHandle handle)
{
    //check if the method is already available in map
    if(isMethodExist(methodName)) {
        return -EFailed;
    }
    MethodData* data = new MethodData;
    string topic = StringUtils::stringFormat(TOPIC_METHOD_FMT,
        m_name.c_str(), methodName);
    data->handle = handle;
    data->topic = topic;
    string key = methodName;
    pair<string, MethodData*> item(key, data);
    pair<MethodMap::iterator, bool> ret = m_methodMap.insert(item);

    return ret.second ? Ok : -EFailed;
}

bool MessageHub::LocalService::isMethodExist(const char* name)
{
    MethodMap::const_iterator lit = m_methodMap.begin();
    while(lit != m_methodMap.end()) {
        string methodName = lit->first;
        if(methodName.compare(name) == 0) {
            return true;
        }
        lit ++;
    }
    return false;
}


//////////////////////MessagQueue//////////////////////
MessageHub::MessageQueue::MessageQueue(const char* id, const char* host, int port)
    : mosquittopp(id)
    , m_isConnected(false)
    , m_hub(NULL)
{
    int keepalive = 60;
    m_hub = &MessageHub::getInstance();
    connect(host, port, keepalive);
}

MessageHub::MessageQueue::~MessageQueue()
{
    disconnect();
}

void MessageHub::MessageQueue::on_connect(int rc)
{
    LOG(INFO) << "on_connect: rc " << rc << endl;
    if(rc == 0) {
        m_isConnected = true;
        if(m_hub != NULL) {
            m_hub->handleConnect();
        }
    }
}

void MessageHub::MessageQueue::on_disconnect(int rc)
{
    LOG(INFO) << "on_disconnect: rc " << rc << endl;
    m_isConnected = false;
}

void MessageHub::MessageQueue::on_message(const struct mosquitto_message* message)
{
    if(m_hub != NULL) {
        m_hub->handleMessage(message);
    }
}

void MessageHub::MessageQueue::on_subscribe(int mid, int qos_count, const int* granted_qos)
{
}

void MessageHub::MessageQueue::on_unsubscribe(int mid)
{
}

Result MessageHub::MessageQueue::sub(const char* topic)
{
    AutoMutex __lock(m_lock);
    LOG(INFO) << "subscribe: " << topic << endl;
    return (subscribe(NULL, topic) == 0) ? Ok : -EFailed;
}

Result MessageHub::MessageQueue::unsub(const char* topic)
{
    AutoMutex __lock(m_lock);
    LOG(INFO) << "unsubscribe: " << topic << endl;
    return (unsubscribe(NULL, topic) == 0) ? Ok : -EFailed;
}

Result MessageHub::MessageQueue::pub(const char* topic, const void* payload, int len)
{
    AutoMutex __lock(m_lock);
    LOG(INFO) << "+send: " << topic << " -> " << (char*)payload << endl;
    return (publish(NULL, topic, len, payload) == 0) ? Ok : -EFailed;
}

bool MessageHub::MessageQueue::isConnected()
{
    return m_isConnected;
}

//////////////////////MessagHub//////////////////////
MessageHub::MessageHub()
    : m_spMq(NULL)
{
    
}

MessageHub::~MessageHub()
{

}

bool MessageHub::threadLoop()
{
    if(m_spMq != NULL) {
        m_spMq->loop_forever();
    }
    return false;
}

Result MessageHub::initialize(const char* id, const char* host,
    int port, bool isDaemon)
{
    if(m_spMq != NULL) {
        m_spMq = NULL;
    }

    MessageQueue* sMq = new MessageQueue(id, host, port);
    if(sMq == NULL) {
        LOG(ERROR) << "Failed to allocate MessageQueue";
        return -EFailed;
    }
    m_spMq = sMq;
    m_clientId = id;
    m_isDaemon = isDaemon;

    return Ok;
}

void MessageHub::destroy()
{
    if(m_spMq != NULL) {
        m_spMq = NULL;
    }
}

void MessageHub::startLooper()
{
    run("MQTT");
}

bool MessageHub::isConnected()
{
    if(m_spMq != NULL) {
        return m_spMq->isConnected();
    } else {
        return false;
    }
}

void MessageHub::handleConnect()
{
    if(m_spMq != NULL) {
        if(m_isDaemon) {
            m_spMq->sub(TOPIC_CLIENT_ONLINE);
            m_spMq->sub(TOPIC_PONG_SERVER);
            m_spMq->sub(TOPIC_ADD_SERVICE);
            m_spMq->sub(TOPIC_GET_SERVICE);
        }
    }
}

void MessageHub::handleMessage(const struct mosquitto_message* message)
{

}

sp<MessageHub::LocalService> MessageHub::createLocalService(const char* name)
{
    if(isLocalServiceExist(name)) {
        LOG(ERROR) << "LocalService " << name << " already exist" << endl;
        return NULL;
    }
    sp<MessageHub::LocalService> service = 
        new MessageHub::LocalService(name, m_spMq);
    if(service == NULL) {
        LOG(ERROR) << "Failed to allocate local service" << endl;
    }
    return service;
}

Result MessageHub::addService(sp<MessageHub::LocalService> service, uint32_t timeout)
{
    return Ok;
}

bool MessageHub::isLocalServiceExist(const char* name)
{
    LocalServiceMap::const_iterator lit = m_localServiceMap.begin();
    while(lit != m_localServiceMap.end()) {
        string serviceName = lit->first;
        if(serviceName.compare(name) == 0) {
            return true;
        }
        lit ++;
    }
    return false;
}

//////////////////////MqClientManager//////////////////////
MqClientManager::MqClientManager()
{
    Result ret = ThreadManager::create(&m_pThreadManager, "MqClientThread");
    if(ret != Ok) {
        LOG(ERROR) << "Failed to create thread manager" << endl;
        return;
    }
    m_destroyClientJob = InvalidJobHandle;
    ret = m_pThreadManager->registerJobFamily(
        clientDestroyJobFunc, "DestroyClientJobFunc", &m_destroyClientJob);
    if(ret != Ok) {
        LOG(ERROR) << "Failed to register job" << endl;
    }
}

MqClientManager::~MqClientManager()
{
    if(m_pThreadManager != NULL) {
        m_pThreadManager->destroy();
    }
}

void MqClientManager::addClient(string& id)
{
    MqClient* client = new MqClient;
    client->id = id;
    client->exit = false;
    client->jobHandle = InvalidJobHandle;
    client->context = this;
    Result ret = m_pThreadManager->registerJobFamily(
        clientJobFunc, "ClientJobFunc", &client->jobHandle);
    if(ret != Ok) {
        LOG(ERROR) << "Failed to register client job" << endl;
        delete client;
        return;
    }
    ret = m_pThreadManager->postJob(client->jobHandle, client, 0);
    if(ret != Ok) {
        LOG(ERROR) << "Failed to post job" << endl;
        delete client;
        return;
    }
    pair<string, MqClient*> item(id, client);
    m_clientMap.insert(item);
}

void MqClientManager::removeClient(MqClient* client)
{
    MqClientMap::iterator item = m_clientMap.find(client->id);
    //MessageHub::getInstance().removeRemoteClient(client->id);
    m_clientMap.erase(item);
    m_pThreadManager->unregisterJobFamily(clientJobFunc, "ClientJobFunc", client->jobHandle);
    delete client;
    client = NULL;
}

void MqClientManager::pong(string& id)
{
    MqClient* client = NULL;
    MqClientMap::iterator item = m_clientMap.find(id);
    if(item != m_clientMap.end()) {
        client = item->second;
    }
    if(client != NULL) {
        client->condition.signal();
    }
}

void* MqClientManager::clientJobFunc(void* data)
{
    MqClient* client = static_cast<MqClient*>(data);
    MqClientManager* manager = static_cast<MqClientManager*>(client->context);

    while(client != NULL && !client->exit) {
        usleep(1000 * CLIENT_KEEPALIVE_INTERNAL_MS);
        //ping the client
        //MessageHub::getInstance().ping(client);
        //wait pong signal
    }
    return NULL;
}

void* MqClientManager::clientDestroyJobFunc(void* data)
{
    MqClient* client = static_cast<MqClient*>(data);
    MqClientManager* manager = static_cast<MqClientManager*>(client->context);
    manager->removeClient(client);
    client = NULL;
    return NULL;
}

} // namespace linking
