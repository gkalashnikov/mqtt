#ifndef MQTT_DISCONNECT_PACKET_H
#define MQTT_DISCONNECT_PACKET_H

#include "mqtt_control_packet.h"

namespace Mqtt
{
    class DisconnectPacket : public ControlPacket
    {
    public:
        class Header
        {
        public:
            ReasonCodeV5 reasonCode;
            Properties   props;
        };

    public:
        DisconnectPacket();
        ~DisconnectPacket() override;

    public:
        ReasonCodeV5 reasonCode() const;
        void setReasonCode(ReasonCodeV5 code);
        Properties & properties();

    public:
        QByteArray serialize(Version protocolVersion, qint32 maxPacketSize) const;
        bool unserialize(const QByteArray & data, Version protocolVersion);

    protected:
        bool isPropertiesValid(Properties & props);

    private:
        mutable Header m_header;
    };

    inline void DisconnectPacket::setReasonCode(ReasonCodeV5 code) { m_header.reasonCode = code; }
    inline ReasonCodeV5 DisconnectPacket::reasonCode() const       { return m_header.reasonCode; }
    inline Properties & DisconnectPacket::properties()             { return m_header.props;      }
}

#endif // MQTT_DISCONNECT_PACKET_H
