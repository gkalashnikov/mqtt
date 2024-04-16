#include "mqtt_broker.h"
#include "network_server.h"
#include "mqtt_connect_packet.h"
#include "mqtt_ping_packet.h"
#include "mqtt_publish_packet.h"
#include "mqtt_disconnect_packet.h"
#include "mqtt_subscribe_packet.h"
#include "mqtt_constants.h"
#include "mqtt_encoder.h"
#include "mqtt_decoder.h"
#include "mqtt_password_file.h"

#include <QUuid>
#include <QTimer>

#include "logger.h"

using namespace Mqtt;
using namespace Mqtt::Store;

class SubscribeOptionsSaver
{
public:
    SubscribeOptionsSaver(SubscribeOptions & options)
        :target(options)
        ,saved(options)
    { }

    ~SubscribeOptionsSaver()
    { target = saved; }

private:
    SubscribeOptions & target;
    SubscribeOptions saved;
};

Broker::Broker(Store::IFactory * storerFactory, QObject * parent)
    :QObject(parent)
    ,isQoS0QueueEnabled(false)
    ,maxFlowMessages{  Constants::DefaultQoS0FlowRate
                      ,Constants::DefaultQoS1FlowRate
                      ,Constants::DefaultQoS2FlowRate }
    ,banDuration(Constants::DefaultBanDuration)
    ,banAccumulative(false)
    ,statistic(new Statistic(this))
    ,subcount(0)
    ,storerFactory(storerFactory)
    ,retainPackets(storerFactory->createStorer(QStringLiteral("retained")))
    ,sessionsStorer(storerFactory->createStorer(QStringLiteral("sessions")))
    ,sharedSubscriptionsStorer(storerFactory->createStorer(QStringLiteral("sharedSubscriptions")))
    ,passFile(Q_NULLPTR)
{
    QTimer::singleShot(0, Qt::PreciseTimer, this, &Broker::initialize);
}

quint32 Broker::maxFlowPerSecond(QoS qos) const
{
    switch (qos)
    {
        case QoS::Value_0: return maxFlowMessages[0];
        case QoS::Value_1: return maxFlowMessages[1];
        case QoS::Value_2: return maxFlowMessages[2];
        default: break;
    }
    return 0;
}

void Broker::setMaxFlowPerSecond(QoS qos, quint32 count)
{
    switch (qos)
    {
        case QoS::Value_0: maxFlowMessages[0] = count; return;
        case QoS::Value_1: maxFlowMessages[1] = count; return;
        case QoS::Value_2: maxFlowMessages[2] = count; return;
        default: break;
    }
}

void Broker::setBanDuration(quint32 seconds, bool accumulative)
{
    banDuration = seconds;
    banAccumulative = accumulative;
}

Broker::~Broker()
{
    sessionsNew.clear();
    sessionsByConn.clear();
    sessions.clear();

    retainPackets.syncAll();

    delete sessionsStorer;
    sessionsStorer = Q_NULLPTR;

    delete sharedSubscriptionsStorer;
    sharedSubscriptionsStorer = Q_NULLPTR;

    delete passFile;
    passFile = Q_NULLPTR;
}

void Broker::initialize()
{
    startPublishStatisticTimer();
    loadSessions();
    loadSharedSubscriptions();
}

void Broker::sessionExpired()
{
    statistic->increaseExpiredClients();
    Session * session = qobject_cast<Session*>(sender());
    if (session != Q_NULLPTR) {
        // remove later
        QTimer::singleShot(0, Qt::TimerType::CoarseTimer, std::bind(&SessionsStore::remove, &sessions, (session->clientId())));
        QTimer::singleShot(0, Qt::TimerType::CoarseTimer, std::bind(&Broker::updateClientsStatistic, this));
    }
}

void Broker::publishStatistic()
{
    publishBrokerInfo();
    publishMqttClientsInfo();
    publishSubscriptionsInfo();
    publishMqttMessRecvInfo();
    publishMqttMessSentInfo();
    publishMqttMessDropInfo();
    publishMqttPublishRecvInfo();
    publishMqttPublishSentInfo();
    publishMqttPublishDropInfo();
    publishNetworkLoadInfo();
}

bool Broker::event(QEvent * event)
{
    switch (static_cast<Network::Event::Type>(event->type()))
    {
        case Network::Event::Type::Data: {
            if (Network::Event::Data * e = dynamic_cast<Network::Event::Data *>(event)) {
                e->accept();
                handleNetworkData(e->connectionId, e->data);
                return true;
            }
            break;
        }
        case Network::Event::Type::IncomingConnection: {
            if (Network::Event::IncomingConnection * e = dynamic_cast<Network::Event::IncomingConnection *>(event)) {
                e->accept();
                handleIncomingConnection(e->connection);
                return true;
            }
            break;
        }
        case Network::Event::Type::CloseConnection: {
            if (Network::Event::CloseConnection * e = dynamic_cast<Network::Event::CloseConnection *>(event)) {
                e->accept();
                handleCloseConnection(e->connectionId);
                return true;
            }
            break;
        }
        case Network::Event::Type::WillUpgraded: {
            if (Network::Event::WillUpgraded * e = dynamic_cast<Network::Event::WillUpgraded *>(event)) {
                e->accept();
                handleConnectionUpgraded(e->connectionId);
                return true;
            }
            break;
        }
    }
    return QObject::event(event);
}

bool Broker::setPasswordFile(const QString & filePath)
{
    if (passFile != Q_NULLPTR) {
        delete passFile;
        passFile = Q_NULLPTR;
    }

    passFile = new PasswordFile(filePath);

    return passwordFile()->sync();
}

PasswordFile * Broker::passwordFile()
{
    return dynamic_cast<PasswordFile*>(passFile);
}

SessionPtr Broker::createSession()
{
    Session * s = new Mqtt::Session(this);
    SessionPtr session = SessionPtr(s, std::bind(&Broker::sessionDeleter, this, s));
    connect(s, &Session::expired, this, &Broker::sessionExpired);
    return session;
}

void Broker::sessionDeleter(Session * session)
{
    session->beforeDelete();
    if (!session->clientId().isEmpty()) {
        if (session->hasBeenExpired() || session->isClean())
            sessionsStorer->remove(session->clientId());
        else
            sessionsStorer->store(session->clientId(), session->serialize());
    }
    statistic->decreaseSubscriptionCount(qint32(session->subscriptions().count()));
    removeSharedSubscriptions(session);
    delete session;
}

void Broker::startPublishStatisticTimer()
{
    QTimer * timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &Broker::publishStatistic);
    timer->start(5000);
}

void Broker::loadSessions()
{
    sessionsStorer->beginReadKeys();
    while (sessionsStorer->nextKeyAvailable()) {
        SessionPtr session = createSession();
        session->unserialize(sessionsStorer->load(sessionsStorer->nextKey()));
        if (session->clientId().isEmpty() || session->isBanned())
            continue;
        sessions.insert(session->clientId(), session);
        session->setPendingPacketsStorer(storerFactory->createStorer(QStringLiteral("pending/") + session->clientId()));
        statistic->increaseSubscriptionCount(qint32(session->subscriptions().count()));
    }
    sessionsStorer->endReadKeys();
}

void Broker::storeSharedSubscriptions()
{
    size_t bc = 0;
    const quint8 * buf = Q_NULLPTR;
    union { quint8 b; SubscribeOptions o = {}; } options;

    QList<SubscriptionNode*> nodes = sharedSubscriptions.toTopicNodeList();
    for (SubscriptionNode * node: nodes)
    {
        QByteArray data;
        SharedSubscriptionData * d = dynamic_cast<SharedSubscriptionData*>(node->data());
        const SharedConsumersSessionsList & consumers = d->consumers();
        data.append(Encoder::encodeVariableByteInteger(consumers.count()));
        for (auto it = consumers.begin(); it != consumers.end(); ++it) {
            const QString & consumer = it.key();
            const SharedSubscriptionSessionsList & sessions = it.value();
            data.append(Encoder::encodeUTF8(consumer));
            data.append(Encoder::encodeVariableByteInteger(sessions.count()));
            for (const SharedSubscriptionSessionData & session_data: sessions) {
                SessionPtr session = session_data.session.toStrongRef();
                data.append(Encoder::encodeUTF8(session.isNull() ? QString() : session->clientId()));
                options.o = session_data.data.options;
                data.append(reinterpret_cast<const char*>(&options.b), sizeof(options.b));
                data.append(Encoder::encodeFourByteInteger(session_data.data.identifier));
            }
        }
        sharedSubscriptionsStorer->store(node->topic(), data.toHex());
    }
}

