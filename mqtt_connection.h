#ifndef MQTT_CONNECTION_H
#define MQTT_CONNECTION_H

#include "network_client.h"
#include "network_event.h"
#include "mqtt_connect_packet.h"
#include "mqtt_subscribe_packet.h"
#include "mqtt_chunk_data_controller.h"
#include "mqtt_packet_identifier_controller.h"

#include <QObject>
#include <QQueue>

namespace Mqtt
{
    class Connection : public QObject
    {
        Q_OBJECT
    public:
        class IPacketHandler;
        Connection(QObject * socketController, IPacketHandler * handler, QObject * parent = Q_NULLPTR);
        ~Connection() override;

    public:
        enum class State : quint8
        {
             Unknown = 0
            ,NetworkDisconnected
            ,NetworkDisconnecting
            ,NetworkConnecting
            ,NetworkConnected
            ,BrokerDisconnetced
            ,BrokerConnecting
            ,BrokerConnected
        };

    public:
        class IPacketHandler
        {
        public:
            virtual ~IPacketHandler() { }
            virtual bool handleControlPacket(PacketType type, const QByteArray & data) = 0;
            virtual void oneSecondTimerEvent() = 0;
        };

    signals:
        void stateChanged(State state);

    public:
        quint32 connectTimeout();
        void setConnectTimeout(quint32 seconds);

        quint32 reconnectPeriod() const;
        void setReconnectPeriod(quint32 seconds);

        void connectToHost();
        void connectToHost(const QString & host, quint16 port);
        void setHostAddress(const QString & address);
        void setHostPort(quint16 port);

        void closeConnection(ReasonCodeV5 code, const QString & reason = QString());

        const Network::Client & networkClient() const;

        State state() const;

        void setConnectionType(Network::ConnectionType type);
        void setSecureMode(Network::SecureMode mode);

        bool cleanStart() const;
        void setCleanStart(bool cleanStart);

        const Properties & connectProperties() const;
        void setConnectProperties(const Properties & props);

        bool willEnabled() const;
        QoS willQoS() const;
        bool willRetain() const;
        void setWillOptions(bool enabled, QoS qos, bool retain);

        const Properties & willProperties() const;
        void setWillProperties(const Properties & props);

        const QString & willTopic() const;
        void setWillTopic(const QString & topic);

        const QString & willPayload() const;
        void setWillPayload(const QString & payload);

        quint16 keepAliveInterval() const;
        void setKeepAliveInterval(quint16 seconds);

        const QString & clientId() const;
        void setClientId(const QString & clientId);

        const QString & username() const;
        void setUsername(const QString & username);

        const QString & password() const;
        void setPassword(const QString & password);

        Version protocolVersion() const;
        void setProtocolVersion(Version version);

        bool isSessionPresent() const;
        qint32 sessionExpiryInterval() const;

        bool hasBrokerQuota() const;
        quint16 brokerReceiveMaximum() const;
        QoS brokerQoSMaximum() const;
        bool brokerRetainAvailable() const;
        qint32 brokerMaximumPacketSize() const;
        quint16 brokerTopicAliasMaximum();
        bool brokerWildcardSubscriptionsAvailable() const;
        bool brokerSubscriptionIdentifiersAvailable() const;
        bool brokerSharedSubscriptionsAvailable() const;
        QString brokerResponseInformation() const;

        void setPacketHandler(IPacketHandler * handler);

        ReasonCodeV5 disconnectReasonCode();
        QString disconnectReasonString();

    public:
        void subscribe(const QString & topic
                       ,QoS maxQoS = QoS::Value_2
                       ,bool noLocal = false
                       ,bool retainAsPublished = false
                       ,SubscribeOptions::RetainHandling retainHandling = SubscribeOptions::RetainHandling::SendAtSubscribe
                       ,quint32 subscriptionId = 0 /* zero is mean that not used in this case */
                       );

        void subscribe(const QString & topic
                       ,SubscribeOptions options = { QoS::Value_2, false, false, SubscribeOptions::RetainHandling::SendAtSubscribe }
                       ,quint32 subscriptionId = 0 /* zero is mean that not used in this case */ );

        void unsubscribe(const QString & topic);

    protected:
        bool event(QEvent * event) override;
        void timerEvent(QTimerEvent * event) override;

