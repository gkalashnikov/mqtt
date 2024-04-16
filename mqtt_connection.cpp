#include "mqtt_connection.h"
#include "mqtt_disconnect_packet.h"
#include "mqtt_ping_packet.h"
#include "mqtt_constants.h"
#include "mqtt_broker_cmd_options.h"
#include "mqtt_topic.h"

#include <QCoreApplication>

#include "logger.h"

QDebug operator << (QDebug d, Mqtt::Connection::State state)
{
    switch (state)
    {
        case Mqtt::Connection::State::Unknown              : d << QStringLiteral("Unknown");               break;
        case Mqtt::Connection::State::NetworkDisconnected  : d << QStringLiteral("Network Disconnected");  break;
        case Mqtt::Connection::State::NetworkDisconnecting : d << QStringLiteral("Network Disconnecting"); break;
        case Mqtt::Connection::State::BrokerDisconnetced   : d << QStringLiteral("Broker Disconnetced");   break;
        case Mqtt::Connection::State::NetworkConnecting    : d << QStringLiteral("Network Connecting");    break;
        case Mqtt::Connection::State::NetworkConnected     : d << QStringLiteral("Network Connected");     break;
        case Mqtt::Connection::State::BrokerConnecting     : d << QStringLiteral("Broker Connecting");     break;
        case Mqtt::Connection::State::BrokerConnected      : d << QStringLiteral("Broker Connected");      break;
    }

    return d;
}

QDebug operator << (QDebug d, const Mqtt::Connection & connection)
{
    d << connection.networkClient();
    if (!connection.clientId().isEmpty())
        d << "client" << connection.clientId();
    return d;
}

class EmptyPacketHandler : public Mqtt::Connection::IPacketHandler
{
public:
    ~EmptyPacketHandler() override;
    bool handleControlPacket(Mqtt::PacketType type, const QByteArray &data) override;
    void oneSecondTimerEvent() override;
};

EmptyPacketHandler::~EmptyPacketHandler() { }
bool EmptyPacketHandler::handleControlPacket(Mqtt::PacketType, const QByteArray&) { return false; }
void EmptyPacketHandler::oneSecondTimerEvent() { }

Q_GLOBAL_STATIC(EmptyPacketHandler, emptyPacketHandler)

using namespace Mqtt;

Connection::Connection(QObject * socketController, IPacketHandler * handler, QObject * parent)
    :QObject(parent)
    ,m_state(State::Unknown)
    ,m_client(Network::ConnectionType::TCP, Network::SecureMode::NonSecured, QString(), 0, reinterpret_cast<quintptr>(this), this)
    ,m_socketController(socketController)
    ,m_subscribe_timer_id(0)
    ,m_one_second_timer_id(0)
    ,m_reconnect_period(5000)
    ,m_reconnect_timer_id(0)
    ,m_keepalive_time(0)
    ,m_connect_timeout(120)
    ,m_connect_abort_time(0)
    ,m_is_session_present(false)
    ,m_session_expiry_interval(Constants::DefaultSessionTimeout)
    ,m_broker_receive_maximum(Constants::DefaultReceiveMaximum)
    ,m_broker_quota(Constants::DefaultReceiveMaximum)
    ,m_broker_qos_maximum(QoS::Value_2)
    ,m_broker_retain_available(true)
    ,m_broker_max_packet_size(Constants::DefaultMaxPacketSize)
    ,m_broker_topic_alias_maximum(0)
    ,m_broker_wildcard_available(true)
    ,m_broker_subid_available(true)
    ,m_broker_shared_available(true)
    ,m_handler(Q_NULLPTR)
    ,m_disconnectReasonCode(ReasonCodeV5::Success)
    ,m_disconnectReasonString()
{
    m_connect.setCleanSession(true);
    m_connect.setProtocolVersion(Version::Ver_5_0);

    m_one_second_timer_id = startTimer(1000);

    setPacketHandler(handler);
}

Connection::~Connection()
{
    cancelSubscribeMessages();
    executeCheckSubscriptionsQueue();

    if (m_reconnect_timer_id != 0)
        killTimer(m_reconnect_timer_id);

    killTimer(m_one_second_timer_id);

    closeConnection(ReasonCodeV5::NormalDisconnection);

    setPacketHandler(Q_NULLPTR);
}

