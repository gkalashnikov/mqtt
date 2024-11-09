#include "mqtt_client.h"
#include "mqtt_disconnect_packet.h"
#include "mqtt_ping_packet.h"
#include "mqtt_constants.h"
#include "mqtt_broker_cmd_options.h"
#include "mqtt_topic.h"

#include <QCoreApplication>

#include "logger.h"

using namespace Mqtt;

Client::Client(QObject * socketController, QObject * parent)
    :Connection(socketController, this, parent)
    ,m_timer_id(0)
{
    connect(this, &Connection::stateChanged, this, &Client::stateChanged);
}

Client::~Client()
{
    if (m_timer_id != 0)
        killTimer(m_timer_id);
}

void Client::stateChanged(State state)
{
    switch (state)
    {
        case State::BrokerConnected:
        {
            executeCheckMessageQueue();
            break;
        }

        case State::NetworkDisconnecting:
        {
            break;
        }

        default: break;
    }
}

qint32 Client::publish(const QString & topic, const QByteArray & payload, QoS qos, bool retain)
{
    return publish(topic, payload, Properties(), qos, retain);
}

qint32 Client::publish(const QString & topic, const QByteArray & payload, const Properties & properties, QoS qos, bool retain)
{
    quint16 id = (qos == QoS::Value_0) ? 0 : idctrl().generateId();

    if (qos != QoS::Value_0 && id == 0)
        return -1;

    PublishPacket pub;
    pub.setTopicName(topic);
    pub.setPayload(payload);
    pub.properties() = properties;
    pub.setQoS(qos);
    pub.setRetain(retain);
    pub.setDuplicate(false);
    pub.setPacketId(id);
    enqueuePublishMessage(pub);

    return id;
}

void Client::timerEvent(QTimerEvent * event)
{
    if (event->timerId() == m_timer_id) {
        event->accept();
        executeCheckMessageQueue();
        return;
    }

    Connection::timerEvent(event);
}

void Client::oneSecondTimerEvent()
{
    switch (state())
    {
        case State::BrokerConnected:
        {
            checkMessageQueue();
            break;
        }
        default: break;
    }
}

bool Client::handleControlPacket(PacketType type, const QByteArray & data)
{
    switch (type)
    {
        case PacketType::PUBLISH:        { handlePublishPacket(data);      return true; }
        case PacketType::PUBACK:         { handlePubAckPacket(data);       return true; }
        case PacketType::PUBREC:         { handlePubRecPacket(data);       return true; }
        case PacketType::PUBREL:         { handlePubRelPacket(data);       return true; }
        case PacketType::PUBCOMP:        { handlePubCompPacket(data);      return true; }
        default: break;
    }

    return false;
}

void Client::handlePublishPacket(const QByteArray & data)
{
    PublishPacket pub;

    if (!pub.unserialize(data, protocolVersion())) {
        log_warning << *this << PacketType::PUBLISH << "can't unserialize:" << pub.unserializeReasonString() << end_log;
        closeConnection(pub.unserializeReasonCode(), pub.unserializeReasonString());
        return;
    }

    log_trace << *this << PacketType::PUBLISH << "unserialized succesfully" << end_log;

    const Topic topic(pub.topicName());
    if (!topic.isValidForPublish()) {
        log_warning << *this << PacketType::PUBLISH << "received invalid publish topic:" << pub.topicName() << end_log;
        closeConnection(ReasonCodeV5::TopicNameInvalid);
        return;
    }

    if (!processClientAlias(pub)) {
        log_warning << *this << PacketType::PUBLISH << "unserialized, but wrong broker alias" << end_log;
        closeConnection(ReasonCodeV5::ProtocolError);
        return;
    }

    switch (pub.QoS())
    {
        case QoS::Value_0:
        {
            emit messageReceived(pub.topicName(), pub.payload(), pub.properties(), pub.QoS(), pub.isRetained());
            return;
        }

        case QoS::Value_1:
        {
            emit messageReceived(pub.topicName(), pub.payload(), pub.properties(), pub.QoS(), pub.isRetained());
            PublishAckPacket puback;
            puback.setPacketId(pub.packetId());
            puback.setReasonCode(ReasonCodeV5::Success);
            write(puback.serialize(protocolVersion(), brokerMaximumPacketSize()));
            return;
        }

        case QoS::Value_2:
        {
            PublishRecPacket pubrec;
            pubrec.setPacketId(pub.packetId());
            if (idctrl().addId(pub.packetId())) {
                emit messageReceived(pub.topicName(), pub.payload(), pub.properties(), pub.QoS(), pub.isRetained());
                pubrec.setReasonCode(ReasonCodeV5::Success);
            } else {
                pubrec.setReasonCode(ReasonCodeV5::PacketIdentifierInUse);
                if (protocolVersion() < Version::Ver_5_0) {
                    closeConnection(ReasonCodeV5::PacketIdentifierInUse);
                    return;
                }
            }
            write(pubrec.serialize(protocolVersion(), brokerMaximumPacketSize()));
            return;
        }

        default: break;
    }

    closeConnection(ReasonCodeV5::MalformedPacket);
}