    protected:
        void oneSecondTimerEvent();
        void write(const QByteArray & data) const;
        void decreaseBrokerQuota();
        void increaseBrokerQuota();
        PacketIdController & idctrl();

    private:
        void setState(State state);

        void handleNetworkData(const QByteArray & data);
        void handleConnectionEstablished();
        void handleCloseConnection();

        void handleConnackPacket(const QByteArray & data);
        void handlePingResponsePacket(const QByteArray & data);
        void handleDisconnectPacket(const QByteArray & data);
        void handleAuthPacket(const QByteArray & data);

        void handleSubAckPacket(const QByteArray & data);
        void handleUnsubAckPacket(const QByteArray & data);

        void changeProtocolVersion();
        void scheduleReconnect();
        void scheduleReconnect(quint32 seconds);
        void executeReconnect();

        void applyConnack(ConnackPacket & connack);
        void checkConnectTimeout(qint64 secsSinceEpoch);
        void checkKeepAlive(qint64 secsSinceEpoch);

        void processServerReference(ReasonCodeV5 reasonCode, Properties & properties);

        void enqueueAllSubscribeMessages();
        void enqueueSubscribeMessage(const QString & topic, SubscribeOptions options, quint32 subscriptionId = 0);
        void enqueueUnsubscribeMessage(const QString & topic);
        void cancelSubscribeMessages();

        void scheduleCheckSubscriptionsQueue();
        void executeCheckSubscriptionsQueue();
        void checkSubscriptionsQueue();

    private:
        State           m_state;
        Network::Client m_client;
        QObject       * m_socketController;

        qint32          m_subscribe_timer_id;
        qint32          m_one_second_timer_id;
        quint32         m_reconnect_period;
        qint32          m_reconnect_timer_id;
        qint64          m_keepalive_time;
        quint32         m_connect_timeout;
        qint64          m_connect_abort_time;

        ConnectPacket   m_connect;
        bool            m_is_session_present;
        qint32          m_session_expiry_interval;

        quint16         m_broker_receive_maximum;
        quint16         m_broker_quota;
        QoS             m_broker_qos_maximum;
        bool            m_broker_retain_available;
        qint32          m_broker_max_packet_size;
        quint16         m_broker_topic_alias_maximum;
        bool            m_broker_wildcard_available;
        bool            m_broker_subid_available;
        bool            m_broker_shared_available;
        QString         m_broker_response_information;

        ChunkDataController m_data;
        IPacketHandler    * m_handler;

        mutable PacketIdController m_idctrl;

        QList< std::tuple<QString, SubscribeOptions, quint32> > m_subscriptions;

        QQueue<SubscribePacket> m_subscribe_queue;
        QMap<quint16, SubscribePacket> m_subscribe_in_fligth;

        QQueue<UnsubscribePacket> m_unsubscribe_queue;
        QMap<quint16, UnsubscribePacket> m_unsubscribe_in_fligth;

        ReasonCodeV5 m_disconnectReasonCode;
        QString      m_disconnectReasonString;

    private:
        friend class Bridge;
    };

    typedef QSharedPointer<Connection> ConnectionPtr;
    typedef QWeakPointer<Connection> ConnectionWPtr;

