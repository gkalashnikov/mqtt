#ifndef MQTT_AUTH_PACKET_H
#define MQTT_AUTH_PACKET_H

#include "mqtt_control_packet.h"

namespace Mqtt
{
    class AuthPacket : public ControlPacket
    {
    public:
        class Header
        {
        public:
            Header();

        public:
            ReasonCodeV5 reasonCode;
            Properties props;
        };

    public:
        AuthPacket();
        ~AuthPacket() override;

    public:
    public:
        ReasonCodeV5 reasonCode() const;
        void setReasonCode(ReasonCodeV5 code);
        Properties & properties();

    public:
        QByteArray serialize(Version protocolVersion, qint32 maxPacketSize) const;
        bool unserialize(const QByteArray &data, Version protocolVersion);

    protected:
        bool isPropertiesValid(Properties & props);

    private:
        mutable Header m_header;
    };

    inline void AuthPacket::setReasonCode(ReasonCodeV5 code) { m_header.reasonCode = code; }
    inline ReasonCodeV5 AuthPacket::reasonCode() const       { return m_header.reasonCode; }
    inline Properties & AuthPacket::properties()             { return m_header.props;      }
}

#endif // MQTT_AUTH_PACKET_H