void Broker::loadSharedSubscriptions()
{
    const quint8 * buf = Q_NULLPTR;
    union { quint8 b; SubscribeOptions o = {}; } options;

    sharedSubscriptionsStorer->beginReadKeys();
    while (sharedSubscriptionsStorer->nextKeyAvailable())
    {
        QString key = sharedSubscriptionsStorer->nextKey();

        SubscriptionNode * node = sharedSubscriptions.provide(Topic(key));
        if (node->data() == Q_NULLPTR)
            node->data() = new SharedSubscriptionData();

        QByteArray data = QByteArray::fromHex(sharedSubscriptionsStorer->load(key));
        const quint8 * buf = reinterpret_cast<const quint8*>(data.constData());
        size_t bc = 0;
        qint64 rl = data.length();

        quint64 consumers_count = Decoder::decodeVariableByteInteger(buf, &rl, &bc);    buf += bc;
        while (consumers_count) {
            QString consumer_name  = Decoder::decodeUTF8(buf, &rl, &bc);                buf += bc;
            quint64 sessions_count = Decoder::decodeVariableByteInteger(buf, &rl, &bc); buf += bc;
            while (sessions_count) {
                QString session_client_id = Decoder::decodeUTF8(buf, &rl, &bc);         buf += bc;
                options.b = *buf;                                                     ++buf; --rl;
                quint32 identifier = Decoder::decodeFourByteInteger(buf, &rl, &bc);     buf += bc;
                auto it = sessions.find(session_client_id);
                if (it != sessions.end())
                    dynamic_cast<SharedSubscriptionData*>(node->data())->add(consumer_name, *it, options.o, identifier);
                --sessions_count;
            }
            --consumers_count;
        }
    }
    sharedSubscriptionsStorer->endReadKeys();
}

void Broker::addListener(Network::ServerPtr listener)
{
    listener->setNetworkEventsHandler(this);
    listeners.push_back(listener);
}

QString Broker::generateClientId(Version version)
{
    QString id = QUuid::createUuid().toString().remove('{').remove('}').remove('-');
    if (version == Version::Ver_3_1)
        id.remove(0, id.length() - 23);
    return id;
}

SessionSubscriptionData * Broker::selectSubscriptionDataWithMaximumQoS(const SubscriptionNode::List & nodes, std::vector<quint32> & outSubscriptionIdentifiers)
{
    QoS max_qos = QoS::Value_0;
    SessionSubscriptionData * result = Q_NULLPTR;
    for (const auto node: nodes) {
        SessionSubscriptionData * data = dynamic_cast<SessionSubscriptionData*>(node->data());
        if (max_qos != QoS::Value_2 && data->maxQoS() >= max_qos) {
            max_qos = data->maxQoS();
            result = data;
        }
        if (data->identifier() != 0)
            outSubscriptionIdentifiers.push_back(data->identifier());
    }
    return result;
}

SharedSubscriptionSessionData selectSharedSubscriptionSessionWithMaxQoS(SharedSubscriptionSessionsList & consumerSessions, bool amongConnected, bool * hasNotConnected)
{
    bool has_not_among_connected = false;
    Mqtt::QoS max_qos = Mqtt::QoS::Value_0;
    SessionPtr consumer_session;
    SubscribeOptions options;
    quint32 subscription_id = 0;

    auto it = consumerSessions.begin();
    while (it != consumerSessions.end()) {
        auto & pair = *it;
        SessionPtr session = pair.session;
        if (session.isNull()) {
            it = consumerSessions.erase(it);
            continue;
        }
        if (session->isConnected() == amongConnected) {
            Mqtt::QoS qos = pair.data.options.maximumQoS();
            if (qos >= max_qos) {
                max_qos = qos;
                consumer_session = session;
                options = pair.data.options;
                subscription_id = pair.data.identifier;
            }
            if (Mqtt::QoS::Value_2 == max_qos)
                break;
        } else {
            has_not_among_connected = true;
        }
        ++it;
    }

    if (hasNotConnected != Q_NULLPTR)
        *hasNotConnected = has_not_among_connected;

    return { consumer_session, { options, subscription_id } };
}

SharedSubscriptionSessionData selectSharedSubscriptionSession(SharedSubscriptionSessionsList & consumerSessions, bool amongConnected, bool * hasNotConnected)
{
    bool has_not_among_connected = false;
    SessionPtr consumer_session;
    SubscribeOptions options;
    quint32 subscription_id = 0;

    // cleanup
    auto it = consumerSessions.begin();
    while (it != consumerSessions.end()) {
        SessionPtr session = (*it).session;
        if (session.isNull()) {
            it = consumerSessions.erase(it);
            continue;
        }
        ++it;
    }

    for (decltype(consumerSessions.size()) i = 0; i < consumerSessions.size(); ++i)
    {
        consumerSessions.selectNext();
        auto & head = consumerSessions.head();
        SessionPtr s_ptr = head.session.toStrongRef();
        if (s_ptr->isConnected() == amongConnected) {
            consumer_session = head.session.toStrongRef();
            options = head.data.options;
            subscription_id = head.data.identifier;
            break;
        } else {
            has_not_among_connected = true;
        }
    }

    if (hasNotConnected != Q_NULLPTR)
        *hasNotConnected = has_not_among_connected;

    return { consumer_session, { options, subscription_id } };
}

SharedSubscriptionSessionData selectSharedSubscriptionSession(SharedSubscriptionSessionsList & consumerSessions)
{
    bool has_not_connected = false;
    // find connected session
    auto consumer_session = selectSharedSubscriptionSession(consumerSessions, true, &has_not_connected);
    if (consumer_session.session.isNull() && has_not_connected) {
        // if not found connected session and there is unconnected session,
        // find unconnected session for store message and sending message later, when it become possible.
        consumer_session = selectSharedSubscriptionSession(consumerSessions, false, Q_NULLPTR);
    }
    return consumer_session;
}


const Broker::SharedSubscriptionSessionsArray & Broker::selectSharedSubscriptionSessions(const SubscriptionNode::List & nodes)
{
    static thread_local SharedSubscriptionSessionsArray matched_sessions;
    matched_sessions.clear();

    SharedConsumersSessionsList matched_consumer_sessions;

    for (auto node: nodes)
    {
        SharedSubscriptionData * data = dynamic_cast<SharedSubscriptionData*>(node->data());
        auto consumer_it = data->consumers().begin();
        while (consumer_it != data->consumers().end())
        {
            SharedSubscriptionSessionsList & consumer_sessions = *consumer_it;
            if (consumer_sessions.size() == 0) {
                consumer_it = data->consumers().erase(consumer_it);
                continue;
            }
            auto consumer_session = selectSharedSubscriptionSession(consumer_sessions);
            if (!consumer_session.session.isNull()) {
                matched_consumer_sessions[consumer_it.key()].push_back({ consumer_session.session.toStrongRef(), consumer_session.data });
            }
            ++consumer_it;
        }
        if (data->isEmpty())
            sharedSubscriptions.remove(node);
    }

    for (auto & session_list: matched_consumer_sessions)
    {
        if (session_list.size() > 1) {
            matched_sessions.push_back(selectSharedSubscriptionSession(session_list));
        } else {
            matched_sessions.push_back(session_list[0]);
        }
    }

    return matched_sessions;
}