    inline void Connection::setHostAddress(const QString & address)            { m_client.setAddress(address);                   }
    inline void Connection::setHostPort(quint16 port)                          { m_client.setPort(port);                         }
    inline quint32 Connection::connectTimeout()                                { return m_connect_timeout;                       }
    inline void Connection::setConnectTimeout(quint32 seconds)                 { m_connect_timeout = seconds;                    }
    inline void Connection::setProtocolVersion(Version version)                { m_connect.setProtocolVersion(version);          }
    inline quint32 Connection::reconnectPeriod() const                         { return m_reconnect_period / 1000;               }
    inline void Connection::setReconnectPeriod(quint32 seconds)                { m_reconnect_period = seconds * 1000;            }
    inline const Network::Client & Connection::networkClient() const           { return m_client;                                }
    inline Connection::State Connection::state() const                         { return m_state;                                 }
    inline void Connection::setConnectionType(Network::ConnectionType type)    { m_client.setType(type);                         }
    inline void Connection::setSecureMode(Network::SecureMode mode)            { m_client.setMode(mode);                         }
    inline bool Connection::cleanStart() const                                 { return m_connect.cleanSession();                }
    inline void Connection::setCleanStart(bool cleanStart)                     { m_connect.setCleanSession(cleanStart);          }
    inline const Properties & Connection::connectProperties() const            { return m_connect.properties();                  }
    inline void Connection::setConnectProperties(const Properties & props)     { m_connect.setProperties(props);                 }
    inline bool Connection::willEnabled() const                                { return m_connect.willEnabled();                 }
    inline QoS Connection::willQoS() const                                     { return m_connect.willQoS();                     }
    inline bool Connection::willRetain() const                                 { return m_connect.willRetain();                  }
    inline void Connection::setWillOptions(bool enabled, QoS qos, bool retain) { m_connect.setWillOptions(enabled, qos, retain); }
    inline const Properties & Connection::willProperties() const               { return m_connect.willProperties();              }
    inline void Connection::setWillProperties(const Properties & props)        { m_connect.setWillProperties(props);             }
    inline const QString & Connection::willTopic() const                       { return m_connect.willTopic();                   }
    inline void Connection::setWillTopic(const QString & topic)                { m_connect.setWillTopic(topic);                  }
    inline const QString & Connection::willPayload() const                     { return m_connect.willPayload();                 }
    inline void Connection::setWillPayload(const QString & payload)            { m_connect.setWillPayload(payload);              }
    inline quint16 Connection::keepAliveInterval() const                       { return m_connect.keepAliveInterval();           }
    inline void Connection::setKeepAliveInterval(quint16 seconds)              { m_connect.setKeepAliveInterval(seconds);        }
    inline const QString & Connection::clientId() const                        { return m_connect.clientId();                    }
    inline void Connection::setClientId(const QString & clientId)              { m_connect.setClientId(clientId);                }
    inline const QString & Connection::username() const                        { return m_connect.username();                    }
    inline void Connection::setUsername(const QString & username)              { m_connect.setUsername(username);                }
    inline const QString & Connection::password() const                        { return m_connect.password();                    }
    inline void Connection::setPassword(const QString & password)              { m_connect.setPassword(password);                }
    inline Version Connection::protocolVersion() const                         { return m_connect.protocolVersion();             }
    inline bool Connection::isSessionPresent() const                           { return m_is_session_present;                    }
    inline qint32 Connection::sessionExpiryInterval() const                    { return m_session_expiry_interval;               }
    inline bool Connection::hasBrokerQuota() const                             { return m_broker_quota > 0;                      }
    inline quint16 Connection::brokerReceiveMaximum() const                    { return m_broker_receive_maximum;                }
    inline void Connection::decreaseBrokerQuota()                              { --m_broker_quota;                               }
    inline void Connection::increaseBrokerQuota()                              { if (m_broker_quota < m_broker_receive_maximum)
                                                                                 ++m_broker_quota;                               }
    inline PacketIdController & Connection::idctrl()                           { return m_idctrl;                                }
    inline QoS Connection::brokerQoSMaximum() const                            { return m_broker_qos_maximum;                    }
    inline bool Connection::brokerRetainAvailable() const                      { return m_broker_retain_available;               }
    inline qint32 Connection::brokerMaximumPacketSize() const                  { return m_broker_max_packet_size;                }
    inline quint16 Connection::brokerTopicAliasMaximum()                       { return m_broker_topic_alias_maximum;            }
    inline bool Connection::brokerWildcardSubscriptionsAvailable() const       { return m_broker_wildcard_available;             }
    inline bool Connection::brokerSubscriptionIdentifiersAvailable() const     { return m_broker_subid_available;                }
    inline bool Connection::brokerSharedSubscriptionsAvailable() const         { return m_broker_shared_available;               }
    inline QString Connection::brokerResponseInformation() const               { return m_broker_response_information;           }

    inline ReasonCodeV5 Connection::disconnectReasonCode()                     { return m_disconnectReasonCode;                  }
    inline QString Connection::disconnectReasonString()                        { return m_disconnectReasonString;                }
}

class QDebug;
QDebug operator << (QDebug d, Mqtt::Connection::State state);
QDebug operator << (QDebug d, const Mqtt::Connection & connection);

#endif // MQTT_CONNECTION_H
