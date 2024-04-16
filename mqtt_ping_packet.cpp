#include "mqtt_ping_packet.h"
#include "mqtt_decoder.h"

using namespace Mqtt;

PingRequestPacket::PingRequestPacket()
    :ControlPacket({ Mqtt::Flags::PINGREQ , Mqtt::PacketType::PINGREQ })
{

}

PingRequestPacket::~PingRequestPacket()
{

}

QByteArray PingRequestPacket::serialize(Version protocolVersion) const
{
    Q_UNUSED(protocolVersion)

    QByteArray data; data.resize(2);
    memcpy(data.data(), &m_headerFix, 1);
    memset(data.data() + 1, 0, 1);
    return data;
}

bool PingRequestPacket::unserialize(const QByteArray & data, Version protocolVersion)
{
    Q_UNUSED(protocolVersion)

    m_unserializeReasonCode = ReasonCodeV5::Success;
    m_unserializeReasonString.clear();

    size_t len = 0;
    const quint8 * buf = reinterpret_cast<const quint8*>(data.constData());

    m_headerFix = *(reinterpret_cast<const FixedHeader*>(buf)); ++buf;
    qint64 remaining_length = data.length() - 1;
           remaining_length = Decoder::decodeVariableByteInteger(buf, &remaining_length, &len);

    if (PacketType::PINGREQ == m_headerFix.packetType && Flags::PINGREQ == m_headerFix.flags && remaining_length == 0)
        return true;

    m_unserializeReasonCode = ReasonCodeV5::MalformedPacket;
    m_unserializeReasonString = QStringLiteral("malformed packet: this is not rigth ping request packet");
    return false;
}



PingResponsePacket::PingResponsePacket()
    :ControlPacket({ Mqtt::Flags::PINGRESP , Mqtt::PacketType::PINGRESP })
{

}

PingResponsePacket::~PingResponsePacket()
{

}

QByteArray PingResponsePacket::serialize(Version protocolVersion) const
{
    Q_UNUSED(protocolVersion)

    QByteArray data; data.resize(2);
    memcpy(data.data(), &m_headerFix, 1);
    memset(data.data() + 1, 0, 1);
    return data;
}

bool PingResponsePacket::unserialize(const QByteArray & data, Version protocolVersion)
{
    Q_UNUSED(protocolVersion)

    size_t len = 0;
    qint64 remaining_length = ControlPacket::unserialize(data, &len);

    if (remaining_length == -1)
        return false;

    if (PacketType::PINGRESP == m_headerFix.packetType && Flags::PINGRESP == m_headerFix.flags && remaining_length == 0)
        return true;

    m_unserializeReasonCode = ReasonCodeV5::MalformedPacket;
    m_unserializeReasonString = QStringLiteral("malformed packet: this is not rigth ping response packet");
    return false;
}