void Broker::removeSharedSubscriptions(Session * session)
{
    QList<SubscriptionNode*> nodes = sharedSubscriptions.toTopicNodeList();
    for (SubscriptionNode * node: nodes)
    {
        SharedSubscriptionData * data = dynamic_cast<SharedSubscriptionData*>(node->data());
        auto consumer_it = data->consumers().begin();
        while (consumer_it != data->consumers().end())
        {
            SharedSubscriptionSessionsList & consumer_sessions = *consumer_it;
            auto it = consumer_sessions.begin();
            while (it != consumer_sessions.end()) {
                SessionPtr session_ptr = (*it).session;
                if (session_ptr.isNull() || session_ptr.data() == session) {
                    it = consumer_sessions.erase(it);
                    continue;
                }
                ++it;
            }
            if (consumer_sessions.size() == 0) {
                consumer_it = data->consumers().erase(consumer_it);
                continue;
            }
            ++consumer_it;
        }
        if (data->isEmpty())
            sharedSubscriptions.remove(node);
    }
}

void Broker::publish(const QString & fromClientId, const Topic & topic, const PublishPacket & packet)
{
    static thread_local SubscriptionIdentifiersArray subscription_identifiers;

    if (packet.isRetained())
        retainPackets.add(packet.topicName(), fromClientId, packet);

    if (sharedSubscriptions.has(topic))
    {
        auto & array = selectSharedSubscriptionSessions(sharedSubscriptions.nodes());
        for (auto & pair: array) {
            SessionPtr s_ptr = pair.session.toStrongRef();
            if (!s_ptr.isNull()) {
                subscription_identifiers.clear();
                if (pair.data.identifier != 0)
                    subscription_identifiers.push_back(pair.data.identifier);
                processPublishPacket(s_ptr, packet, pair.data.options, subscription_identifiers);
            }
        }
    }

    for (auto s_ptr : sessions)
    {
        if (s_ptr.isNull())
            continue;
        Session * session = s_ptr.data();
        if (!session->subscriptions().has(topic))
            continue;
        subscription_identifiers.clear();
        // In this case the Server MUST deliver the message to the Client respecting the maximum QoS of all the matching subscriptions
        SessionSubscriptionData * data = selectSubscriptionDataWithMaximumQoS(session->subscriptions().nodes(), subscription_identifiers);
        if (data->options().noLocal() && fromClientId == session->clientId())
            continue;
        processPublishPacket(s_ptr, packet, data->options(), subscription_identifiers);
    }
}

void Broker::publishWill(const ConnectPacket & connectPacket, bool immediately)
{
    PublishPacket packet;
    packet.setTopicName(connectPacket.willTopic());
    packet.setPayload(connectPacket.willPayload().toUtf8());
    packet.setQoS(connectPacket.willQoS());
    packet.setRetain(connectPacket.willRetain());
    packet.properties() = connectPacket.willProperties();

    if (immediately) {
        const Topic topic(packet.topicName());
        if (topic.isValidForPublish())
            publish(connectPacket.clientId(), topic, packet);
    }
    else {
        QVariant delay = ControlPacket::property(connectPacket.willProperties(), PropertyId::WillDelayInterval);
        auto will_func = std::bind(&Broker::executePublishWill, this, connectPacket.clientId(), std::move(packet));
        QTimer::singleShot(delay.toInt() * 1000, will_func);
        log_note << "client" << connectPacket.clientId() << "will message has been prepared" << end_log;
    }
}

void Broker::publishRetainedPackets(SessionPtr & session, const SubscriptionNode::List & newSubscriptions)
{
    static thread_local SubscriptionIdentifiersArray subscription_identifiers;

    for (auto it = retainPackets.begin(); it != retainPackets.end(); )
    {
        PublishUnit & unit = it.value();

        if (!unit.isLoaded())
            retainPackets.loadUnit(it.key(), unit);

        if (unit.expired()) {
            it = retainPackets.erase(it);
            continue;
        }

        const Topic topic(it.key());

        for (auto node: newSubscriptions)
        {
            SessionSubscriptionData * data = dynamic_cast<SessionSubscriptionData*>(node->data());
            if ((SubscribeOptions::RetainHandling::NotSendAtSubscribe == data->options().retainHandlingType())
                 || (SubscribeOptions::RetainHandling::SendAtSubscribeForNewSubscription == data->options().retainHandlingType() && !data->isNew())
                    || (data->options().noLocal() && unit.clientId() == session->clientId()))
            {
                continue;
            }
            if (node->matchesWith(topic))
            {
                subscription_identifiers.clear();
                if (data->identifier() != 0)
                    subscription_identifiers.push_back(data->identifier());
                // Retained messages sent when the subscription is established have the RETAIN flag set to 1
                SubscribeOptionsSaver saver(data->options());
                data->options().setRetainAsPublished(true);
                unit.beforeSend();
                processPublishPacket(session, unit.packet(), data->options(), subscription_identifiers);
            }
        }

        unit.unload();

        ++it;
    }
}

void Broker::processPublishPacket(SessionPtr & session, const PublishPacket & sourcePacket, SubscribeOptions subscribeOptions, const SubscriptionIdentifiersArray & identifiers)
{
    ++subcount;

    PublishPacket packet = sourcePacket;

    if (identifiers.size() > 0) {
        packet.properties().detach();
        for (auto id: identifiers)
            packet.properties().append({ PropertyId::SubscriptionIdentifier, id });
    }

    if (!subscribeOptions.retainAsPublished())
        packet.setRetain(false);

    if (packet.QoS() > subscribeOptions.maximumQoS())
        packet.setQoS(subscribeOptions.maximumQoS());

    bool connected = session->isConnected();

    if ((connected || !session->isClean()) && packet.QoS() != QoS::Value_0)
        session->addPendingPacket(packet);

    if (!connected) {
        if (!session->isClean()) {
            if (QoS::Value_0 == packet.QoS() && isQoS0OfflineEnabled()
                    && !packet.topicName().startsWith(QStringLiteral(u"$SYS/")))
                session->addPendingPacket(packet);
        }
        return;
    }

    if (QoS::Value_0 == packet.QoS())
    {
        QByteArray data = packet.serialize(session->protocolVersion());

        bool can_send = (data.size() <= session->maxPacketSize());

        if (!can_send)
        {
            if (session->processClientAlias(packet)) {
                packet.setTopicName(QString());
                data = packet.serialize(session->protocolVersion());
                can_send = (data.size() <= session->maxPacketSize());
            }
            if (!can_send) {
                log_warning << session->connection() << "can't publish message to client" << session->clientId() << ", packet size exceeded client max packet size" << end_log;
                statistic->increaseDroppedPublishMessages();
                return;
            }
        }

        session->connection().write(data);
        statistic->increaseSentPublishMessages();
    }
    else
    {
        publishPendingPackets(session);
    }
}

void Broker::publishPendingPackets(SessionPtr & session)
{
    if (!session->isConnected())
        return;

    quint16 id = 0;

    while (session->hasQuota())
    {
        PublishUnit * unit = session->nextPacketToFligth();

        if (unit == Q_NULLPTR)
            return;

        if (unit->packet().isDuplicate())
        {
            id = unit->packet().packetId();
            if (!session->storePacketId(id))
                return;
        }
        else
        {
            id = session->generateId();
            if (id == 0) {
                log_warning << session->connection() << session->clientId() << ", can't generate packet id for message, will try later" << end_log;
                return;
            }
        }

        session->putPacketToFligth(id, unit);

        switch (unit->packet().QoS())
        {
            case QoS::Value_0:
            case QoS::Value_2:
                const_cast<PublishPacket &>(unit->packet()).setDuplicate(false);
                break;
            default: break;
        }

        QByteArray data = unit->packet().serialize(session->protocolVersion());

        bool can_send = (data.size() <= session->maxPacketSize());

        if (!can_send)
        {
            PublishPacket packet = unit->packet();
            if (session->processClientAlias(packet)) {
                packet.setTopicName(QString());
                data = packet.serialize(session->protocolVersion());
                can_send = (data.size() <= session->maxPacketSize());
            }
            if (!can_send) {
                log_warning << session->connection() << "can't publish message to client" << session->clientId() << ", packet size exceeded client max packet size" << end_log;
                session->packetDelivered(unit->packet().packetId(), ReasonCodeV5::PacketTooLarge, true);
                statistic->increaseDroppedPublishMessages();
                continue;
            }
        }

        session->connection().write(data);
        statistic->increaseSentPublishMessages();

        if (QoS::Value_0 == unit->packet().QoS())
            session->packetDelivered(unit->packet().packetId(), ReasonCodeV5::Success, true);
    }
}

