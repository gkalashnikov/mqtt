#include "mqtt_session.h"
#include "mqtt_constants.h"
#include "mqtt_storer_interface.h"
#include "mqtt_store_publish_container.h"
#include "mqtt_encoder.h"
#include "mqtt_decoder.h"
#include <QUuid>
#include <QTimer>

#include <logger.h>

Q_GLOBAL_STATIC_WITH_ARGS(Mqtt::ConnectPacketPtr, kEmptyConnectPacket, (new Mqtt::ConnectPacket()))

using namespace Mqtt;
using namespace Mqtt::Store;

Session::Session(QObject * parent)
    :QObject(parent)
    ,m_is_present(false)
    ,m_is_conn_packet_expects(true)
    ,m_is_client_id_provided_by_server(false)
    ,m_is_normal_disconnect(false)
    ,m_request_response_information(false)
    ,m_request_problem_information(false)
    ,m_keep_alive_interval(0)
    ,m_expiry_interval(Constants::DefaultSessionTimeout)
    ,m_packet_size_max(Constants::DefaultMaxPacketSize)
    ,m_receive_maximum(Constants::DefaultReceiveMaximum)
    ,m_quota(Constants::DefaultReceiveMaximum)
    ,m_last_activity_time(QDateTime::currentSecsSinceEpoch())
    ,m_ban_duration(0)
    ,m_ban_timeout(0)
    ,m_topic_alias_maximum(Constants::TopicAliasMaximum)
    ,m_conn()
    ,m_conn_packet(*kEmptyConnectPacket())
    ,m_data_controller()
    ,m_subscriptions()
    ,m_timer(Q_NULLPTR)
    ,m_idctrl()
{
    m_keep_alive_interval = connectPacket().keepAliveInterval() * 2;

    for (int i = 0; i < 3; ++i) {
        m_flow[i].load().valuesReserve(1);
        m_flow[i].load().valueAppend(FlowControlWindowSize);
    }

    startTimer();
}

Session::~Session()
{
    if (m_timer != Q_NULLPTR) {
        m_timer->stop();
        delete m_timer;
        m_timer = Q_NULLPTR;
    }
    beforeDelete();
}

void Session::oneSecondTimer()
{
    for (int i = 0; i < 3; ++i)
        m_flow[i].oneSecondTimer();

    if (isBanned())
        decreaseBanTimeout();

    if (isConnected()) {
        m_data_controller.oneSecondTimer();
        if (elapsed() >= m_keep_alive_interval) {
            m_conn.close();
            restartElapsed();
        }
    }
    else {
        if (hasBeenExpired())
            emit expired();
    }
}

void Session::beforeDelete()
{
    hasBeenExpired() ? removeAllStoredPackets() : cancelAllInFligthPackets();
    m_pending_packets.syncAll();
}

void Session::setConnectPacket(ConnectPacketPtr connectPacket)
{
    if (m_conn_packet == connectPacket)
        return;

    m_conn_packet = connectPacket.isNull() ? *kEmptyConnectPacket() : connectPacket;
    m_is_conn_packet_expects = connectPacket.isNull();
    m_keep_alive_interval = this->connectPacket().keepAliveInterval() * 2; // 2 intervals in secs

    m_data_controller.setTimeout(m_keep_alive_interval);

    QVariant v = ControlPacket::property(this->connectPacket().properties(), PropertyId::SessionExpiryInterval);
    setExpiryInterval(v.toULongLong()); // interval in secs

    v = ControlPacket::property(this->connectPacket().properties(), PropertyId::MaximumPacketSize);
    m_packet_size_max = v.isValid() ? v.toInt() : Constants::DefaultMaxPacketSize;

    v = ControlPacket::property(this->connectPacket().properties(), PropertyId::TopicAliasMaximum);
    m_topic_alias_maximum = v.isValid() ? v.toUInt() : Constants::TopicAliasMaximum;

    v = ControlPacket::property(this->connectPacket().properties(), PropertyId::ReceiveMaximum);
    m_quota = m_receive_maximum = v.isValid() ? v.toUInt() : Constants::DefaultReceiveMaximum;

    m_request_response_information = ControlPacket::property(this->connectPacket().properties(), PropertyId::RequestResponseInformation).toBool();
    m_response_information = m_request_response_information ? QUuid::createUuid().toString().remove('{').remove('}') : QString();

    m_request_problem_information  = ControlPacket::property(this->connectPacket().properties(), PropertyId::RequestProblemInformation).toBool();

    m_broker_aliases.clear();
    m_client_aliases.clear();

    if (m_conn_packet != *kEmptyConnectPacket())
    {
        if (m_timer == Q_NULLPTR)
            startTimer();

        if (isClean())
            removeAllStoredPackets();
    }
}

