#ifndef MQTT_CONNECT_PACKET_H
#define MQTT_CONNECT_PACKET_H

#include "mqtt_control_packet.h"

namespace Mqtt
{
    class ConnectPacket : public ControlPacket
    {
    public:
        ConnectPacket();
        ~ConnectPacket() override;

    public:
        struct Flags
        {
            quint8  reserved   : 1;
            quint8  cleanStart : 1;
            quint8  willFlag   : 1;
            quint8  willQoS    : 2;
            quint8  willRetain : 1;
            quint8  passFlag   : 1;
            quint8  userFlag   : 1;
        };

        class Header
        {
        public:
            Header();

        public:
            QString protoName;
            Version protoVersion;
            Flags   flags;
            quint16 keepAlive;
            Properties props;
        };

        class Payload
        {
        public:
            QString    clientId;
            Properties willProps;
            QString    willTopic;
            QString    willPayload;
            QString    username;
            QString    password;
        };

    public:
        const QString & protocolName() const;
        void setProtocolName(const QString & proto);

        Version protocolVersion() const;
        void setProtocolVersion(Version v);

        bool cleanSession() const;
        void setCleanSession(bool clean);

        const Properties & properties() const;
        void setProperties(const Properties & properies);

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

        const QString & username() const;
        void setUsername(const QString & username);

        const QString & password() const;
        void setPassword(const QString & password);

        quint16 keepAliveInterval() const;
        void setKeepAliveInterval(quint16 seconds);

        const QString & clientId() const;
        void setClientId(const QString & id);

    public:
        // This packet uses setProtocolVersion() function instead first argument
        QByteArray serialize() const;
        // This packet uses setProtocolVersion() function instead second argument
        bool unserialize(const QByteArray &data);

    protected:
        bool isPropertiesValid(Properties & props);
        bool isWillPropertiesValid(Properties & props);

    private:
        Header     m_header;
        Payload    m_payload;
    };

    typedef QSharedPointer<ConnectPacket> ConnectPacketPtr;
    typedef QWeakPointer<ConnectPacket> ConnectPacketWPtr;

    inline const QString & ConnectPacket::protocolName() const                    { return m_header.protoName;          }
    inline void ConnectPacket::setProtocolName(const QString & proto)             { m_header.protoName = proto;         }
    inline Version ConnectPacket::protocolVersion() const                         { return m_header.protoVersion;       }
    inline void ConnectPacket::setProtocolVersion(Version v)                      { m_header.protoVersion = v;          }
    inline bool ConnectPacket::cleanSession() const                               { return m_header.flags.cleanStart;   }
    inline void ConnectPacket::setCleanSession(bool clean)                        { m_header.flags.cleanStart = clean;  }
    inline const Properties & ConnectPacket::properties() const                   { return m_header.props;              }
    inline void ConnectPacket::setProperties(const Properties & properies)        { m_header.props = properies;         }
    inline bool ConnectPacket::willEnabled() const                                { return m_header.flags.willFlag;     }
    inline QoS ConnectPacket::willQoS() const                                     { return static_cast<QoS>(
                                                                                          m_header.flags.willQoS);      }
    inline bool ConnectPacket::willRetain() const                                 { return m_header.flags.willRetain;   }
    inline void ConnectPacket::setWillOptions(bool enabled, QoS qos, bool retain) { m_header.flags.willFlag = enabled;
                                                                                    m_header.flags.willQoS
                                                                                            = static_cast<quint8>(qos);
                                                                                    m_header.flags.willRetain = retain; }
    inline const Properties & ConnectPacket::willProperties() const               { return m_payload.willProps;         }
    inline void ConnectPacket::setWillProperties(const Properties & props)        { m_payload.willProps = props;        }
    inline const QString & ConnectPacket::willTopic() const                       { return m_payload.willTopic;         }
    inline void ConnectPacket::setWillTopic(const QString & topic)                { m_payload.willTopic = topic;        }
    inline const QString & ConnectPacket::willPayload() const                     { return m_payload.willPayload;       }
    inline void ConnectPacket::setWillPayload(const QString & payload)            { m_payload.willPayload = payload;    }
    inline const QString & ConnectPacket::username() const                        { return m_payload.username;          }
    inline const QString & ConnectPacket::password() const                        { return m_payload.password;          }
    inline quint16 ConnectPacket::keepAliveInterval() const                       { return m_header.keepAlive;          }
    inline void ConnectPacket::setKeepAliveInterval(quint16 seconds)              { m_header.keepAlive = seconds;       }
    inline const QString & ConnectPacket::clientId() const                        { return m_payload.clientId;          }
    inline void ConnectPacket::setClientId(const QString & id)                    { m_payload.clientId = id;            }


    class ConnackPacket : public ControlPacket
    {
    public:
        ConnackPacket();
        ~ConnackPacket() override;

    public:
        struct ConnectAcknowledgeFlags
        {
            quint8 sessionPresent : 1;
            quint8 reserved : 7;
        };

        class Header
        {
        public:
            Header();

        public:
            ConnectAcknowledgeFlags flags;
            union
            {
                ReasonCodeV5 reasonCodeV5;
                ReturnCodeV3 returnCodeV3;
            };
            Properties props;
        };

    public:
        bool isSessionPresent() const;
        void setSessionPresent(bool present);

        ReasonCodeV5 reasonCodeV5() const;
        void setReasonCodeV5(ReasonCodeV5 code);

        ReturnCodeV3 returnCodeV3();
        void setReturnCodeV3(ReturnCodeV3 code);

        Properties & properties();

    public:
        QByteArray serialize(Version protocolVersion, qint32 maxPacketSize) const;
        bool unserialize(const QByteArray &data, Version protocolVersion);

    protected:
        bool isPropertiesValid(Properties & props);

    private:
        mutable Header m_header;
    };

    inline bool ConnackPacket::isSessionPresent() const           { return m_header.flags.sessionPresent;    }
    inline void ConnackPacket::setSessionPresent(bool present)    { m_header.flags.sessionPresent = present; }
    inline ReasonCodeV5 ConnackPacket::reasonCodeV5() const       { return m_header.reasonCodeV5; }
    inline void ConnackPacket::setReasonCodeV5(ReasonCodeV5 code) { m_header.reasonCodeV5 = code; }
    inline ReturnCodeV3 ConnackPacket::returnCodeV3()             { return m_header.returnCodeV3; }
    inline void ConnackPacket::setReturnCodeV3(ReturnCodeV3 code) { m_header.returnCodeV3 = code; }
    inline Properties & ConnackPacket::properties()               { return m_header.props; }
}

#endif // MQTT_CONNECT_PACKET_H