void Broker::updateClientsStatistic()
{
    statistic->setConnectedClients(sessionsByConn.count());
    statistic->setDisconnectedClients(sessions.count() - sessionsByConn.count());
    statistic->setTotalClients(sessions.count());
}

void Broker::executePublishWill(const QString & clientId, const PublishPacket & packet)
{
    bool client_connected = false;

    auto it = sessions.find(clientId);
    if (it != sessions.end())
        client_connected = (*it)->isConnected();

    if (client_connected) {
        log_note << "client" << clientId << "will message has been discarded, client has been connected again!" << end_log;
        return;
    }

    Topic topic(packet.topicName());

    if (topic.isValidForPublish()) {
        publish(clientId, topic, packet);
        log_note << "client" << clientId << "will message has been published" << end_log;
    }
}

SessionPtr Broker::findSession(quintptr connectionId) const
{
    auto it = sessionsByConn.find(connectionId);
    if (it != sessionsByConn.end())
        return *it;
    return SessionPtr(Q_NULLPTR);
}

void Broker::handleIncomingConnection(Network::ServerClient connection)
{
    SessionPtr session = createSession();
    session->setConnection(connection);
    sessionsNew.append(session);
    sessionsByConn[connection.id()] = session.toWeakRef();
    updateClientsStatistic();
}

void Broker::handleCloseConnection(quintptr connectionId)
{
    SessionPtr new_session;
    SessionPtr session;

    auto comparator = [&connectionId] (const SessionPtr & session) -> bool { return (session->connection().id() == connectionId); };
    {
        auto it = std::find_if(sessionsNew.begin(), sessionsNew.end(), comparator);
        if (it != sessionsNew.end()) {
            new_session = *it;
            sessionsNew.erase(it);
        }
    }

    {
        auto it = sessionsByConn.find(connectionId);
        if (it != sessionsByConn.end())
        {
            session = *it;
            sessionsByConn.erase(it);
            if (session != new_session) {
                if (!session->isNormalDisconnected() && session->connectPacket().willEnabled())
                    publishWill(session->connectPacket(), false);
                sessionsStorer->store(session->clientId(), session->serialize());
            }
        }
        else
        {
            auto it = std::find_if(sessions.begin(), sessions.end(), comparator);
            if (it != sessions.end()) {
                session = *it;
                sessions.erase(it);
            }
        }
    }

    updateClientsStatistic();

    if (!new_session.isNull()) {
        log_note << new_session->connection() << "unknown client DISCONNECTED" << end_log;
        new_session->clearConnection();
    }
    else if (!session.isNull()) {
        log_note << session->connection() << "mqtt client" << session->clientId() << "DISCONNECTED" << end_log;
        session->clearConnection();
    }
}

void Broker::handleConnectionUpgraded(quintptr connectionId)
{
    sessionsByConn.remove(connectionId);
    auto comparator = [&connectionId] (const SessionPtr & session) -> bool { return (session->connection().id() == connectionId); };
    {
        auto it = std::find_if(sessionsNew.begin(), sessionsNew.end(), comparator);
        if (it != sessionsNew.end())
            sessionsNew.erase(it);
    }
}

void Broker::handleNetworkData(quintptr connectionId, const QByteArray & data)
{
    SessionPtr session = findSession(connectionId);

    if (session.isNull()) {
        statistic->increaseDroppedMessages();
        return;
    }

    session->dataController().append(data);
    while (session->dataController().packetAvailable()) {
        session->restartElapsed();
        handleControlPacket(session, session->dataController().takePacket());
    }
}

void Broker::handleControlPacket(SessionPtr & session, const QByteArray & data)
{
    if (session->isBanned())
        return;

    Mqtt::PacketType type = Mqtt::ControlPacket::extractType(data);

    if (session->expectsConnectPacket() && Mqtt::PacketType::CONNECT != type) {
        log_warning << session->connection() << "received packet's is not expected CONNECT, type was" << type << printByteArray(data) << end_log;
        session->connection().close();
        if (Mqtt::PacketType::PUBLISH == type)
            statistic->increaseDroppedPublishMessages();
        else
            statistic->increaseDroppedMessages();
        return;
    }

    statistic->increaseMessages();

    switch (type)
    {
        case Mqtt::PacketType::CONNECT:        { handleConnectPacket(session, data);     return; }
        case Mqtt::PacketType::CONNACK:        { /* cause server to client packet */     break;  }
        case Mqtt::PacketType::PUBLISH:
        {
            subcount = 0;
            handlePublishPacket(session, data);
            if (subcount == 0)
                statistic->increaseDroppedPublishMessages();
            return;
        }
        case Mqtt::PacketType::PUBACK:         { handlePubAckPacket(session, data);      return; }
        case Mqtt::PacketType::PUBREC:         { handlePubRecPacket(session, data);      return; }
        case Mqtt::PacketType::PUBREL:         { handlePubRelPacket(session, data);      return; }
        case Mqtt::PacketType::PUBCOMP:        { handlePubCompPacket(session, data);     return; }
        case Mqtt::PacketType::SUBSCRIBE:      { handleSubscribePacket(session, data);   return; }
        case Mqtt::PacketType::UNSUBSCRIBE:    { handleUnsubscribePacket(session, data); return; }
        case Mqtt::PacketType::PINGREQ:        { handlePingRequestPacket(session, data); return; }
        case Mqtt::PacketType::DISCONNECT:     { handleDisconnectPacket(session, data);  return; }
        case Mqtt::PacketType::AUTH:           { handleAuthPacket(session, data);        return; }
        default: break;
    }

    log_warning << session->connection() << "received packet's type is not valid:" << type << printByteArray(data) << end_log;
    session->connection().close();
    statistic->increaseDroppedMessages();
}