void Session::clearConnection()
{
    m_conn = Network::ServerClient();
    m_is_conn_packet_expects = true;
    m_data_controller.clear();
    cancelAllInFligthPackets();
}

void Session::addPendingPacket(const PublishPacket & packet)
{
    if (!hasBeenExpired()) {
        static thread_local QString fake_client_id;
        m_pending_packets.add(m_pending_packets.nextOrderedKey(), fake_client_id, packet);
    }
}

void Session::removeAllStoredPackets()
{
    auto it = m_in_fligth_packets.begin();
    while (it != m_in_fligth_packets.end()) {
        m_idctrl.removeId(it.key());
        it = m_in_fligth_packets.erase(it);
        increaseQuota();
    }
    m_pending_packets.removeAll();
}

void Session::cancelAllInFligthPackets()
{
    auto it = m_in_fligth_packets.begin();
    while (it != m_in_fligth_packets.end()) {
        m_idctrl.removeId(it.key());
        QString key = it.value();
        {
            /* mark dup to reuse packet id*/
            auto unit_it = m_pending_packets.find(key);
            if (unit_it != m_pending_packets.end()) {
                if (!(*unit_it).isLoaded())
                    m_pending_packets.loadUnit(unit_it.key(), *unit_it);
                const_cast<PublishPacket&>(unit_it.value().packet()).setDuplicate(true);
                m_pending_packets.scheduleSync(key);
            }
        }
        it = m_in_fligth_packets.erase(it);
        increaseQuota();
    }
}

PublishUnit * Session::nextPacketToFligth()
{
    for ( ; ; )
    {
        int in_fligth_count = m_in_fligth_packets.count();
        if (m_pending_packets.size() > in_fligth_count)
        {
            Store::PublishContainer::iterator it = m_pending_packets.begin();
            std::advance(it, in_fligth_count);
            PublishUnit & unit = *it;
            if (!unit.isLoaded())
                m_pending_packets.loadUnit(it.key(), unit);
            if (unit.expired() && !unit.packet().isDuplicate()) {
                m_pending_packets.erase(it);
                continue;
            }
            return &unit;
        }
        break;
    }
    return Q_NULLPTR;
}

void Session::putPacketToFligth(quint16 id, PublishUnit * unit)
{
    const_cast<PublishPacket&>(unit->packet()).setPacketId(id);
    m_pending_packets.scheduleSync(unit->key());
    m_in_fligth_packets.insert(id, unit->key());
    unit->beforeSend();
    decreaseQuota();
}

void Session::packetDelivered(quint16 id, ReasonCodeV5 code, bool freeId)
{
    auto it = m_in_fligth_packets.find(id);
    if (it != m_in_fligth_packets.end())
    {
        QString key = it.value();
        if (ReasonCodeV5::PacketTooLarge == code || code < ReasonCodeV5::UnspecifiedError) {
            /* mean succesfully delivered */
            m_pending_packets.remove(key);
            if (freeId)
                m_idctrl.removeId(id);
        }
        else {
            /* not delivered */
            auto unit_it = m_pending_packets.find(key);
            if (unit_it != m_pending_packets.end()) {
                /* mark dup to reuse packet id */
                if (!(*unit_it).isLoaded())
                    m_pending_packets.loadUnit(unit_it.key(), *unit_it);
                const_cast<PublishPacket&>(unit_it.value().packet()).setDuplicate(true);
                m_pending_packets.scheduleSync(key);
            }
            m_idctrl.removeId(id);
        }
        m_in_fligth_packets.erase(it);
        increaseQuota();
    }
}

QByteArray Session::serialize() const
{
    QByteArray data;
    data.reserve(256);
    data.append(Encoder::encodeVariableByteInteger(m_last_activity_time));
    data.append(Encoder::encodeVariableByteInteger(m_ban_duration));
    data.append(Encoder::encodeVariableByteInteger(m_ban_timeout));
    // suscriptions with identifiers
    auto nodes = m_subscriptions.toTopicNodeList();
    data.append(Encoder::encodeFourByteInteger(nodes.count()));
    union { quint8 b; SubscribeOptions o = {}; } options;
    for (auto & node: nodes) {
        data.append(Encoder::encodeUTF8(node->topic()));
        SessionSubscriptionData * d = dynamic_cast<SessionSubscriptionData*>(node->data());
        data.append(Encoder::encodeFourByteInteger(d->identifier()));
        options.o = d->options();
        data.append(reinterpret_cast<const char*>(&options.b), sizeof(options.b));
    }
    data.append(m_conn_packet->serialize());
    return data.toHex();
}