void Connection::connectToHost(const QString & host, quint16 port)
{
    if (host != m_client.address() || port != m_client.port())
    {
        if (m_state >= State::NetworkConnecting)
        {
            closeConnection(ReasonCodeV5::NoErrors);
            setState(State::NetworkDisconnected);
        }
        setHostAddress(host);
        setHostPort(port);
    }
    connectToHost();
}

void Connection::connectToHost()
{
    if (m_state >= State::NetworkConnecting)
        return;

    m_data.clear();
    QCoreApplication::postEvent(m_socketController, new Network::Event::OpenConnection(m_client), Qt::HighEventPriority);
    m_connect_abort_time = QDateTime::currentSecsSinceEpoch() + m_connect_timeout;
    setState(State::NetworkConnecting);
}

void Connection::checkConnectTimeout(qint64 secsSinceEpoch)
{
    if (secsSinceEpoch >= m_connect_abort_time)
    {
        QCoreApplication::postEvent(m_socketController, new Network::Event::CloseConnection(m_client.id()), Qt::HighEventPriority);
        setState(State::NetworkDisconnected);
    }
}

void Connection::write(const QByteArray & data) const
{
    QCoreApplication::postEvent(m_socketController, new Network::Event::Data(m_client.id(), data), Qt::HighEventPriority);
}

void Connection::setState(State state)
{
    if (m_state != state)
    {
        m_state = state;
        log_debug << *this << "state changed:" << m_state << end_log;
        emit stateChanged(m_state);
    }
}

void Connection::closeConnection(ReasonCodeV5 code, const QString & reason)
{
    if (m_state == State::BrokerConnected)
    {
        DisconnectPacket discon;
        discon.setReasonCode(code);
        if (!reason.isEmpty())
            discon.properties().append({ PropertyId::ReasonString, reason });
        write(discon.serialize(m_connect.protocolVersion(), m_broker_max_packet_size));
    }
    setState(State::NetworkDisconnecting);
    cancelSubscribeMessages();
    QCoreApplication::postEvent(m_socketController, new Network::Event::CloseConnection(m_client.id()), Qt::HighEventPriority);
}

void Connection::setPacketHandler(IPacketHandler * handler)
{
    m_handler = (handler == Q_NULLPTR) ? emptyPacketHandler() : handler;
}

bool Connection::event(QEvent * event)
{
    switch (static_cast<Network::Event::Type>(event->type()))
    {
        case Network::Event::Type::Data:
        {
            if (Network::Event::Data * e = dynamic_cast<Network::Event::Data *>(event))
            {
                e->accept();
                handleNetworkData(e->data);
                return true;
            }
            break;
        }

        case Network::Event::Type::CloseConnection:
        {
            if (Network::Event::CloseConnection * e = dynamic_cast<Network::Event::CloseConnection *>(event))
            {
                e->accept();
                handleCloseConnection();
                return true;
            }
            break;
        }

        case Network::Event::Type::ConnectionEstablished:
        {
            if (Network::Event::ConnectionEstablished * e = dynamic_cast<Network::Event::ConnectionEstablished *>(event))
            {
                e->accept();
                handleConnectionEstablished();
                return true;
            }
            break;
        }

        default: break;
    }

    return QObject::event(event);
}

void Connection::timerEvent(QTimerEvent * event)
{
    if (event->timerId() == m_one_second_timer_id)
    {
        event->accept();
        m_data.oneSecondTimer();
        oneSecondTimerEvent();
        m_handler->oneSecondTimerEvent();
        return;
    }

    if (event->timerId() == m_reconnect_timer_id)
    {
        event->accept();
        executeReconnect();
        return;
    }

    if (event->timerId() == m_subscribe_timer_id)
    {
        event->accept();
        executeCheckSubscriptionsQueue();
        return;
    }

    QObject::timerEvent(event);
}

void Connection::oneSecondTimerEvent()
{
    switch (state())
    {
        case State::NetworkDisconnected:
        {
            scheduleReconnect();
            break;
        }
        case State::NetworkConnecting:
        case State::BrokerConnecting:
        {
            checkConnectTimeout(QDateTime::currentSecsSinceEpoch());
            break;
        }
        case State::BrokerConnected:
        {
            checkKeepAlive(QDateTime::currentSecsSinceEpoch());
            break;
        }
        default: break;
    }
}

