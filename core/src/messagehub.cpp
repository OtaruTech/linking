#include <glog/logging.h>

#include "messagehub.h"
#include "stringutils.h"

#define TOPIC_SND_FMT                       "/SND/%s/%d"
#define TOPIC_RSP_FMT                       "/RSP/%s/%d"
//#define TOPIC_ONLINE                        "/ONLINE"

#define CLIENT_DAEMON                       "daemon"

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
    data->handle = handle;
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

//////////////////////RemoteService//////////////////////
MessageHub::RemoteService::RemoteService(const char* name)
{
    m_name = name;
    m_methodVec.clear();
}

MessageHub::RemoteService::~RemoteService()
{
}

Result MessageHub::RemoteService::callMethod(const char* methodName, Message& msg,
    Response& rsp, uint32_t timeout)
{
    return MessageHub::getInstance().callMethod(this, methodName, msg, rsp, timeout);
}


//////////////////////MessagQueue//////////////////////
MessageHub::MessageQueue::MessageQueue(const char* id, const char* host, int port)
    : mosquittopp(id)
    , m_isConnected(false)
    , m_hub(NULL)
    , m_pThreadManager(NULL)
{
    int keepalive = 60;
    m_hub = &MessageHub::getInstance();
    connect(host, port, keepalive);
    for(int i = 0; i < MQ_MAX_JOBS; i++) {
        m_jobs[i] = InvalidJobHandle;
    }
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
            string sndTopic = StringUtils::stringFormat("/SND/%s/#", m_hub->m_clientId.c_str());
            string rspTopic = StringUtils::stringFormat("/RSP/%s/#", m_hub->m_clientId.c_str());
            sub(sndTopic.c_str());
            sub(rspTopic.c_str());
            Result ret = ThreadManager::create(&m_pThreadManager, "MessageThread");
            if(ret != Ok) {
                LOG(ERROR) << "Failed to create thread manager" << endl;
                return;
            }
            for(int i = 0; i < MQ_MAX_JOBS; i++) {
                ret = m_pThreadManager->registerJobFamily(
                        handleMessageFunc, "MqJobFunc", &m_jobs[i]);
                if(ret != Ok) {
                    LOG(ERROR) << "Failed to register client job" << endl;
                    return;
                }
            }
            m_hub->handleConnect();
        }
    }
}

void* MessageHub::MessageQueue::handleMessageFunc(void* data)
{
    MessageThreadData* threadData = static_cast<MessageThreadData*>(data);
    MessageQueue *mq = static_cast<MessageQueue*>(threadData->context);

    string topic = threadData->topic;
    //LOG(INFO) << "+recv: " << topic << " -> " << (char*)threadData->buffer << endl;
    Message msg(MESSAGE_BASE64, (const char*)threadData->buffer);
    //LOG(INFO) << "+recv: " << topic << " -> " << msg.toString() << endl;
    if(strncmp(topic.c_str(), "/SND", 4) == 0) {
        MessageHub::getInstance().handleMessage(msg);
    } else if(strncmp(topic.c_str(), "/RSP", 4) == 0) {
        MessageHub::getInstance().handleResponse(msg);
    } else {
        LOG(ERROR) << "Invalid topic" << endl;
    }

    ::free(threadData->buffer);
    delete threadData;
    threadData = NULL;
    return NULL;
}

void MessageHub::MessageQueue::on_disconnect(int rc)
{
    LOG(INFO) << "on_disconnect: rc " << rc << endl;
    m_isConnected = false;
}