void Client::handlePubAckPacket(const QByteArray & data)
{
    PublishAckPacket ack;

    if (!ack.unserialize(data, protocolVersion())) {
        log_warning << *this << PacketType::PUBACK << "can't unserialize:" << ack.unserializeReasonString() << end_log;
        closeConnection(ack.unserializeReasonCode(), ack.unserializeReasonString());
        return;
    }

    log_trace << *this << PacketType::PUBACK << "unserialized succesfully" << end_log;

    packetDelivered(ack.packetId(), ack.reasonCode(), true);

    checkMessageQueue();
}

void Client::handlePubRecPacket(const QByteArray & data)
{
    PublishRecPacket rec;

    if (!rec.unserialize(data, protocolVersion())) {
        log_warning << *this << PacketType::PUBREC << "can't unserialize:" << rec.unserializeReasonString() << end_log;
        closeConnection(rec.unserializeReasonCode(), rec.unserializeReasonString());
        return;
    }

    log_trace << *this << PacketType::PUBREC << "unserialized succesfully" << end_log;

    packetDelivered(rec.packetId(), rec.reasonCode(), false);

    if (rec.reasonCode() < ReasonCodeV5::UnspecifiedError)
    {
        PublishRelPacket rel;
        rel.setPacketId(rec.packetId());
        rel.setReasonCode(ReasonCodeV5::Success);
        write(rel.serialize(protocolVersion(), brokerMaximumPacketSize()));
    }
    else
    {
        checkMessageQueue();
    }
}

void Client::handlePubRelPacket(const QByteArray & data)
{
    PublishRelPacket rel;

    if (!rel.unserialize(data, protocolVersion())) {
        log_warning << *this << PacketType::PUBREL << "can't unserialize:" << rel.unserializeReasonString() << end_log;
        closeConnection(rel.unserializeReasonCode(), rel.unserializeReasonString());
        return;
    }

    log_trace << *this << PacketType::PUBREL << "unserialized succesfully" << end_log;

    idctrl().removeId(rel.packetId());

    PublishCompPacket comp;
    comp.setPacketId(rel.packetId());
    comp.setReasonCode(ReasonCodeV5::Success);

    write(comp.serialize(protocolVersion(), brokerMaximumPacketSize()));
}

void Client::handlePubCompPacket(const QByteArray & data)
{
    PublishCompPacket comp;

    if (!comp.unserialize(data, protocolVersion())) {
        log_warning << *this << PacketType::PUBCOMP << "can't unserialize:" << comp.unserializeReasonString() << end_log;
        closeConnection(comp.unserializeReasonCode(), comp.unserializeReasonString());
        return;
    }

    log_trace << *this << PacketType::PUBCOMP << "unserialized succesfully" << end_log;

    packetDelivered(comp.packetId(), comp.reasonCode(), true);

    checkMessageQueue();
}

void Client::enqueuePublishMessage(const PublishPacket & packet)
{
    m_publish_queue.enqueue(packet);
    scheduleCheckMessageQueue();
}