void Connection::handleConnectionEstablished()
{
    log_note << *this << "network connection established" << end_log;

    setState(State::NetworkConnected);

    ConnectPacket connect = m_connect;
    auto props = connect.properties();

    QVariant v = ControlPacket::property(connect.properties(), PropertyId::SessionExpiryInterval);
    if (!v.isValid())
        ControlPacket::setProperty(props, PropertyId::SessionExpiryInterval, m_session_expiry_interval);
    else
        m_session_expiry_interval = v.toLongLong();

    v = ControlPacket::property(connect.properties(), PropertyId::ReceiveMaximum);
    if (!v.isValid())
        ControlPacket::setProperty(props, PropertyId::ReceiveMaximum, Constants::DefaultReceiveMaximum);

    connect.setProperties(props);
    write(connect.serialize());

    setState(State::BrokerConnecting);

    m_connect_abort_time = QDateTime::currentSecsSinceEpoch() + m_connect_timeout;
}

void Connection::handleNetworkData(const QByteArray & data)
{
    m_data.append(data);

    while (m_data.packetAvailable())
    {
        QByteArray data = m_data.takePacket();

        PacketType type = ControlPacket::extractType(data);

        log_trace_6 << *this << "received packet:" << type << printByteArrayPartly(data, 30) << end_log;

        switch (type)
        {
            case PacketType::CONNACK:        { handleConnackPacket(data);      continue; }
            case PacketType::PINGRESP:       { handlePingResponsePacket(data); continue; }
            case PacketType::DISCONNECT:     { handleDisconnectPacket(data);   continue; }
            case PacketType::AUTH:           { handleAuthPacket(data);         continue; }
            case PacketType::SUBACK:         { handleSubAckPacket(data);       continue; }
            case PacketType::UNSUBACK:       { handleUnsubAckPacket(data);     continue; }
            default:
            {
                if (m_handler->handleControlPacket(type, data))
                    continue;
                break;
            }
        }

        log_warning << *this << state() << "received packet's type not handled:" << type << printByteArrayPartly(data, 30) << end_log;

        closeConnection(ReasonCodeV5::ProtocolError);
        break;
    }
}

void Connection::handleCloseConnection()
{
    log_note << *this << "connection closed" << end_log;
    setState(State::NetworkDisconnected);
    scheduleReconnect();
}

void Connection::handleConnackPacket(const QByteArray & data)
{
    ConnackPacket connack;

    if (!connack.unserialize(data, protocolVersion()))
    {
        log_warning << *this << PacketType::CONNACK << "can't unserialize:" << connack.unserializeReasonString() << end_log;
        closeConnection(connack.unserializeReasonCode(), connack.unserializeReasonString());
        return;
    }

    log_trace << *this << PacketType::CONNACK << "unserialized succesfully" << end_log;

    if (connack.reasonCodeV5() != ReasonCodeV5::Success)
    {
        if (connack.reasonCodeV5() < ReasonCodeV5::UnspecifiedError)
        {
            log_warning << *this << PacketType::CONNACK << "answer error, code:" << connack.returnCodeV3() << end_log;

            if (connack.returnCodeV3() == ReturnCodeV3::UnacceptableProtocolVersion)
            {
                changeProtocolVersion();
                scheduleReconnect(1);
            }
        }
        else
        {
            log_warning << *this << PacketType::CONNACK << "answer error, code:" << connack.reasonCodeV5()
                        << "reason:" << ControlPacket::property(connack.properties(), PropertyId::ReasonString).toString() << end_log;

            switch (connack.reasonCodeV5())
            {
                case ReasonCodeV5::UnsupportedProtocolVersion:
                {
                    changeProtocolVersion();
                    scheduleReconnect(1);
                    break;
                }

                case ReasonCodeV5::UseAnotherServer:
                case ReasonCodeV5::ServerMoved:
                {
                    processServerReference(connack.reasonCodeV5(),connack.properties());
                    break;
                }

                default: break;
            }
        }
        return;
    }

    applyConnack(connack);

    log_note << *this << "broker connetion established, protocol version" << protocolVersion() << end_log;

    m_disconnectReasonCode   = ReasonCodeV5::Success;
    m_disconnectReasonString = QString();

    setState(State::BrokerConnected);

    enqueueAllSubscribeMessages();
}

void Connection::checkKeepAlive(qint64 secsSinceEpoch)
{
    if (secsSinceEpoch >= m_keepalive_time) {
        write(PingRequestPacket().serialize(protocolVersion()));
        m_keepalive_time += keepAliveInterval();
        log_trace << *this << PacketType::PINGREQ << "sent" << end_log;
    }
}

void Connection::handlePingResponsePacket(const QByteArray & data)
{
    Q_UNUSED(data)
    log_trace << *this << PacketType::PINGRESP << "received" << end_log;
}