void Broker::handleConnectPacket(SessionPtr & session, const QByteArray & data)
{
    auto comparator = [&session] (const SessionPtr & other) -> bool { return (other->connection() == session->connection()); };
    auto sn_it = std::find_if(sessionsNew.begin(), sessionsNew.end(), comparator);
    if (sn_it == sessionsNew.end()) {
        session->connection().close();
        statistic->increaseDroppedMessages();
        return;
    }

    Mqtt::ConnectPacketPtr packet = Mqtt::ConnectPacketPtr(new ConnectPacket());

    ConnectPacket * conn_packet = packet.data();
    conn_packet->unserialize(data);

    Version proto_version = conn_packet->protocolVersion();

    if (packet->unserializeReasonCode() != ReasonCodeV5::Success)
    {
        log_warning << session->connection() << "packet" << packet->type() << "not unserialized:" << packet->unserializeReasonString() << printByteArray(data) << end_log;
        if (Version::Ver_5_0 == proto_version)
            sendConnackError(session, quint8(packet->unserializeReasonCode()), packet->unserializeReasonString());
        session->connection().close();
        statistic->increaseDroppedMessages();
        return;
    }

    log_trace << session->connection() << "packet" << packet->type() << "unserialized succesfully" << end_log;

    if (conn_packet->clientId().isEmpty())
    {
        if (!conn_packet->cleanSession()) {
            sendConnackError(session, Version::Ver_5_0 == proto_version ? quint8(ReasonCodeV5::ClientIdentifierNotValid) : quint8(ReturnCodeV3::IdentifierRejected));
            session->connection().close();
            statistic->increaseDroppedMessages();
            return;
        }
        conn_packet->setClientId(Broker::generateClientId(proto_version));
        session->setClientIdProvidedByServer(true);
    }

    bool session_is_present = false;
    auto s_it = sessions.find(conn_packet->clientId());

    if (s_it == sessions.end()) {
        QByteArray data = sessionsStorer->load(conn_packet->clientId());
        if (!data.isEmpty()) {
            SessionPtr s = createSession();
            s->unserialize(data);
            s_it = sessions.insert(conn_packet->clientId(), s);
            statistic->increaseSubscriptionCount(qint32(s->subscriptions().count()));
        }
    }

    if (s_it != sessions.end())
    {
        SessionPtr previous_session = *s_it;

        if (previous_session->isBanned())
        {
            sessionsNew.erase(sn_it);
            sendConnackError(session, (Version::Ver_5_0 == session->protocolVersion())
                             ? static_cast<quint8>(ReasonCodeV5::Banned)
                             : static_cast<quint8>(ReturnCodeV3::ServerUnavailable)
                             , QStringLiteral("you are banned!"));
            session->connection().close();
            log_note << session->connection() << packet->clientId() << "BANNED for" << previous_session->banTimeout() << "seconds" << end_log;
            return;
        }

        if (previous_session->isConnected())
        {
            session->setConnectPacket(packet);
            if (previous_session->connection() == session->connection()) {
                processTwiceConnection(session);
                statistic->increaseDroppedMessages();
                return;
            } else {
                processOtherConnectionWithSameClientId(session, previous_session);
                sessionsByConn.remove(previous_session->connection().id());
                sessions.erase(s_it);
            }
        }
        else
        {
            if (!conn_packet->cleanSession()) {
                previous_session->setConnection(session->connection());
                sessionsByConn.remove(session->connection().id());
                session = previous_session;
                session_is_present = true;
            } else {
                session->syncBanDuration(*(previous_session.data()));
            }
        }
    }

    session->setConnectPacket(packet);
    session->setNormalDisconnectWas(false);
    session->setPresent(session_is_present);

    sessions.insert(session->clientId(), session);
    sessionsByConn.insert(session->connection().id(), session);
    sessionsNew.erase(sn_it);

    // NOTE: perform authentication and authorization checks (If any of these checks fail, it MUST close the Network Connection)
    // Before closing the Network Connection, it MAY send an appropriate CONNACK response with a Reason Code of 0x80 or greater.

    if (ControlPacket::property(packet->properties(), PropertyId::AuthentificationMethod).isValid())
    {
        // TODO: add support later
        log_warning << session->connection() << "auth method not supported currently" << end_log;
        sendConnackError(session, static_cast<quint8>(ReasonCodeV5::BadAuthenticationMethod));
        session->connection().close();
    }
    else if (passFile == Q_NULLPTR || passFile->isUserAccepted(packet->username(), packet->password()))
    {
        loadSharedSubscriptions();
        if (!session->hasPendingPacketsStorer())
            session->setPendingPacketsStorer(storerFactory->createStorer(QStringLiteral("pending/") + session->clientId()));
        finalizeSuccessfulConnect(session);
    }
    else
    {
        sendConnackError(session, (Version::Ver_5_0 == session->protocolVersion())
                         ? static_cast<quint8>(ReasonCodeV5::BadUserNameOrPassword)
                         : static_cast<quint8>(ReturnCodeV3::BadUserNameOrPassword)
                         , QStringLiteral("username and password are not defined"));
        session->connection().close();
    }
}

void Broker::sendConnackSuccess(SessionPtr & session)
{
    ConnackPacket connack;
    connack.setSessionPresent(session->isPresent());
    connack.setReasonCodeV5(ReasonCodeV5::Success);
    if (Version::Ver_5_0 == session->protocolVersion()) {
        connack.properties().append(Property(PropertyId::RetainAvailable, 1));
        if (session->isClientIdProvideByServer())
            connack.properties().append(Property(PropertyId::AssignedClientIdentifier, session->clientId()));
        connack.properties().append(Property(PropertyId::WildcardSubscriptionAvailable, 1));
        connack.properties().append(Property(PropertyId::SubscriptionIdentifierAvailable, 1));
        connack.properties().append(Property(PropertyId::SharedSubscriptionAvailable, 1));
        if (session->isResponseInformationRequested())
            connack.properties().append(Property(PropertyId::ResponseInformation, session->responseInformation()));
        connack.properties().append(Property(PropertyId::TopicAliasMaximum, Constants::TopicAliasMaximum));
    }
    session->connection().write(connack.serialize(session->protocolVersion(), session->maxPacketSize()));
    statistic->increaseSentMessages();
}

void Broker::finalizeSuccessfulConnect(SessionPtr & session)
{
    sendConnackSuccess(session);
    updateClientsStatistic();
    publishPendingPackets(session);
    sessionsStorer->store(session->clientId(), session->serialize());

    log_note << session->connection() << "mqtt client" << session->clientId() << "CONNECTED" << ", session is present:" << (session->isPresent() ? "yes" : "no") << end_log;
}

void Broker::sendConnackError(SessionPtr & session, quint8 reasonCode, const QString & reasonString)
{
    ConnackPacket connack;
    // If a Server sends a CONNACK packet containing a non-zero Reason Code it MUST set Session Present to 0
    connack.setSessionPresent(false);

    if (Mqtt::Version::Ver_5_0 == session->protocolVersion())
    {
        connack.setReasonCodeV5(ReasonCodeV5(reasonCode));
        connack.properties().append(Property(PropertyId::ReasonString, !reasonString.isEmpty() ? reasonString : (QString(QStringLiteral("error code %1")).arg(reasonCode))));
        session->connection().write(connack.serialize(session->protocolVersion(), session->maxPacketSize()));
        statistic->increaseSentMessages();
    }
    else // V3
    {
        if (reasonCode > 0 && reasonCode < 6)
        {
            connack.setReturnCodeV3(ReturnCodeV3(reasonCode));
            session->connection().write(connack.serialize(session->protocolVersion(), session->maxPacketSize()));
            statistic->increaseSentMessages();
        }
    }
}

void Broker::processTwiceConnection(SessionPtr & session)
{
    log_warning << session->connection() << "receive second CONNECT packet -> disconnect client, client id:" << session->clientId() << end_log;

    if (Mqtt::Version::Ver_5_0 == session->protocolVersion())
    {
        // Client can only send the CONNECT packet once over a Network Connection. The Server MUST
        // process a second CONNECT packet sent from a Client as a Protocol Error and close the Network Connection
        sendConnackError(session, quint8(ReasonCodeV5::ProtocolError), QStringLiteral("CONNECT packet was received again (TWICE)"));
        session->connection().close();
    }
    else // V3
    {
        // A Client can only send the CONNECT Packet once over a Network Connection. The Server MUST
        // process a second CONNECT Packet sent from a Client as a protocol violation and disconnect the Client
        // If none of the return codes listed in Table 3.1 – Connect Return code values are deemed applicable, then
        // the Server MUST close the Network Connection without sending a CONNACK
        session->connection().close();
    }
}

void Broker::processOtherConnectionWithSameClientId(SessionPtr & session, SessionPtr & otherSession)
{
    log_warning << session->connection() << "receive CONNECT packet, that was used by other connection -> disconnect other client:" << otherSession->connection() << ", client id:" << session->clientId() << end_log;

    if (Mqtt::Version::Ver_5_0 == otherSession->protocolVersion()) {
        // If the ClientID represents a Client already connected to the Server, the Server sends a
        // DISCONNECT packet to the existing Client with Reason Code of 0x8E (Session taken over) as
        // described in section 4.13 and MUST close the Network Connection of the existing Client
        // If the existing Client has a Will Message, that Will Message is published as described in section
        sendConnackError(otherSession, quint8(ReasonCodeV5::SessionTakenOver),
                         QString(QStringLiteral("client from %1:%2 take over session"))
                         .arg(otherSession->connection().ip().toString())
                         .arg(otherSession->connection().port()));
        otherSession->connection().close();
    }
    else // V3
    {
        // A Client can only send the CONNECT Packet once over a Network Connection. The Server MUST
        // process a second CONNECT Packet sent from a Client as a protocol violation and disconnect the Client
        // If none of the return codes listed in Table 3.1 – Connect Return code values are deemed applicable, then
        // the Server MUST close the Network Connection without sending a CONNACK
        sendConnackError(otherSession, quint8(ReturnCodeV3::IdentifierRejected));
        otherSession->connection().close();
    }
}