void Client::packetDelivered(quint16 id, ReasonCodeV5 code, bool endOfDelivery)
{
    auto it = m_publish_in_fligth.find(id);

    if (it != m_publish_in_fligth.end())
    {
        if (code < ReasonCodeV5::UnspecifiedError) {
            emit messageDelivered(id, code);
        } else {
            emit messageNotDelivered(id, code);
            endOfDelivery = true;
        }
        m_publish_in_fligth.erase(it);

        if (endOfDelivery) {
            idctrl().removeId(id);
            increaseBrokerQuota();
        } else {
            m_publish_rec_received.insert(id);
        }
    }
    else
    {
        auto it = m_publish_rec_received.find(id);
        if (it != m_publish_rec_received.end()) {
            idctrl().removeId(id);
            m_publish_rec_received.erase(it);
            increaseBrokerQuota();
        }
    }
}

void Client::scheduleCheckMessageQueue()
{
    if (m_timer_id == 0)
        m_timer_id = startTimer(0);
}

void Client::executeCheckMessageQueue()
{
    if (m_timer_id != 0) {
        killTimer(m_timer_id);
        m_timer_id = 0;
    }
    checkMessageQueue();
}

void Client::checkMessageQueue()
{
    if (state() != State::BrokerConnected)
        return;

    while (!m_publish_queue.isEmpty() && hasBrokerQuota())
    {
        const QoS qos = m_publish_queue.first().QoS();

        if (qos != QoS::Value_0)
        {
            if (m_publish_queue.first().packetId() == 0) {
                quint16 id = idctrl().generateId();
                if (id == 0) return;
                m_publish_queue.first().setPacketId(id);
            }
        }

        QByteArray data = m_publish_queue.first().serialize(protocolVersion());

        bool can_send = (data.size() <= brokerMaximumPacketSize());

        if (!can_send)
        {
            if (processBrokerAlias(m_publish_queue.first())) {
                m_publish_queue.first().setTopicName(QString());
                data = m_publish_queue.first().serialize(protocolVersion());
                can_send = (data.size() <= brokerMaximumPacketSize());
            }
            if (!can_send) {
                log_warning << *this << "can't publish message to broker, packet size exceeded broker max packet size" << end_log;
                m_publish_queue.dequeue();
                continue;
            }
        }

        write(data);

        PublishPacket pub = m_publish_queue.dequeue();

        if (qos != QoS::Value_0)
        {
            m_publish_in_fligth[pub.packetId()] = pub;
            decreaseBrokerQuota();
        }
    }
}

bool Client::processBrokerAlias(PublishPacket & pub)
{
    auto it = m_broker_aliases.find(pub.topicName());

    if (it == m_broker_aliases.end())
    {
        if (m_broker_aliases.count() == brokerTopicAliasMaximum())
            return false;

        quint16 alias = m_broker_aliases.count() + 1;

        PublishPacket packet = pub;
        packet.setPayload(QByteArray());
        packet.setQoS(QoS::Value_0);
        packet.setRetain(false);
        packet.properties().clear();
        packet.properties().append(Property { PropertyId::TopicAlias, alias });

        QByteArray data = packet.serialize(protocolVersion());
        if (data.size() > brokerMaximumPacketSize())
            return false;

        it = m_broker_aliases.insert(pub.topicName(), alias);

        write(data);
    }

    ControlPacket::setProperty(pub.properties(), PropertyId::TopicAlias, *it);

    return true;
}

bool Client::processClientAlias(PublishPacket & pub)
{
    QVariant alias_prop = ControlPacket::property(pub.properties(), PropertyId::TopicAlias);

    if (alias_prop.isValid())
    {
        quint16 alias = alias_prop.toUInt();

        auto it = m_client_aliases.find(alias);

        if (it == m_client_aliases.end())
        {
            if (pub.topicName().isEmpty())
                return false;

            m_client_aliases.insert(alias, pub.topicName());
        }
        else
        {
            if (pub.topicName().isEmpty())
                pub.setTopicName(*it);
            else
                *it = pub.topicName();
        }

        ControlPacket::removeProperty(pub.properties(), PropertyId::TopicAlias);
    }

    return true;
}
