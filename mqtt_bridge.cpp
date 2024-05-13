#include "mqtt_bridge.h"
#include "mqtt_publish_packet.h"
#include "mqtt_constants.h"

using namespace Mqtt;

Bridge::Handler::Handler(Type type, Bridge * bridge)
    :type(type)
    ,bridge(bridge)
{

}

Bridge::Handler::~Handler()
{

}

bool Bridge::Handler::handleControlPacket(PacketType type, const QByteArray & data)
{
    switch (this->type)
    {
        case Type::Local:  return bridge->handleLocalControlPacket(type, data);
        case Type::Remote: return bridge->handleRemoteControlPacket(type, data);
    }

    return false;
}

void Bridge::Handler::oneSecondTimerEvent()
{
    switch (this->type)
    {
        case Type::Local:  bridge->sendRemoteMessageQueue(); break;
        case Type::Remote: bridge->sendLocalMessageQueue();  break;
    }
}

Bridge::Bridge(QObject * socketController, QObject * parent)
    :QObject(parent)
    ,m_step(Step::FirstOpenLocalConnection)
    ,m_local_handler(Bridge::Handler::Type::Local, this)
    ,m_local_connection(socketController, Q_NULLPTR)
    ,m_remote_handler(Bridge::Handler::Type::Remote, this)
    ,m_remote_connection(socketController, Q_NULLPTR)
{
    m_local_connection.setPacketHandler(&m_local_handler);
    m_remote_connection.setPacketHandler(&m_remote_handler);

    connect( &m_local_connection,  &Connection::stateChanged, this, &Bridge::localStateChanged  );
    connect( &m_remote_connection, &Connection::stateChanged, this, &Bridge::remoteStateChanged );
}

Bridge::~Bridge()
{

}

void Bridge::localStateChanged(Connection::State state)
{
    switch (state)
    {
        case Connection::State::BrokerConnected:
        {
            switch (m_step)
            {
                case Step::FirstOpenLocalConnection:
                {
                    m_step = Step::OpenRemoteConnection;
                    m_local_connection.closeConnection(ReasonCodeV5::Success);
                    connectToHosts();
                    break;
                }
                case Step::OpenLocalConnection:
                {
                    m_step = Step::OpenConnectionDone;
                    break;
                }
            }
            break;
        }

        case Connection::State::NetworkDisconnected:
        {
            if (m_step == Step::OpenConnectionDone)
                m_remote_connection.closeConnection(m_local_connection.disconnectReasonCode(), m_local_connection.disconnectReasonString());
            break;
        }
    }
}

void Bridge::remoteStateChanged(Connection::State state)
{
    switch (state)
    {
        case Connection::State::BrokerConnected:
        {
            if (m_step != Step::OpenConnectionDone) {
                m_step = Step::OpenLocalConnection;
                connectToHosts();
            } else {
                m_local_connection.connectToHost();
            }
            break;
        }

        case Connection::State::NetworkDisconnected:
        {
            m_local_connection.closeConnection(m_remote_connection.disconnectReasonCode(), m_remote_connection.disconnectReasonString());
            break;
        }
    }
}

void Bridge::connectToHosts()
{
    switch (m_step)
    {
        case Step::FirstOpenLocalConnection:
        {
            m_local_connection.setProtocolVersion(Version::Ver_5_0);
            m_local_connection.connectToHost();
            break;
        }

        case Step::OpenRemoteConnection:
        {
            if (m_remote_connection.state() > Connection::State::NetworkConnected)
                m_remote_connection.closeConnection(ReasonCodeV5::Success);

            Properties conn_props {
                                    { PropertyId::TopicAliasMaximum, m_local_connection.brokerTopicAliasMaximum() }
                                   ,{ PropertyId::ReceiveMaximum   , m_local_connection.brokerReceiveMaximum()    }
                                  };

            m_remote_connection.setProtocolVersion(Version::Ver_5_0);
            m_remote_connection.setConnectProperties(conn_props);
            m_remote_connection.connectToHost();
            break;
        }

        case Step::OpenLocalConnection:
        {
            if (m_local_connection.state() > Connection::State::NetworkConnected)
                m_local_connection.closeConnection(ReasonCodeV5::Success);

            Properties conn_props { { PropertyId::MaximumPacketSize, m_remote_connection.brokerMaximumPacketSize()  }
                                   ,{ PropertyId::TopicAliasMaximum, m_remote_connection.brokerTopicAliasMaximum()  }
                                   ,{ PropertyId::ReceiveMaximum   , m_remote_connection.brokerReceiveMaximum()     }
                                  };

            m_local_connection.setConnectProperties(conn_props);
            m_local_connection.connectToHost();
            break;
        }

        case Step::OpenConnectionDone:
        {
            m_remote_connection.connectToHost();
            break;
        }
    }
}

