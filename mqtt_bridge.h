#ifndef MQTT_BRIDGE_H
#define MQTT_BRIDGE_H

#include "mqtt_connection.h"

namespace Mqtt
{
    class Bridge : public QObject
    {
        Q_OBJECT
    public:
        explicit Bridge(QObject * socketController, QObject * parent = Q_NULLPTR);
        ~Bridge();

    public:
        enum class Step : quint8
        {
             FirstOpenLocalConnection = 0
            ,OpenRemoteConnection
            ,OpenLocalConnection
            ,OpenConnectionDone
        };

    private slots:
        void localStateChanged(Connection::State state);
        void remoteStateChanged(Connection::State state);

    public:
        void connectToHosts();

        void setLocalHostAddress(const QString & address);
        void setLocalHostPort(quint16 port);
        quint32 localConnectTimeout();
        void setLocalConnectTimeout(quint32 seconds);
        void setLocalConnectionType(Network::ConnectionType type);
        void setLocalSecureMode(Network::SecureMode mode);
        const QString & localClientId() const;
        void setLocalClientId(const QString & clientId);
        const QString & localUsername() const;
        void setLocalUsername(const QString & username);
        const QString & localPassword() const;
        void setLocalPassword(const QString & password);
        bool localWillEnabled() const;
        QoS localWillQoS() const;
        bool localWillRetain() const;
        void setLocalWillOptions(bool enabled, QoS qos, bool retain);
        const Properties & localWillProperties() const;
        void setLocalWillProperties(const Properties & props);
        const QString & localWillTopic() const;
        void setLocalWillTopic(const QString & topic);
        const QString & localWillPayload() const;
        void setLocalWillPayload(const QString & payload);
        void localSubscribe(const QString & topic
                           ,QoS maxQoS = QoS::Value_2
                           ,bool noLocal = false
                           ,bool retainAsPublished = false
                           ,SubscribeOptions::RetainHandling retainHandling = SubscribeOptions::RetainHandling::SendAtSubscribe
                           ,quint32 subscriptionId = 0 /* zero is mean that not used in this case */);

        void setRemoteHostAddress(const QString & address);
        void setRemoteHostPort(quint16 port);
        quint32 remoteConnectTimeout();
        void setRemoteConnectTimeout(quint32 seconds);
        void setRemoteConnectionType(Network::ConnectionType type);
        void setRemoteSecureMode(Network::SecureMode mode);
        const QString & remoteClientId() const;
        void setRemoteClientId(const QString & clientId);
        const QString & remoteUsername() const;
        void setRemoteUsername(const QString & username);
        const QString & remotePassword() const;
        void setRemotePassword(const QString & password);
        bool remoteWillEnabled() const;
        QoS remoteWillQoS() const;
        bool remoteWillRetain() const;
        void setRemoteWillOptions(bool enabled, QoS qos, bool retain);
        const Properties & remoteWillProperties() const;
        void setRemoteWillProperties(const Properties & props);
        const QString & remoteWillTopic() const;
        void setRemoteWillTopic(const QString & topic);
        const QString & remoteWillPayload() const;
        void setRemoteWillPayload(const QString & payload);
        void remoteSubscribe(const QString & topic
                            ,QoS maxQoS = QoS::Value_2
                            ,bool noLocal = false
                            ,bool retainAsPublished = false
                            ,SubscribeOptions::RetainHandling retainHandling = SubscribeOptions::RetainHandling::SendAtSubscribe
                            ,quint32 subscriptionId = 0 /* zero is mean that not used in this case */);

        bool cleanStart() const;
        void setCleanStart(bool cleanStart);
        quint32 reconnectPeriod() const;
        void setReconnectPeriod(quint32 seconds);

    private:
        class Handler : public Connection::IPacketHandler
        {
        public:
            enum class Type : quint8
            {
                 Local = 1
                ,Remote
            };

        public:
            Handler(Type type, Bridge * bridge);
            ~Handler() override;

        public:
            bool handleControlPacket(PacketType type, const QByteArray &data) override;
            void oneSecondTimerEvent() override;

        private:
            Type type;
            Bridge * bridge;
        };

    private:
        bool handleLocalControlPacket(PacketType type, const QByteArray & data);
        bool handleRemoteControlPacket(PacketType type, const QByteArray & data);

        void sendLocalMessageQueue();
        void sendRemoteMessageQueue();

        QByteArray convertPacket(PacketType type, const QByteArray & data, Version fromVersion, Version toVersion);

    private:
        Step       m_step;
        Handler    m_local_handler;
        Connection m_local_connection;
        Handler    m_remote_handler;
        Connection m_remote_connection;

        QQueue<QByteArray> m_local_queue;
        QQueue<QByteArray> m_remote_queue;

    private:
        friend class Handler;
    };

    typedef QSharedPointer<Bridge> BridgePtr;
    typedef QWeakPointer<Bridge> BridgeWPtr;