void MessageHub::MessageQueue::on_message(const struct mosquitto_message* message)
{
    if(m_hub == NULL) {
        LOG(ERROR) << "on_message: m_hub is NULL" << endl;
        return;
    }

    string topic = message->topic;
#if 0
    //filter online action
    if(topic.compare(TOPIC_ONLINE) == 0) {
        Message msg(MESSAGE_OBJECT, (const char*)message->payload);
        string from;
        msg.getString("from", from);
        m_hub->m_spClientManager->addClient(from);
        return;
    }
#endif
    //LOG(INFO) << "on_message: " << topic << " -> " << (const char*)message->payload << endl;
    MessageThreadData* threadData = new MessageThreadData;
    threadData->context = this;
    threadData->length = message->payloadlen;
    threadData->buffer = (uint8_t *)::malloc(threadData->length + 1);
    ::memset(threadData->buffer, 0x00, threadData->length + 1);
    ::memcpy(threadData->buffer, message->payload, threadData->length);
    ::strncpy(threadData->topic, message->topic, TOPIC_MAX_LEN);

    JobHandle availableHandle = InvalidJobHandle;
    do {
        for(int i = 0; i < MQ_MAX_JOBS; i++) {
            if(m_jobs[i] != InvalidJobHandle
                && m_pThreadManager->isJobAvailable(m_jobs[i]))
            {
                availableHandle = m_jobs[i];
                break;
            }
        }
        if(availableHandle == InvalidJobHandle) {
            ::usleep(1000);
        }
    } while(availableHandle == InvalidJobHandle);
    if(availableHandle != InvalidJobHandle) {
        m_pThreadManager->postJob(availableHandle, threadData, 0);
    } else {
        LOG(ERROR) << "Cannot find available job handle" << endl;
    }
}

void MessageHub::MessageQueue::on_subscribe(int mid, int qos_count, const int* granted_qos)
{
}

void MessageHub::MessageQueue::on_unsubscribe(int mid)
{
}

Result MessageHub::MessageQueue::sendMessage(const char* id, Message& msg)
{
    int mid;
    Result ret = Ok;

    ret = msg.getInt("mid", &mid);
    if(ret != Ok) {
        LOG(ERROR) << "Failed to get message id" << endl;
        return ret;
    }
    string topic = StringUtils::stringFormat(TOPIC_SND_FMT, id, mid);
    string message = msg.toBase64();

    return pub(topic.c_str(), message.c_str(), message.length());
}

Result MessageHub::MessageQueue::sendMessage(const char* id, Message& msg,
    Response& rsp, uint32_t timeout)
{
    int mid;
    Result ret = Ok;

    ret = msg.getInt("mid", &mid);
    if(ret != Ok) {
        LOG(ERROR) << "Failed to get message id" << endl;
        return ret;
    }
    string topic = StringUtils::stringFormat(TOPIC_SND_FMT, id, mid);
    string message = msg.toBase64();

    m_rspLock.lock();
    //create controller
    MessageControl* ctrl = new MessageControl;
    ctrl->rsp = NULL;
    pair<int, MessageControl*> item(mid, ctrl);
    m_ctrlMap.insert(item);
    m_rspLock.unlock();

    ret = pub(topic.c_str(), message.c_str(), message.length());
    if(ret != Ok) {
        LOG(ERROR) << "Failed to publish message" << endl;
        return ret;
    }
    //wait timeout
    chrono::milliseconds timeout_ms(timeout);
    nsecs_t timeout_ns = std::numeric_limits<nsecs_t>::max();
    if(timeout_ms.count() < timeout_ns / 1000 / 1000) {
        timeout_ns = timeout_ms.count() * 1000 * 1000;
    }
    status_t rc = ctrl->cond.waitRelative(ctrl->lock, timeout_ns);
    if(rc < 0) {
        LOG(ERROR) << "Wait response timeout" << endl;
        return -EFailed;
    }
    m_rspLock.lock();
    if(ctrl->rsp != NULL) {
        rsp = *ctrl->rsp;
    }
    MessageControlMap::iterator it = m_ctrlMap.find(mid);
    if(it != m_ctrlMap.end()) {
        m_ctrlMap.erase(it);
        delete ctrl;
    }
    m_rspLock.unlock();

    return Ok;
}