void Session::unserialize(const QByteArray & dataHex)
{
    QByteArray data = QByteArray::fromHex(dataHex);

    const quint8 * buf = reinterpret_cast<const quint8 *>(data.constData());
    size_t bc = 0;
    qint64 rl = data.length();

    m_last_activity_time = Decoder::decodeVariableByteInteger(buf, &rl, &bc); buf += bc;
    m_ban_duration       = Decoder::decodeVariableByteInteger(buf, &rl, &bc); buf += bc;
    m_ban_timeout        = Decoder::decodeVariableByteInteger(buf, &rl, &bc); buf += bc;
    quint32 topics_count = Decoder::decodeFourByteInteger(buf, &rl, &bc);     buf += bc;
    union { quint8 b; SubscribeOptions o = {}; } options;
    for (quint32 i = 0; i < topics_count; ++i) {
        Topic   topic   (Decoder::decodeUTF8(buf, &rl, &bc));                 buf += bc;
        quint32 sub_id = Decoder::decodeFourByteInteger(buf, &rl, &bc);       buf += bc;
        options.b      = *buf;                                              ++buf; --rl;
        SubscriptionNode * node = m_subscriptions.provide(topic);
        if (node->data() == Q_NULLPTR)
            node->data() = new SessionSubscriptionData();
        auto data = dynamic_cast<SessionSubscriptionData*>(node->data());
        data->setOptions(options.o);
        data->setIdentifier(sub_id);
    }

    ConnectPacketPtr conn_packet(new ConnectPacket());
    conn_packet->unserialize(QByteArray::fromRawData(reinterpret_cast<const char*>(buf), rl));
    setConnectPacket(conn_packet);
}

void Session::startTimer()
{
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &Session::oneSecondTimer);
    m_timer->start(1000);
}

void Session::increaseFlowRate(QoS qos)
{
    switch (qos)
    {
        case QoS::Value_0: m_flow[0].increase(1); return;
        case QoS::Value_1: m_flow[1].increase(1); return;
        case QoS::Value_2: m_flow[2].increase(1); return;
        default: break;
    }
}

double Session::flowRate(QoS qos) const
{
    switch (qos)
    {
        case QoS::Value_0: return m_flow[0].load().value(0);
        case QoS::Value_1: return m_flow[1].load().value(0);
        case QoS::Value_2: return m_flow[2].load().value(0);
        default: break;
    }
    return 0.0;
}

void Session::ban(quint32 timeoutSeconds, bool accumulative)
{
    m_ban_duration = accumulative
                     ? (m_ban_duration + timeoutSeconds)
                     : timeoutSeconds;
    m_ban_timeout = m_ban_duration;
}

bool Session::processBrokerAlias(PublishPacket & pub)
{
    QVariant alias_prop = ControlPacket::property(pub.properties(), PropertyId::TopicAlias);

    if (alias_prop.isValid())
    {
        quint16 alias = alias_prop.toUInt();

        auto it = m_broker_aliases.find(alias);

        if (it == m_broker_aliases.end())
        {
            if (pub.topicName().isEmpty())
                return false;

            m_broker_aliases.insert(alias, pub.topicName());
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

bool Session::processClientAlias(PublishPacket & pub)
{
    auto it = m_client_aliases.find(pub.topicName());

    if (it == m_client_aliases.end())
    {
        if (m_client_aliases.count() == m_topic_alias_maximum)
            return false;

        quint16 alias = m_client_aliases.count() + 1;

        PublishPacket packet = pub;
        packet.setPayload(QByteArray());
        packet.setQoS(QoS::Value_0);
        packet.setRetain(false);
        packet.properties().clear();
        packet.properties().append(Property { PropertyId::TopicAlias, alias });

        QByteArray data = packet.serialize(protocolVersion());
        if (data.size() > maxPacketSize())
            return false;

        it = m_client_aliases.insert(pub.topicName(), alias);

        connection().write(data);
    }

    ControlPacket::setProperty(pub.properties(), PropertyId::TopicAlias, *it);

    return true;
}