    inline void Bridge::setLocalHostAddress(const QString & address)             { m_local_connection.setHostAddress(address);               }
    inline void Bridge::setLocalHostPort(quint16 port)                           { m_local_connection.setHostPort(port);                     }
    inline quint32 Bridge::localConnectTimeout()                                 { return m_local_connection.connectTimeout();               }
    inline void Bridge::setLocalConnectTimeout(quint32 seconds)                  { m_local_connection.setConnectTimeout(seconds);            }
    inline void Bridge::setLocalConnectionType(Network::ConnectionType type)     { m_local_connection.setConnectionType(type);               }
    inline void Bridge::setLocalSecureMode(Network::SecureMode mode)             { m_local_connection.setSecureMode(mode);                   }
    inline const QString & Bridge::localClientId() const                         { return m_local_connection.clientId();                     }
    inline void Bridge::setLocalClientId(const QString & clientId)               { m_local_connection.setClientId(clientId);                 }
    inline const QString & Bridge::localUsername() const                         { return m_local_connection.username();                     }
    inline void Bridge::setLocalUsername(const QString & username)               { m_local_connection.setUsername(username);                 }
    inline const QString & Bridge::localPassword() const                         { return m_local_connection.password();                     }
    inline void Bridge::setLocalPassword(const QString & password)               { m_local_connection.setPassword(password);                 }
    inline bool Bridge::localWillEnabled() const                                 { return m_local_connection.willEnabled();                  }
    inline QoS Bridge::localWillQoS() const                                      { return m_local_connection.willQoS();                      }
    inline bool Bridge::localWillRetain() const                                  { return m_local_connection.willRetain();                   }
    inline void Bridge::setLocalWillOptions(bool enabled, QoS qos, bool retain)  { m_local_connection.setWillOptions(enabled, qos, retain);  }
    inline const Properties & Bridge::localWillProperties() const                { return m_local_connection.willProperties();               }
    inline void Bridge::setLocalWillProperties(const Properties & props)         { m_local_connection.setWillProperties(props);              }
    inline const QString & Bridge::localWillTopic() const                        { return m_local_connection.willTopic();                    }
    inline void Bridge::setLocalWillTopic(const QString & topic)                 { m_local_connection.setWillTopic(topic);                   }
    inline const QString & Bridge::localWillPayload() const                      { return m_local_connection.willPayload();                  }
    inline void Bridge::setLocalWillPayload(const QString & payload)             { m_local_connection.setWillPayload(payload);               }

    inline void Bridge::setRemoteHostAddress(const QString & address)            { m_remote_connection.setHostAddress(address);              }
    inline void Bridge::setRemoteHostPort(quint16 port)                          { m_remote_connection.setHostPort(port);                    }
    inline quint32 Bridge::remoteConnectTimeout()                                { return m_remote_connection.connectTimeout();              }
    inline void Bridge::setRemoteConnectTimeout(quint32 seconds)                 { m_remote_connection.setConnectTimeout(seconds);           }
    inline void Bridge::setRemoteConnectionType(Network::ConnectionType type)    { m_remote_connection.setConnectionType(type);              }
    inline void Bridge::setRemoteSecureMode(Network::SecureMode mode)            { m_remote_connection.setSecureMode(mode);                  }
    inline const QString & Bridge::remoteClientId() const                        { return m_remote_connection.clientId();                    }
    inline void Bridge::setRemoteClientId(const QString & clientId)              { m_remote_connection.setClientId(clientId);                }
    inline const QString & Bridge::remoteUsername() const                        { return m_remote_connection.username();                    }
    inline void Bridge::setRemoteUsername(const QString & username)              { m_remote_connection.setUsername(username);                }
    inline const QString & Bridge::remotePassword() const                        { return m_remote_connection.password();                    }
    inline void Bridge::setRemotePassword(const QString & password)              { m_remote_connection.setPassword(password);                }
    inline bool Bridge::remoteWillEnabled() const                                { return m_remote_connection.willEnabled();                 }
    inline QoS Bridge::remoteWillQoS() const                                     { return m_remote_connection.willQoS();                     }
    inline bool Bridge::remoteWillRetain() const                                 { return m_remote_connection.willRetain();                  }
    inline void Bridge::setRemoteWillOptions(bool enabled, QoS qos, bool retain) { m_remote_connection.setWillOptions(enabled, qos, retain); }
    inline const Properties & Bridge::remoteWillProperties() const               { return m_remote_connection.willProperties();              }
    inline void Bridge::setRemoteWillProperties(const Properties & props)        { m_remote_connection.setWillProperties(props);             }
    inline const QString & Bridge::remoteWillTopic() const                       { return m_remote_connection.willTopic();                   }
    inline void Bridge::setRemoteWillTopic(const QString & topic)                { m_remote_connection.setWillTopic(topic);                  }
    inline const QString & Bridge::remoteWillPayload() const                     { return m_remote_connection.willPayload();                 }
    inline void Bridge::setRemoteWillPayload(const QString & payload)            { m_remote_connection.setWillPayload(payload);              }

    inline bool Bridge::cleanStart() const                                       { return m_local_connection.cleanStart();                   }
    inline void Bridge::setCleanStart(bool cleanStart)                           { m_local_connection.setCleanStart(cleanStart);
                                                                                   m_remote_connection.setCleanStart(cleanStart);            }
    inline quint32 Bridge::reconnectPeriod() const                               { return m_remote_connection.reconnectPeriod();             }
    inline void Bridge::setReconnectPeriod(quint32 seconds)                      { m_local_connection.setReconnectPeriod(0);
                                                                                   m_remote_connection.setReconnectPeriod(seconds);          }
}

#endif // MQTT_BRIDGE_H