void Broker::handlePingRequestPacket(SessionPtr & session, const QByteArray & data)
{
    PingRequestPacket packet;
    if (packet.unserialize(data, session->protocolVersion())) {
        log_trace << session->connection() << "packet" << packet.type() << "unserialized succesfully" << end_log;
        session->connection().write(PingResponsePacket().serialize(session->protocolVersion()));
        statistic->increaseSentMessages();
    } else {
        session->connection().close();
        statistic->increaseDroppedMessages();
    }
}

void Broker::handlePublishPacket(SessionPtr & session, const QByteArray & data)
{
    statistic->increasePublishMessages();

    PublishPacket packet;

    if (packet.unserialize(data, session->protocolVersion()))
    {
        log_trace << session->connection() << "packet" << packet.type() << "unserialized succesfully" << end_log;

        const Topic topic(packet.topicName());

        if (topic.isValidForPublish())
        {
            if (!session->processBrokerAlias(packet)) {
                log_warning << session->connection() << "packet" << packet.type() << "unserialized, but wrong broker alias" << end_log;
                sendDisconnect(session, ReasonCodeV5::ProtocolError);
                session->connection().close();
                return;
            }

            session->increaseFlowRate(packet.QoS());

            if (session->flowRate(packet.QoS()) > maxFlowPerSecond(packet.QoS())) {
                session->ban(banDuration, banAccumulative);
                sendDisconnect(session, ReasonCodeV5::MessageRateTooHigh);
                log_important << session->connection() << session->clientId()
                              << "message flow (" << session->flowRate(packet.QoS())
                              << ") limit has been reached (" << maxFlowPerSecond(packet.QoS())
                              << ") -> BANNED for" << session->banTimeout() << "seconds" << end_log;
                session->connection().close();
                return;
            }

            switch (packet.QoS())
            {
                case QoS::Value_0:
                {
                    publish(session->clientId(), topic, packet);
                    return;
                }

                case QoS::Value_1:
                {
                    publish(session->clientId(), topic, packet);
                    /* The receiver does not need to complete delivery of the Application Message before sending the
                       PUBACK. When its original sender receives the PUBACK packet, ownership of the Application
                       Message is transferred to the receiver */
                    PublishAckPacket puback;
                    puback.setPacketId(packet.packetId());
                    puback.setReasonCode(subcount == 0 ? ReasonCodeV5::NoMatchingSubscribers : ReasonCodeV5::Success);
                    QByteArray data = puback.serialize(session->protocolVersion(), session->maxPacketSize());
                    session->connection().write(data);
                    statistic->increaseSentMessages();
                    return;
                }

                case QoS::Value_2:
                {
                    PublishRecPacket pubrec;
                    pubrec.setPacketId(packet.packetId());
                    if (session->storePacketId(packet.packetId())) {
                        publish(session->clientId(), topic, packet);
                        pubrec.setReasonCode(subcount == 0 ? ReasonCodeV5::NoMatchingSubscribers : ReasonCodeV5::Success);
                    } else {
                        pubrec.setReasonCode(ReasonCodeV5::PacketIdentifierInUse);
                        if (session->protocolVersion() < Version::Ver_5_0) { break; /* close connection */ }
                    }
                    QByteArray data = pubrec.serialize(session->protocolVersion(), session->maxPacketSize());
                    session->connection().write(data);
                    statistic->increaseSentMessages();
                    return;
                }

                default: break;
            }
        }
    }

    if (ReasonCodeV5::TopicAliasInvalid == packet.unserializeReasonCode())
        sendDisconnect(session, ReasonCodeV5::TopicAliasInvalid);

    session->connection().close();
}

void Broker::handlePubAckPacket(SessionPtr & session, const QByteArray & data)
{
    const Network::ServerClient & conn = session->connection();
    PublishAckPacket packet;
    if (packet.unserialize(data, session->protocolVersion())) {
        log_trace << conn << "packet" << packet.type() << "unserialized succesfully" << end_log;
        session->packetDelivered(packet.packetId(), packet.reasonCode(), true);
        publishPendingPackets(session);
        return;
    }
    conn.close();
    statistic->increaseDroppedMessages();
}

void Broker::handlePubRecPacket(SessionPtr & session, const QByteArray & data)
{
    PublishRecPacket packet;
    if (packet.unserialize(data, session->protocolVersion())) {
        log_trace << session->connection() << "packet" << packet.type() << "unserialized succesfully" << end_log;
        session->packetDelivered(packet.packetId(), packet.reasonCode(), false);
        if (packet.reasonCode() >= ReasonCodeV5::UnspecifiedError) {
            publishPendingPackets(session);
            return;
        } else {
            PublishRelPacket pubrel;
            pubrel.setPacketId(packet.packetId());
            pubrel.setReasonCode(ReasonCodeV5::Success);
            QByteArray data = pubrel.serialize(session->protocolVersion(), session->maxPacketSize());
            session->connection().write(data);
            statistic->increaseSentMessages();
            return;
        }
    }
    session->connection().close();
    statistic->increaseDroppedMessages();
}

void Broker::handlePubRelPacket(SessionPtr & session, const QByteArray & data)
{
    PublishRelPacket packet;
    if (packet.unserialize(data, session->protocolVersion())) {
        log_trace << session->connection() << "packet" << packet.type() << "unserialized succesfully" << end_log;
        session->freePacketId(packet.packetId());
        PublishCompPacket pubcomp;
        pubcomp.setPacketId(packet.packetId());
        pubcomp.setReasonCode(ReasonCodeV5::Success);
        QByteArray data = pubcomp.serialize(session->protocolVersion(), session->maxPacketSize());
        session->connection().write(data);
        statistic->increaseSentMessages();
        return;
    }
    session->connection().close();
    statistic->increaseDroppedMessages();
}

void Broker::handlePubCompPacket(SessionPtr & session, const QByteArray & data)
{
    PublishCompPacket packet;
    if (packet.unserialize(data, session->protocolVersion())) {
        log_trace << session->connection() << "packet" << packet.type() << "unserialized succesfully" << end_log;
        session->freePacketId(packet.packetId());
        publishPendingPackets(session);
        return;
    }
    session->connection().close();
    statistic->increaseDroppedMessages();
}

void Broker::handleDisconnectPacket(SessionPtr & session, const QByteArray & data)
{
    const Network::ServerClient & conn = session->connection();
    DisconnectPacket packet;
    if (packet.unserialize(data, session->protocolVersion())) {
        log_trace << conn << "packet" << packet.type() << "unserialized succesfully" << end_log;
        QVariant v = ControlPacket::property(packet.properties(), PropertyId::SessionExpiryInterval);
        if (v.isValid()) {
            quint32 expiry_by_connect = v.toUInt();
            QVariant v2 = ControlPacket::property(session->connectPacket().properties(), PropertyId::SessionExpiryInterval);
            quint32 expiry_by_disconnect = v2.toUInt();
            if (expiry_by_connect == 0 && expiry_by_disconnect != 0) {
                sendDisconnect(session, ReasonCodeV5::ProtocolError);
            } else {
                session->setExpiryInterval(expiry_by_disconnect);
            }
        }
        session->setNormalDisconnectWas(packet.reasonCode() < ReasonCodeV5::UnspecifiedError);
        if (ReasonCodeV5::DisconnectWithWillMessage == packet.reasonCode()) {
            if (session->connectPacket().willEnabled())
                publishWill(session->connectPacket(), true);
        }
    } else {
        statistic->increaseDroppedMessages();
    }
    conn.close();
    handleCloseConnection(conn.id());
}