void Connection::handleDisconnectPacket(const QByteArray & data)
{
    log_trace << *this << PacketType::DISCONNECT << end_log;

    DisconnectPacket discon;
    if (!discon.unserialize(data, protocolVersion())) {
        log_warning << *this << PacketType::DISCONNECT << "can't unserialize:" << discon.unserializeReasonString() << end_log;
        closeConnection(discon.unserializeReasonCode(), discon.unserializeReasonString());
        return;
    }

    log_trace << *this << PacketType::DISCONNECT << "unserialized succesfully" << end_log;

    m_disconnectReasonCode   = discon.reasonCode();
    m_disconnectReasonString = ControlPacket::property(discon.properties(), PropertyId::ReasonString).toString();

    log_note << *this << "broker connection closed, code:" << m_disconnectReasonCode << "reason:" << m_disconnectReasonString << end_log;

    QVariant v = ControlPacket::property(discon.properties(), PropertyId::SessionExpiryInterval);
    if (v.isValid())
        m_session_expiry_interval = v.toLongLong();

    switch (discon.reasonCode())
    {
        case ReasonCodeV5::UseAnotherServer:
        case ReasonCodeV5::ServerMoved:
            processServerReference(discon.reasonCode(), discon.properties());
            break;

        default: break;
    }

    setState(State::BrokerDisconnetced);

    closeConnection(ReasonCodeV5::NormalDisconnection);
}

void Connection::processServerReference(ReasonCodeV5 reasonCode, Properties & properties)
{
    QVariant v = ControlPacket::property(properties, PropertyId::ServerReference);
    if (v.isValid())
    {
        QString reference = v.toString();

        log_important << *this << (reasonCode == ReasonCodeV5::UseAnotherServer
                         ? QStringLiteral("the client should temporarily use another server:")
                         : QStringLiteral("the client should permanently use another server:"))
                      << reference << end_log;

        auto result = BrokerOptions::parseConnectionReference(reference);
        Network::SecureMode mode = std::get<0>(result);
        if (mode != Network::SecureMode::Unknown)
        {
            setSecureMode(mode);
            setConnectionType(std::get<1>(result));
            connectToHost(std::get<2>(result), std::get<3>(result));
        }
    }
}

void Connection::handleAuthPacket(const QByteArray & data)
{
    // TODO: later
    Q_UNUSED(data)
    log_warning << *this << PacketType::AUTH << "not supported currently" << end_log;
}

void Connection::handleSubAckPacket(const QByteArray & data)
{
    SubscribeAckPacket suback;

    if (!suback.unserialize(data, protocolVersion())) {
        log_warning << *this << PacketType::SUBACK << "can't unserialize:" << suback.unserializeReasonString() << end_log;
        closeConnection(suback.unserializeReasonCode(), suback.unserializeReasonString());
        return;
    }

    log_trace << *this << PacketType::SUBACK << "unserialized succesfully" << end_log;

    auto it = m_subscribe_in_fligth.find(suback.packetId());
    if (it != m_subscribe_in_fligth.end())
    {
        m_idctrl.removeId(suback.packetId());
        SubscribePacket sub = it.value();
        m_subscribe_in_fligth.erase(it);
        for (decltype(suback.returnCodes().count()) i = 0; i < suback.returnCodes().count(); ++i)
        {
            QString topic = sub.topics().count() > i ? sub.topics().at(i).first : QString();
            if (suback.returnCodes().at(i) >= ReasonCodeV5::UnspecifiedError)
                log_error << *this << PacketType::SUBACK << "can't subscribe to" << topic << ", error:" << suback.returnCodes().at(i) << end_log;
            else
                log_note << *this << PacketType::SUBACK << "subcribed to" << topic << ", result:" << suback.returnCodes().at(i) << end_log;
        }
        increaseBrokerQuota();
        return;
    }

    closeConnection(ReasonCodeV5::PacketIdentifierNotFound);
}