void Bridge::localSubscribe(const QString & topic, QoS maxQoS, bool noLocal, bool retainAsPublished, SubscribeOptions::RetainHandling retainHandling, quint32 subscriptionId)
{
    m_local_connection.subscribe(topic, maxQoS, noLocal, retainAsPublished, retainHandling, subscriptionId);
}

void Bridge::remoteSubscribe(const QString & topic, QoS maxQoS, bool noLocal, bool retainAsPublished, SubscribeOptions::RetainHandling retainHandling, quint32 subscriptionId)
{
    m_remote_connection.subscribe(topic, maxQoS, noLocal, retainAsPublished, retainHandling, subscriptionId);
}

bool Bridge::handleLocalControlPacket(PacketType type, const QByteArray & data)
{
    if (m_local_connection.protocolVersion() == m_remote_connection.protocolVersion())
        m_remote_queue.enqueue(data);
    else
        m_remote_queue.enqueue(convertPacket(type, data, m_local_connection.protocolVersion(), m_remote_connection.protocolVersion()));

    sendRemoteMessageQueue();
    return (m_remote_connection.state() > Connection::State::BrokerDisconnetced);
}

bool Bridge::handleRemoteControlPacket(PacketType type, const QByteArray & data)
{
    if (m_local_connection.protocolVersion() == m_remote_connection.protocolVersion())
        m_local_queue.enqueue(data);
    else
        m_local_queue.enqueue(convertPacket(type, data, m_remote_connection.protocolVersion(), m_local_connection.protocolVersion()));

    sendLocalMessageQueue();
    return (m_local_connection.state() > Connection::State::BrokerDisconnetced);
}

void Bridge::sendLocalMessageQueue()
{
    if (m_local_connection.state() == Connection::State::BrokerConnected)
        while (!m_local_queue.isEmpty())
            m_local_connection.write(m_local_queue.dequeue());
}

void Bridge::sendRemoteMessageQueue()
{
    if (m_remote_connection.state() == Connection::State::BrokerConnected)
        while (!m_remote_queue.isEmpty())
            m_remote_connection.write(m_remote_queue.dequeue());
}

QByteArray Bridge::convertPacket(PacketType type, const QByteArray & data, Version fromVersion, Version toVersion)
{
    switch (type)
    {
        case PacketType::PUBLISH:
        {
            PublishPacket p;
            p.unserialize(data, fromVersion);
            return p.serialize(toVersion);
        }
        case PacketType::PUBACK:
        {
            PublishAckPacket p;
            p.unserialize(data, fromVersion);
            return p.serialize(toVersion, Constants::DefaultMaxPacketSize);
        }
        case PacketType::PUBREC:
        {
            PublishRecPacket p;
            p.unserialize(data, fromVersion);
            return p.serialize(toVersion, Constants::DefaultMaxPacketSize);
        }
        case PacketType::PUBREL:
        {
            PublishRelPacket p;
            p.unserialize(data, fromVersion);
            return p.serialize(toVersion, Constants::DefaultMaxPacketSize);
        }
        case PacketType::PUBCOMP:
        {
            PublishCompPacket p;
            p.unserialize(data, fromVersion);
            return p.serialize(toVersion, Constants::DefaultMaxPacketSize);
        }
    }

    return data;
}