void Broker::handleSubscribePacket(SessionPtr & session, const QByteArray & data)
{
    SubscribePacket packet;
    if (!packet.unserialize(data, session->protocolVersion())) {
        sendDisconnect(session, packet.unserializeReasonCode());
        session->connection().close();
        statistic->increaseDroppedMessages();
        return;
    }

    quint32 sub_id = 0;
    QVariant v = ControlPacket::property(packet.properties(), PropertyId::SubscriptionIdentifier);
    if (v.isValid()) {
        sub_id = v.toUInt();
        if (sub_id < Constants::MinSubscriptionIdentifier || sub_id > Constants::MaxSubscriptionIdentifier) {
            sendDisconnect(session, ReasonCodeV5::MalformedPacket);
            session->connection().close();
            statistic->increaseDroppedMessages();
            return;
        }
    }

    log_trace << session->connection() << "packet" << packet.type() << "unserialized succesfully" << end_log;

    SubscribeAckPacket suback;
    suback.setPacketId(packet.packetId());

    static thread_local SubscriptionNode::List new_nodes;
    new_nodes.clear();
    new_nodes.reserve(packet.topics().count());

    for (int i = 0; i < packet.topics().count(); ++i)
    {
        const SubscriptionPair & sub = packet.topics().at(i);
        const Topic topic(sub.first);
        const SubscribeOptions & options = sub.second;

        if (topic.isValidForSubscribe())
        {
            switch (topic.destination())
            {
                case Topic::All:
                case Topic::System: {
                    auto node = session->subscriptions().provide(topic);
                    SessionSubscriptionData * data = dynamic_cast<SessionSubscriptionData*>(node->data());
                    if (data == Q_NULLPTR) {
                        node->data() = data = new SessionSubscriptionData();
                        statistic->increaseSubscriptionCount();
                    } else {
                        data->setNew(false);
                    }
                    data->setOptions(options);
                    data->setIdentifier(sub_id);
                    new_nodes.push_back(node);
                    break;
                }
                case Topic::Shared: {
                    // No Retained Messages are sent to the Session when it first subscribes.
                    // It will be sent other matching messages as they are published.
                    auto node = sharedSubscriptions.provide(topic);
                    if (node->data() == Q_NULLPTR)
                        node->data() = new SharedSubscriptionData();
                    dynamic_cast<SharedSubscriptionData*>(node->data())->add(topic.consumer(), session, options, sub_id);
                    storeSharedSubscriptions();
                    break;
                }
            }
            switch (options.maximumQoS()) {
                case QoS::Value_0:  suback.returnCodes().append(ReasonCodeV5::GrantedQoS0);      break;
                case QoS::Value_1:  suback.returnCodes().append(ReasonCodeV5::GrantedQoS1);      break;
                case QoS::Value_2:  suback.returnCodes().append(ReasonCodeV5::GrantedQoS2);      break;
                default:            suback.returnCodes().append(ReasonCodeV5::UnspecifiedError); break;
            }
        }
        else
        {
            suback.returnCodes().append(ReasonCodeV5::TopicFilterInvalid);
        }
    }

    session->connection().write(suback.serialize(session->protocolVersion(), session->maxPacketSize()));
    statistic->increaseSentMessages();

    if (new_nodes.size() != 0) {
        publishRetainedPackets(session, new_nodes);
        publishSystemPackets(session, new_nodes);
    }

    sessionsStorer->store(session->clientId(), session->serialize());
}

void Broker::handleUnsubscribePacket(SessionPtr & session, const QByteArray & data)
{
    UnsubscribePacket packet;
    if (packet.unserialize(data, session->protocolVersion()))
    {
        log_trace << session->connection() << "packet" << packet.type() << "unserialized succesfully" << end_log;

        UnsubscribeAckPacket unsuback;
        unsuback.setPacketId(packet.packetId());
        for (int i = 0; i < packet.topics().count(); ++i)
        {
            Topic topic(packet.topics().at(i));
            if (topic.isValidForSubscribe())
            {
                switch (topic.destination())
                {
                    case Topic::All:
                    case Topic::System: {
                        auto node = session->subscriptions().findNode(topic);
                        if (node != Q_NULLPTR) {
                            session->subscriptions().remove(node);
                            unsuback.returnCodes().append(ReasonCodeV5::Success);
                            statistic->decreaseSubscriptionCount();
                        } else {
                            unsuback.returnCodes().append(ReasonCodeV5::NoSubscriptionExisted);
                        }
                        break;
                    }
                    case Topic::Shared: {
                        SubscriptionNode * node = sharedSubscriptions.findNode(topic);
                        if (node != Q_NULLPTR) {
                            SharedSubscriptionData * data = dynamic_cast<SharedSubscriptionData*>(node->data());
                            int count = data->remove(topic.consumer(), session);
                            unsuback.returnCodes().append(count > 0 ? ReasonCodeV5::Success : ReasonCodeV5::NoSubscriptionExisted);
                            if (data->isEmpty())
                                sharedSubscriptions.remove(node);
                        }
                        break;
                    }
                }
            }
            else
            {
                unsuback.returnCodes().append(ReasonCodeV5::TopicFilterInvalid);
            }
        }
        session->connection().write(unsuback.serialize(session->protocolVersion(), session->maxPacketSize()));
        statistic->increaseSentMessages();

        sessionsStorer->store(session->clientId(), session->serialize());
    }
    else {
        session->connection().close();
        statistic->increaseDroppedMessages();
    }
}

void Broker::handleAuthPacket(SessionPtr & session, const QByteArray & data)
{
    // TODO: later
    log_warning << session->connection() << PacketType::AUTH << "not supported currently" << end_log;
    sendDisconnect(session, ReasonCodeV5::BadAuthenticationMethod);
    session->connection().close();
}

void Broker::sendDisconnect(SessionPtr & session, ReasonCodeV5 reason)
{
    DisconnectPacket d;
    d.setReasonCode(reason);
    session->connection().write(d.serialize(session->protocolVersion(), session->maxPacketSize()));
    statistic->increaseSentMessages();
}

#define TopicSysBroker                QStringLiteral(u"$SYS/broker")
#define TopicSysMqttClients           QStringLiteral(u"$SYS/broker/mqtt/clients")
#define TopicSysMqttSubscriptions     QStringLiteral(u"$SYS/broker/mqtt/subscriptions")
#define TopicSysMqttMessagesAllRecv   QStringLiteral(u"$SYS/broker/mqtt/all/received")
#define TopicSysMqttMessagesAllSent   QStringLiteral(u"$SYS/broker/mqtt/all/sent")
#define TopicSysMqttMessagesAllDrop   QStringLiteral(u"$SYS/broker/mqtt/all/dropped")
#define TopicSysMqttMessagesPubRecv   QStringLiteral(u"$SYS/broker/mqtt/publish/received")
#define TopicSysMqttMessagesPubSent   QStringLiteral(u"$SYS/broker/mqtt/publish/sent")
#define TopicSysMqttMessagesPubDrop   QStringLiteral(u"$SYS/broker/mqtt/publish/dropped")
#define TopicSysNetworkLoad           QStringLiteral(u"$SYS/broker/network/load")

#define BytesStatisticName            QByteArrayLiteral("bytes")
#define MessagesStatisticName         QByteArrayLiteral("messages")

void Broker::publishBrokerInfo()          { publishSystemPacket(TopicSysBroker             , makeBrokerInfoPayload());        }
void Broker::publishMqttClientsInfo()     { publishSystemPacket(TopicSysMqttClients        , makeMqttClientsInfoPayload());   }
void Broker::publishSubscriptionsInfo()   { publishSystemPacket(TopicSysMqttSubscriptions  , makeSubscriptionsInfoPayload()); }

void Broker::publishMqttMessRecvInfo()    { publishSystemPacket(TopicSysMqttMessagesAllRecv, statistic->allmessages.received.load().toJSON(MessagesStatisticName)); }
void Broker::publishMqttMessSentInfo()    { publishSystemPacket(TopicSysMqttMessagesAllSent, statistic->allmessages.sent.load()    .toJSON(MessagesStatisticName)); }
void Broker::publishMqttMessDropInfo()    { publishSystemPacket(TopicSysMqttMessagesAllDrop, statistic->allmessages.dropped.load() .toJSON(MessagesStatisticName)); }
void Broker::publishMqttPublishRecvInfo() { publishSystemPacket(TopicSysMqttMessagesPubRecv, statistic->publish.received.load()    .toJSON(MessagesStatisticName)); }
void Broker::publishMqttPublishSentInfo() { publishSystemPacket(TopicSysMqttMessagesPubSent, statistic->publish.sent.load()        .toJSON(MessagesStatisticName)); }
void Broker::publishMqttPublishDropInfo() { publishSystemPacket(TopicSysMqttMessagesPubDrop, statistic->publish.dropped.load()     .toJSON(MessagesStatisticName)); }