void Connection::handleUnsubAckPacket(const QByteArray & data)
{
    UnsubscribeAckPacket unsuback;
    if (!unsuback.unserialize(data, protocolVersion())) {
        log_warning << *this << PacketType::UNSUBACK << "can't unserialize:" << unsuback.unserializeReasonString() << end_log;
        closeConnection(unsuback.unserializeReasonCode(), unsuback.unserializeReasonString());
        return;
    }

    log_trace << *this << PacketType::UNSUBACK << "unserialized succesfully" << end_log;

    auto it = m_unsubscribe_in_fligth.find(unsuback.packetId());
    if (it != m_unsubscribe_in_fligth.end())
    {
        m_idctrl.removeId(unsuback.packetId());
        UnsubscribePacket unsub = it.value();
        m_unsubscribe_in_fligth.erase(it);
        for (decltype(unsuback.returnCodes().count()) i = 0; i < unsuback.returnCodes().count(); ++i)
        {
            QString topic = unsub.topics().count() > i ? unsub.topics().at(i) : QString();
            if (unsuback.returnCodes().at(i) >= ReasonCodeV5::UnspecifiedError)
                log_error << *this << PacketType::UNSUBACK << "can't unsubscribe from" << topic << ", error:" << unsuback.returnCodes().at(i) << end_log;
            else
                log_note << *this << PacketType::UNSUBACK << "unsubcribed from" << topic << ", result:" << unsuback.returnCodes().at(i) << end_log;
        }
        increaseBrokerQuota();
        return;
    }

    closeConnection(ReasonCodeV5::PacketIdentifierNotFound);
}

void Connection::changeProtocolVersion()
{
    auto v = protocolVersion();

    switch (v)
    {
        case Version::Ver_5_0:    setProtocolVersion(Version::Ver_3_1_1);  break;
        case Version::Ver_3_1_1:  setProtocolVersion(Version::Ver_3_1);    break;
        default:                  setProtocolVersion(Version::Ver_5_0);    break;
    }

    log_important << *this << "will try change protocol's version from" << v << "to" << protocolVersion() << end_log;
}

void Connection::scheduleReconnect()
{
    if (m_reconnect_period == 0)
        return;

    if (m_reconnect_timer_id == 0)
        m_reconnect_timer_id = startTimer(m_reconnect_period);
}

void Connection::scheduleReconnect(quint32 seconds)
{
    if (m_reconnect_timer_id != 0)
        killTimer(m_reconnect_timer_id);
    m_reconnect_timer_id = startTimer(seconds * 1000);
}

void Connection::executeReconnect()
{
    if (m_reconnect_timer_id != 0) {
        killTimer(m_reconnect_timer_id);
        m_reconnect_timer_id = 0;
    }
    connectToHost();
}

void Connection::applyConnack(ConnackPacket & connack)
{
    m_is_session_present = connack.isSessionPresent();

    QVariant v = ControlPacket::property(connack.properties(), PropertyId::SessionExpiryInterval);
    if (v.isValid())
        m_session_expiry_interval = v.toLongLong();

    v = ControlPacket::property(connack.properties(), PropertyId::ReceiveMaximum);
    m_broker_quota = m_broker_receive_maximum = v.isValid() ? v.toUInt() : Constants::DefaultReceiveMaximum;

    v = ControlPacket::property(connack.properties(), PropertyId::MaximumQoS);
    m_broker_qos_maximum = v.isValid() ? QoS(v.toUInt()) : QoS::Value_2;

    v = ControlPacket::property(connack.properties(), PropertyId::RetainAvailable);
    m_broker_retain_available = v.isValid() ? v.toBool() : true;

    v = ControlPacket::property(connack.properties(), PropertyId::MaximumPacketSize);
    m_broker_max_packet_size = v.isValid() ? v.toULongLong() : Constants::DefaultMaxPacketSize;

    v = ControlPacket::property(connack.properties(), PropertyId::AssignedClientIdentifier);
    if (v.isValid()) {
        QString client_id = v.toString();
        m_connect.setClientId(client_id);
        log_important << *this << "client id:" << client_id << "was assigned by broker" << end_log;
    }

    v = ControlPacket::property(connack.properties(), PropertyId::TopicAliasMaximum);
    m_broker_topic_alias_maximum = v.isValid() ? v.toUInt() : 0;

    v = ControlPacket::property(connack.properties(), PropertyId::WildcardSubscriptionAvailable);
    m_broker_wildcard_available = v.isValid() ? v.toBool() : true;

    v = ControlPacket::property(connack.properties(), PropertyId::SubscriptionIdentifierAvailable);
    m_broker_subid_available = v.isValid() ? v.toBool() : true;

    v = ControlPacket::property(connack.properties(), PropertyId::SharedSubscriptionAvailable);
    m_broker_shared_available = v.isValid() ? v.toBool() : true;

    v = ControlPacket::property(connack.properties(), PropertyId::ServerKeepAlive);
    if (v.isValid())
        setKeepAliveInterval(v.toUInt());

    v = ControlPacket::property(connack.properties(), PropertyId::ResponseInformation);
    m_broker_response_information = v.isValid() ? v.toString() : QString();

    m_keepalive_time = QDateTime::currentSecsSinceEpoch() + keepAliveInterval();

    m_data.setTimeout(m_keepalive_time);
}

