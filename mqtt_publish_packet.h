#ifndef MQTT_PUBLISH_PACKET_H
#define MQTT_PUBLISH_PACKET_H

#include "mqtt_control_packet.h"
#include <QVariant>

namespace Mqtt
{
    class PublishPacket : public ControlPacket
    {
    public:
        PublishPacket();
        ~PublishPacket() override;

    public:
        class Header
        {
        public:
            Header();

        public:
            QString topicName;
            quint16 packetId;
            Properties props;
        };

    public:
        bool isDuplicate() const;
        void setDuplicate(bool dup);

        Mqtt::QoS QoS() const;
        void setQoS(Mqtt::QoS qos);

        bool isRetained() const;
        void setRetain(bool retain);

        QString topicName() const;
        void setTopicName(const QString & topic);

        quint16 packetId() const;
        void setPacketId(quint16 id);

        Properties & properties();

        QByteArray payload() const;
        void setPayload(const QByteArray & payload);

    public:
        QByteArray serialize(Version protocolVersion) const;
        bool unserialize(const QByteArray &data, Version protocolVersion);

    private:
        bool isPropertiesValid(Properties & props);

    private:
        Header m_header;
        QByteArray m_payload;
    };

    inline QString PublishPacket::topicName() const                   { return m_header.topicName;  }
    inline void PublishPacket::setTopicName(const QString & topic)    { m_header.topicName = topic; }
    inline quint16 PublishPacket::packetId() const                    { return m_header.packetId;   }
    inline void PublishPacket::setPacketId(quint16 id)                { m_header.packetId = id;     }
    inline Properties & PublishPacket::properties()                   { return m_header.props;      }
    inline QByteArray PublishPacket::payload() const                  { return m_payload;           }
    inline void PublishPacket::setPayload(const QByteArray & payload) { m_payload = payload;        }


    class PublishAnswerPacket : public ControlPacket
    {
    protected:
        PublishAnswerPacket(FixedHeader fixedHeader);

    public:
        ~PublishAnswerPacket() override;

    public:
        class Header
        {
        public:
            Header();

        public:
            quint16 packetId;
            ReasonCodeV5 reasonCodeV5;
            Properties props;
        };

    public:
        quint16 packetId() const;
        void setPacketId(quint16 id);

        Properties & properties();

        ReasonCodeV5 reasonCode() const;
        void setReasonCode(ReasonCodeV5 code);

    public:
        QByteArray serialize(Version protocolVersion, qint32 maxPacketSize) const;
        bool unserialize(const QByteArray &data, Version protocolVersion);

    private:
        bool isPropertiesValid(Properties & props) const;

    private:
        mutable Header m_header;
    };

    inline quint16 PublishAnswerPacket::packetId() const              { return m_header.packetId;     }
    inline void PublishAnswerPacket::setPacketId(quint16 id)          { m_header.packetId = id;       }
    inline Properties & PublishAnswerPacket::properties()             { return m_header.props;        }
    inline ReasonCodeV5 PublishAnswerPacket::reasonCode() const       { return m_header.reasonCodeV5; }
    inline void PublishAnswerPacket::setReasonCode(ReasonCodeV5 code) { m_header.reasonCodeV5 = code; }

    class PublishAckPacket : public PublishAnswerPacket
    {
    public:
        PublishAckPacket();
        ~PublishAckPacket();
    };

    class PublishRecPacket : public PublishAnswerPacket
    {
    public:
        PublishRecPacket();
        ~PublishRecPacket();
    };

    class PublishRelPacket : public PublishAnswerPacket
    {
    public:
        PublishRelPacket();
        ~PublishRelPacket();
    };

    class PublishCompPacket : public PublishAnswerPacket
    {
    public:
        PublishCompPacket();
        ~PublishCompPacket();
    };

}

#endif // MQTT_PUBLISH_PACKET_H