Result MessageHub::MessageQueue::rspOk(const char* id, int mid)
{
    string topic = StringUtils::stringFormat(TOPIC_RSP_FMT, id, mid);
    Message msg;
    msg.setInt("status", STATUS_OK);
    msg.setInt("mid", mid);
    msg.setString("from", m_hub->m_clientId.c_str());
    msg.setString("to", id);
    string message = msg.toBase64();
    return pub(topic.c_str(), message.c_str(), message.length());
}

Result MessageHub::MessageQueue::rspOkWithMsg(const char* id, int mid, Response& rsp)
{
    string topic = StringUtils::stringFormat(TOPIC_RSP_FMT, id, mid);
    Message msg;
    msg.setInt("status", STATUS_OK);
    msg.setInt("mid", mid);
    msg.setString("from", m_hub->m_clientId.c_str());
    msg.setString("to", id);
    msg.setMessage("data", rsp);
    string message = msg.toBase64();
    return pub(topic.c_str(), message.c_str(), message.length());
    return Ok;
}

Result MessageHub::MessageQueue::rspFailed(const char* id, int mid)
{
    string topic = StringUtils::stringFormat(TOPIC_RSP_FMT, id, mid);
    Message msg;
    msg.setInt("status", STATUS_FAILED);
    msg.setInt("mid", mid);
    msg.setString("from", m_hub->m_clientId.c_str());
    msg.setString("to", id);
    string message = msg.toBase64();
    return pub(topic.c_str(), message.c_str(), message.length());
}

Result MessageHub::MessageQueue::sub(const char* topic)
{
    AutoMutex __lock(m_lock);
    //LOG(INFO) << "subscribe: " << topic << endl;
    return (subscribe(NULL, topic) == 0) ? Ok : -EFailed;
}

Result MessageHub::MessageQueue::unsub(const char* topic)
{
    AutoMutex __lock(m_lock);
    //LOG(INFO) << "unsubscribe: " << topic << endl;
    return (unsubscribe(NULL, topic) == 0) ? Ok : -EFailed;
}