void Broker::publishNetworkLoadInfo()     { publishSystemPacket(TopicSysNetworkLoad        , makeNetworkLoadInfoPayload());   }

void Broker::publishSystemPackets(SessionPtr & session, const SubscriptionNode::List & newSubscriptions)
{
    static thread_local SubscriptionIdentifiersArray subscription_identifiers;

    auto publishSystemInfo = [&](const QString & t, std::function<QByteArray(void)> payloadMaker) {
        const Topic topic(t);
        for (auto node: newSubscriptions)
        {
            if (node->matchesWith(topic)) {
                SessionSubscriptionData * data = dynamic_cast<SessionSubscriptionData*>(node->data());
                subscription_identifiers.clear();
                if (data->identifier() != 0)
                    subscription_identifiers.push_back(data->identifier());
                processPublishPacket(session, this->makeSystemInfoPacket(t, payloadMaker()), data->options(), subscription_identifiers);
            }
        }
    };

    publishSystemInfo(TopicSysBroker             , std::bind(&Broker::makeBrokerInfoPayload         , this));
    publishSystemInfo(TopicSysMqttClients        , std::bind(&Broker::makeMqttClientsInfoPayload    , this));
    publishSystemInfo(TopicSysMqttSubscriptions  , std::bind(&Broker::makeSubscriptionsInfoPayload  , this));

    publishSystemInfo(TopicSysMqttMessagesAllRecv, std::bind(&Average::Load::toJSON, &statistic->allmessages.received.load(), MessagesStatisticName));
    publishSystemInfo(TopicSysMqttMessagesAllSent, std::bind(&Average::Load::toJSON, &statistic->allmessages.sent.load()    , MessagesStatisticName));
    publishSystemInfo(TopicSysMqttMessagesAllDrop, std::bind(&Average::Load::toJSON, &statistic->allmessages.dropped.load() , MessagesStatisticName));
    publishSystemInfo(TopicSysMqttMessagesPubRecv, std::bind(&Average::Load::toJSON, &statistic->publish.received.load()    , MessagesStatisticName));
    publishSystemInfo(TopicSysMqttMessagesPubSent, std::bind(&Average::Load::toJSON, &statistic->publish.sent.load()        , MessagesStatisticName));
    publishSystemInfo(TopicSysMqttMessagesPubDrop, std::bind(&Average::Load::toJSON, &statistic->publish.dropped.load()     , MessagesStatisticName));

    publishSystemInfo(TopicSysNetworkLoad        , std::bind(&Broker::makeNetworkLoadInfoPayload    , this));
}

#undef TopicSysBroker
#undef TopicSysMqttClients
#undef TopicSysMqttSubscriptions
#undef TopicSysMqttMessagesAllRecv
#undef TopicSysMqttMessagesAllSent
#undef TopicSysMqttMessagesPubRecv
#undef TopicSysMqttMessagesPubSent
#undef TopicSysNetworkLoad

PublishPacket Broker::makeSystemInfoPacket(const QString & topic, const QByteArray & payload)
{
    PublishPacket packet;
    packet.setTopicName(topic);
    packet.setPayload(payload);
    packet.setQoS(QoS::Value_0);
    packet.setRetain(false);
    ControlPacket::setProperty(packet.properties(), PropertyId::ContentType, QStringLiteral("JSON"));
    return packet;
}

void Broker::publishSystemPacket(const QString & t, const QByteArray & payload)
{
    PublishPacket packet = makeSystemInfoPacket(t, payload);
    Topic topic(packet.topicName());
    publish(QString(), topic, packet);
}

QByteArray formatTimeUnit(quint64 value, char name, bool justified = true, bool appendSpace = true)
{
    QByteArray ba; ba.reserve(5);
    if (justified && value < 10)
        ba.append('0');
    ba.append(QByteArray::number(value)).append(name);
    return (appendSpace ? ba.append(' ') : ba);
}

QByteArray fromatUptime(quint64 seconds)
{
    QByteArray ba; ba.reserve(20);

    quint64 days    = seconds / 86400;  seconds -= days * 86400;

    if (days > 0)
        ba.append(formatTimeUnit(days, 'd', false));

    quint64 hours   = seconds / 3600;   seconds -= hours * 3600;

    if (hours > 0)
        ba.append(formatTimeUnit(hours, 'h'));

    quint64 minutes = seconds / 60;     seconds -= minutes * 60;

    if (minutes > 0)
        ba.append(formatTimeUnit(minutes, 'm'));

    return ba.append(formatTimeUnit(seconds, 's', true, false));
}

QByteArray Broker::makeBrokerInfoPayload() const
{
    QByteArray payload;
    payload.reserve(200);
    payload.append('{');
    payload.append("\"version\":\"");
    payload.append(statistic->broker.version); payload.append("\"");
    payload.append(",\"description\":\"");
    payload.append(statistic->broker.description); payload.append("\"");
    payload.append(",\"timestamp\":\"");
    payload.append(statistic->broker.timestamp); payload.append("\"");
    payload.append(",\"gitrevision\":\"");
    payload.append(statistic->broker.gitrevision); payload.append("\"");
    payload.append(",\"uptime\":\"");
    payload.append(fromatUptime(statistic->broker.uptime));
    payload.append("\"}");
    return payload;
}

QByteArray Broker::makeMqttClientsInfoPayload() const
{
    QByteArray payload;
    payload.reserve(140);
    payload.append('{');
    payload.append("\"total\":");
    payload.append(QByteArray::number(statistic->clients.total));
    payload.append(",\"maximum\":");
    payload.append(QByteArray::number(statistic->clients.maximum));
    payload.append(",\"connected\":");
    payload.append(QByteArray::number(statistic->clients.connected));
    payload.append(",\"disconnected\":");
    payload.append(QByteArray::number(statistic->clients.disconnected));
    payload.append(",\"expired\":");
    payload.append(QByteArray::number(statistic->clients.expired));
    payload.append('}');
    return payload;
}

QByteArray Broker::makeSubscriptionsInfoPayload() const
{
    QByteArray payload;
    payload.reserve(60);
    payload.append('{');
    payload.append("\"count\":");
    payload.append(QByteArray::number(statistic->subscriptionsCount));
    payload.append(",\"shared\":");
    payload.append(QByteArray::number(sharedSubscriptions.count()));
    payload.append('}');
    return payload;
}

QByteArray Broker::makeNetworkLoadInfoPayload() const
{
    QByteArray payload;
    payload.reserve(250 * listeners.count());

    payload.append('{');
    auto it = listeners.begin();
    while (it != listeners.end())
    {
        Network::ServerPtr s_ptr = *it;
        if (!s_ptr.isNull())
        {
            Network::Server * server = s_ptr.data();
            Average::Load recv = server->receivedStats();
            Average::Load sent = server->sentStats();
            const char * type = Network::secureModeCStr(server->secureMode());
            if (it != listeners.begin())
                payload.append(',');
            payload.append('\"');
            payload.append(type);
            payload.append("\":{");
                payload.append("\"ip\":\"");  payload.append(server->listenIp().toString().toUtf8());  payload.append("\",");
                payload.append("\"port\":");  payload.append(QByteArray::number(server->port()));  payload.append(',');
                payload.append("\"received\":");
                    payload.append(recv.toJSON(BytesStatisticName));
                payload.append(',');
                payload.append("\"sent\":");
                    payload.append(sent.toJSON(BytesStatisticName));
            payload.append('}');
        }
        ++it;
    }
    payload.append('}');

    return payload;
}

#undef BytesStatisticName
#undef MessagesStatisticName