void Connection::subscribe(const QString & topic, QoS maxQoS, bool noLocal, bool retainAsPublished, SubscribeOptions::RetainHandling retainHandling, quint32 subscriptionId)
{
    subscribe(topic, SubscribeOptions { maxQoS, noLocal, retainAsPublished, retainHandling }, subscriptionId);
}

void Connection::subscribe(const QString & topic, SubscribeOptions options, quint32 subscriptionId)
{
    auto subscriptionContains = [this](const QString & topic, SubscribeOptions options, quint32 subscriptionId)->bool {
        for (decltype(m_subscriptions.size()) i = 0; i < m_subscriptions.size(); ++i) {
            if (std::get<0>(m_subscriptions[i]) == topic) {
                std::get<1>(m_subscriptions[i]) = options;
                std::get<2>(m_subscriptions[i]) = subscriptionId;
                return true;
            }
        }
        return false;
    };

    if (!subscriptionContains(topic, options, subscriptionId))
        m_subscriptions.append(std::make_tuple(topic, options, subscriptionId));

    if (state() == State::BrokerConnected)
        enqueueSubscribeMessage(topic, options, subscriptionId);
}

void Connection::unsubscribe(const QString & topic)
{
    auto it = m_subscriptions.begin();
    while (it < m_subscriptions.end()) {
        if (std::get<0>(*it) == topic) {
            m_subscriptions.erase(it);
            break;
        } ++it;
    }
    enqueueUnsubscribeMessage(topic);
}

void Connection::enqueueAllSubscribeMessages()
{
    for (auto tuple: m_subscriptions)
        enqueueSubscribeMessage(std::get<0>(tuple), std::get<1>(tuple), std::get<2>(tuple));
}

void Connection::enqueueSubscribeMessage(const QString & topic, SubscribeOptions options, quint32 subscriptionId)
{
    SubscribePacket sub;
    sub.topics().append(SubscriptionPair{ topic, options });
    if (brokerSubscriptionIdentifiersAvailable() && subscriptionId != 0)
        sub.properties().append(Property{ PropertyId::SubscriptionIdentifier, subscriptionId });
    m_subscribe_queue.enqueue(sub);
    scheduleCheckSubscriptionsQueue();
}

void Connection::enqueueUnsubscribeMessage(const QString & topic)
{
    UnsubscribePacket unsub;
    unsub.topics().append(topic);
    m_unsubscribe_queue.enqueue(unsub);
    scheduleCheckSubscriptionsQueue();
}

void Connection::cancelSubscribeMessages()
{
    for (auto it = m_subscribe_in_fligth.begin(); it != m_subscribe_in_fligth.end(); ++it)
        m_idctrl.removeId(it.key());
    m_subscribe_in_fligth.clear();
    m_subscribe_queue.clear();
}

void Connection::scheduleCheckSubscriptionsQueue()
{
    if (m_subscribe_timer_id == 0)
        m_subscribe_timer_id = startTimer(0);
}

void Connection::executeCheckSubscriptionsQueue()
{
    if (m_subscribe_timer_id != 0) {
        killTimer(m_subscribe_timer_id);
        m_subscribe_timer_id = 0;
    }
    checkSubscriptionsQueue();
}

void Connection::checkSubscriptionsQueue()
{
    if (state() != State::BrokerConnected)
        return;

    while (!m_subscribe_queue.isEmpty() && hasBrokerQuota())
    {
        quint16 id = m_idctrl.generateId();
        if (id == 0) return;
        m_subscribe_queue.first().setPacketId(id);
        write(m_subscribe_queue.first().serialize(protocolVersion()));
        m_subscribe_in_fligth[id] = m_subscribe_queue.dequeue();
        decreaseBrokerQuota();
    }

    while (!m_unsubscribe_queue.isEmpty() && hasBrokerQuota())
    {
        quint16 id = m_idctrl.generateId();
        if (id == 0) return;
        m_unsubscribe_queue.first().setPacketId(id);
        write(m_unsubscribe_queue.first().serialize(protocolVersion()));
        m_unsubscribe_in_fligth[id] = m_unsubscribe_queue.dequeue();
        decreaseBrokerQuota();
    }
}