Result MessageHub::MessageQueue::pub(const char* topic, const void* payload, int len)
{
    AutoMutex __lock(m_lock);
    //LOG(INFO) << "+send: " << topic << " -> " << (char*)payload << endl;
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

uint32_t MessageHub::generateUuid()
{
    uuid_t uu;
    uuid_generate(uu);
    return (uu[0] << 24 | uu[1] << 16 | uu[2] << 8 | uu[3]) & 0x7FFFFFFF;
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
    if(m_isDaemon) {
        m_spClientManager = new MqClientManager(this);
    } else {
        Result ret = ThreadManager::create(&m_pThreadManager, "CallMethodThread");
        if(ret != Ok) {
            LOG(ERROR) << "Failed to create thread manager" << endl;
            return ret;
        }
        for(int i = 0; i < MESSAGE_MAX_JOBS; i++) {
            m_jobs[i] = InvalidJobHandle;
            ret = m_pThreadManager->registerJobFamily(
                    processMethodCall, "MethodCallFunc", &m_jobs[i]);
            if(ret != Ok) {
                LOG(ERROR) << "Failed to register method call job" << endl;
                return -EFailed;
            }
        }
    }

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

void* MessageHub::processMethodCall(void* ptr)
{
    const char* buf = static_cast<const char*>(ptr);
    //LOG(INFO) << "processMethodCall: " << buf << endl;
    Message msg(MESSAGE_OBJECT, buf);
    MessageHub* hub = &MessageHub::getInstance();

    LocalServiceMap::iterator localServiceItem;
    MethodMap::iterator methodItem;
    MethodData* methodData = NULL;
    Message data;
    Message input;
    string method;
    string serviceName;
    Response rsp;
    string from;
    int mid = 0;

    msg.getInt("mid", &mid);
    msg.getString("from", from);
    msg.getMessage("data", data);
    data.getMessage("input", input);
    data.getString("method", method);
    data.getString("service", serviceName);

    localServiceItem = hub->m_localServiceMap.find(serviceName);
    if(localServiceItem != hub->m_localServiceMap.end()) {
        methodItem = localServiceItem->second->m_methodMap.find(method);
        if(methodItem != localServiceItem->second->m_methodMap.end()) {
            methodData = methodItem->second;
        }
    }
    if(methodData != NULL) {
        methodData->handle(input, rsp);
        hub->m_spMq->rspOkWithMsg(from.c_str(), mid, rsp);
    } else {
        LOG(ERROR) << "Method " << methodItem->first << " not register yet" << endl;
        hub->m_spMq->rspFailed(from.c_str(), mid);
    }
    delete buf;
    buf = NULL;
    return NULL;
}

void MessageHub::handleConnect()
{
    if(m_spMq != NULL && !m_isDaemon) {
        //send online to daemon
        uint32_t uid = generateUuid();
        Message msg;
        msg.setInt("mid", uid);
        msg.setString("from", m_clientId);
        msg.setString("to", CLIENT_DAEMON);
        msg.setInt("action", ACTION_CLIENT_ONLINE);
        Result ret = m_spMq->sendMessage(CLIENT_DAEMON, msg);
        if(ret != Ok) {
            LOG(ERROR) << "Failed to send client online message" << endl;
        }
    } else {
        //m_spMq->sub(TOPIC_ONLINE);
    }
}

void MessageHub::handleMessage(Message& msg)
{
    //LOG(INFO) << "+recv: " << msg.toString() << endl;
    int action = ACTION_INVALID;
    int mid = 0;
    string serviceName;
    string from;
    Message data;
    Response rsp;
    sp<RemoteService> remoteService;
    vector<sp<RemoteService>> serviceVec;
    RemoteServiceMap::iterator remoteServiceItem;
    JobHandle availableHandle = InvalidJobHandle;

    Result ret = msg.getInt("action", &action);
    if(ret != Ok) {
        LOG(ERROR) << "Failed to get action" << endl;
        return;
    }
    msg.getInt("mid", &mid);
    msg.getString("from", from);
    switch(action) {
        case ACTION_CLIENT_ONLINE:
            m_spClientManager->addClient(from);
            break;
        case ACTION_SERVER_PING:
            m_spMq->rspOk(from.c_str(), mid);
            break;
        case ACTION_SERVICE_ADD:
            msg.getMessage("data", data);
            //save service info
            data.getString("service", serviceName);
            if(isRemoteServiceExist(serviceName.c_str())) {
                LOG(ERROR) << "Remote service " << serviceName << " already exist" << endl;
                m_spMq->rspFailed(from.c_str(), mid);
                return;
            }
            remoteService = new RemoteService(serviceName.c_str());
            remoteService->m_ownerId = from;
            data.getStringArray("methods", remoteService->m_methodVec);
            remoteServiceItem = m_remoteServiceMap.find(from);
            if(remoteServiceItem != m_remoteServiceMap.end()) {
                serviceVec = remoteServiceItem->second;
            }
            serviceVec.push_back(remoteService);
            if(remoteServiceItem == m_remoteServiceMap.end()) {
                string key = from;
                pair<string, vector<sp<RemoteService>>> it(key, serviceVec);
                m_remoteServiceMap.insert(it);
            } else {
                m_remoteServiceMap[from] = serviceVec;
            }
            m_spMq->rspOk(from.c_str(), mid);
            break;
        case ACTION_SERVICE_GET:
            msg.getMessage("data", data);
            data.getString("service", serviceName);
            remoteService = getRemoteServiceByName(serviceName.c_str());
            if(remoteService == NULL) {
                LOG(ERROR) << "Cannot find remote service " << serviceName << endl;
                m_spMq->rspFailed(from.c_str(), mid);
                break;;
            }
            rsp.setString("service", serviceName);
            rsp.setStringArray("methods", remoteService->m_methodVec);
            rsp.setString("owner", remoteService->m_ownerId);
            m_spMq->rspOkWithMsg(from.c_str(), mid, rsp);
            break;
        case ACTION_CALL_METHOD: {
            AutoMutex __lock(m_jobLock);
            do {
                for(int i = 0; i < MESSAGE_MAX_JOBS; i++) {
                    if(m_jobs[i] != InvalidJobHandle
                        && m_pThreadManager->isJobAvailable(m_jobs[i]))
                    {
                        availableHandle = m_jobs[i];
                        break;
                    }
                }
                if(availableHandle == InvalidJobHandle) {
                    ::usleep(1000);
                }
            } while(availableHandle == InvalidJobHandle);
            if(availableHandle != InvalidJobHandle) {
                char* buf = (char*)::malloc(msg.toString().length() + 1);
                ::memset(buf, 0x00, msg.toString().length() + 1);
                ::memcpy(buf, msg.toString().c_str(), msg.toString().length());
                m_pThreadManager->postJob(availableHandle, buf, 0);
            } else {
                LOG(ERROR) << "Cannot find available job handle" << endl;
                break;
            }
        }
            break;
        default:
            break;
    }
}

void MessageHub::handleResponse(Response& rsp)
{
    //LOG(INFO) << "+recv: " << rsp.toString() << endl;
    int mid = 0;
    string from;
    rsp.getInt("mid", &mid);
    rsp.getString("from", from);
    m_spMq->m_rspLock.lock();
    MessageControlMap::iterator item = m_spMq->m_ctrlMap.find(mid);
    if(item != m_spMq->m_ctrlMap.end()) {
        MessageControl* ctrl = item->second;
        ctrl->rsp = new Response(MESSAGE_OBJECT, rsp.toString());
        ctrl->cond.signal();
    } else {
        LOG(ERROR) << "Failed to find Message Control" << endl;
    }
    m_spMq->m_rspLock.unlock();
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
    uint32_t uid = generateUuid();
    Message msg;
    msg.setInt("mid", uid);
    msg.setString("from", m_clientId);
    msg.setString("to", CLIENT_DAEMON);
    msg.setInt("action", ACTION_SERVICE_ADD);
    Message data;
    data.setString("service", service->m_name);
    vector<string> methods;
    MethodMap::const_iterator lit = service->m_methodMap.begin();
    while(lit != service->m_methodMap.end()) {
        methods.push_back(lit->first);
        lit ++;
    }
    data.setStringArray("methods", methods);
    msg.setMessage("data", data);
    Response rsp;
    Result ret = m_spMq->sendMessage(CLIENT_DAEMON, msg, rsp, timeout);
    if(ret != Ok) {
        LOG(ERROR) << "Failed to send add service message" << endl;
        return -EFailed;
    }
    int status;
    rsp.getInt("status", &status);
    if(status != STATUS_OK) {
        return -EFailed;
    }
    pair<string, sp<LocalService>> item(service->m_name, service);
    pair<LocalServiceMap::iterator, bool> rc = m_localServiceMap.insert(item);
    return rc.second ? Ok : -EFailed;
}

sp<MessageHub::RemoteService> MessageHub::getService(const char* name, uint32_t timeout)
{
    uint32_t uid = generateUuid();
    Message msg;
    msg.setInt("mid", uid);
    msg.setString("from", m_clientId);
    msg.setString("to", CLIENT_DAEMON);
    msg.setInt("action", ACTION_SERVICE_GET);
    Message data;
    data.setString("service", name);
    msg.setMessage("data", data);
    Response rsp;
    Result ret = m_spMq->sendMessage(CLIENT_DAEMON, msg, rsp, timeout);
    if(ret != Ok) {
        LOG(ERROR) << "Failed to send get service message" << endl;
        return NULL;
    }
    int status;
    rsp.getInt("status", &status);
    if(status != STATUS_OK) {
        return NULL;
    }
    Message rspData;
    string serviceName;
    rsp.getMessage("data", rspData);
    rspData.getString("service", serviceName);
    sp<MessageHub::RemoteService> service = new RemoteService(serviceName.c_str());
    rspData.getStringArray("methods", service->m_methodVec);
    rspData.getString("owner", service->m_ownerId);

    return service;
}

Result MessageHub::callMethod(MessageHub::RemoteService* service, const char* methodName,
    Message& input, Response& output, uint32_t timeout)
{
    uint32_t uid = generateUuid();
    Message msg;
    msg.setInt("mid", uid);
    msg.setString("from", m_clientId);
    msg.setString("to", service->m_ownerId);
    msg.setInt("action", ACTION_CALL_METHOD);
    Message data;
    data.setMessage("input", input);
    data.setString("method", methodName);
    data.setString("service", service->m_name);
    msg.setMessage("data", data);

    Response rsp;
    Result ret = m_spMq->sendMessage(service->m_ownerId.c_str(), msg, rsp, timeout);
    if(ret != Ok) {
        LOG(ERROR) << "Failed to call method" << endl;
        return -EFailed;
    }
    return rsp.getMessage("data", output);
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

bool MessageHub::isRemoteServiceExist(const char* name)
{
    RemoteServiceMap::const_iterator lit = m_remoteServiceMap.begin();
    while(lit != m_remoteServiceMap.end()) {
        vector<sp<RemoteService>> serviceVec = lit->second;
        for(int i = 0; i < serviceVec.size(); i++) {
            sp<RemoteService> sService = serviceVec[i];
            if(sService->m_name.compare(name) == 0) {
                return true;
            }
        }
        lit ++;
    }
    return false;
}

sp<MessageHub::RemoteService> MessageHub::getRemoteServiceByName(const char* name)
{
    RemoteServiceMap::const_iterator lit = m_remoteServiceMap.begin();
    while(lit != m_remoteServiceMap.end()) {
        vector<sp<RemoteService>> serviceVec = lit->second;
        for(int i = 0; i < serviceVec.size(); i++) {
            sp<RemoteService> sService = serviceVec[i];
            if(sService->m_name.compare(name) == 0) {
                return sService;
            }
        }
    }
    return NULL;
}

void MessageHub::removeRemoteClient(string& id)
{
    RemoteServiceMap::iterator item = m_remoteServiceMap.find(id);
    if(item != m_remoteServiceMap.end()) {
        vector<sp<RemoteService>> spServices = item->second;
        for(int i = 0; i < spServices.size(); i++) {
            //clear the remote service strong point
            spServices[i].clear();
            spServices[i] = NULL;
        }
        m_remoteServiceMap.erase(item);
    }
    LOG(INFO) << "Remove remote client " << id << " done" << endl;
}

//////////////////////MqClientManager//////////////////////
MqClientManager::MqClientManager(MessageHub* hub)
{
    m_hub = hub;
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
    m_clientMap.erase(item);
    m_pThreadManager->flushJob(client->jobHandle, false);
    m_pThreadManager->removeJob(client->jobHandle, client);
    m_pThreadManager->unregisterJobFamily(clientJobFunc, "ClientJobFunc", client->jobHandle);
    m_hub->removeRemoteClient(client->id);
    delete client;
    client = NULL;
}

void* MqClientManager::clientJobFunc(void* data)
{
    MqClient* client = static_cast<MqClient*>(data);
    MqClientManager* manager = static_cast<MqClientManager*>(client->context);

    LOG(INFO) << "clientJobFunc: enter " << client->id <<  endl;
    while(client != NULL && !client->exit) {
        usleep(1000 * CLIENT_KEEPALIVE_INTERNAL_MS);
        //ping the client
        uint32_t uid = manager->m_hub->generateUuid();
        Message msg;
        msg.setInt("mid", uid);
        msg.setString("from", CLIENT_DAEMON);
        msg.setString("to", client->id);
        msg.setInt("action", ACTION_SERVER_PING);
        Response rsp;
        Result ret = manager->m_hub->m_spMq->sendMessage(client->id.c_str(), msg, rsp, 3000);
        if(ret != Ok) {
            LOG(ERROR) << "PING Client " << client->id << " failed" << endl;
            client->exit = true;
            manager->m_pThreadManager->postJob(manager->m_destroyClientJob, client, 0);
        }
    }
    LOG(INFO) << "clientJobFunc: exit" << endl;
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
