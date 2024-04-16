#ifndef MQTT_PING_PACKET_H
#define MQTT_PING_PACKET_H

#include "mqtt_control_packet.h"

namespace Mqtt
{
    class PingRequestPacket : public ControlPacket
    {
    public:
        PingRequestPacket();
        ~PingRequestPacket() override;

    public:
        QByteArray serialize(Version protocolVersion) const;
        bool unserialize(const QByteArray &data, Version protocolVersion);
    };

    class PingResponsePacket : public ControlPacket
    {
    public:
        PingResponsePacket();
        ~PingResponsePacket() override;

    public:
        QByteArray serialize(Version protocolVersion) const;
        bool unserialize(const QByteArray &data, Version protocolVersion);
    };
}

#endif // MQTT_PING_PACKET_H
