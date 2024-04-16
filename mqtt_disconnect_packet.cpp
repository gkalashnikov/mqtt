#include "mqtt_disconnect_packet.h"
#include "mqtt_decoder.h"
#include "mqtt_encoder.h"

using namespace Mqtt;

DisconnectPacket::DisconnectPacket()
    :ControlPacket({ Mqtt::Flags::DISCONNECT, Mqtt::PacketType::DISCONNECT })
{

}

DisconnectPacket::~DisconnectPacket()
{

}

QByteArray DisconnectPacket::serialize(Version protocolVersion, qint32 maxPacketSize) const
{
    QByteArray packet;

    switch (protocolVersion)
    {
        case Version::Ver_3_1:
        case Version::Ver_3_1_1:
        {
            packet.resize(2);
            memcpy(packet.data(), &m_headerFix, 1);
            memset(packet.data() + 1, 0, 1);
            return packet;
        }
        case Version::Ver_5_0:
        {
            for (int i = 0; i < 2; ++i)
            {
                QByteArray props     = serializeProperties(m_header.props);
                QByteArray remainlen = Encoder::encodeVariableByteInteger(props.length() + 1);
                qint32 packlen       = remainlen.length() + 2 + props.length();
                if (packlen > maxPacketSize && i == 0) {
                    ControlPacket::removeProperty(m_header.props, PropertyId::ReasonString);
                    ControlPacket::removeProperty(m_header.props, PropertyId::UserProperty);
                    continue;
                }
                packet.reserve(packlen);
                packet.append(reinterpret_cast<const char *>(&m_headerFix), 1);
                packet.append(remainlen);
                packet.append(reinterpret_cast<const char*>(&m_header.reasonCode), 1);
                packet.append(props);
                break;
            }
            break;
        }
        default: break;
    }

    return packet;
}

bool DisconnectPacket::unserialize(const QByteArray & data, Version protocolVersion)
{
    m_header = Header();

    size_t len = 0;
    qint64 remaining_length = ControlPacket::unserialize(data, &len);

    if (remaining_length == -1)
        return false;

    const quint8 * buf = reinterpret_cast<const quint8*>(data.constData() + len);

    switch (m_headerFix.packetType)
    {
        case PacketType::DISCONNECT:
        {
            if (Mqtt::Flags::DISCONNECT != m_headerFix.flags) {
                m_unserializeReasonCode = ReasonCodeV5::MalformedPacket;
                m_unserializeReasonString = QStringLiteral("malformed packet: DISCONNECT's flags illegal");
                return false;
            }

            if (remaining_length == 0) {
                m_header.reasonCode = ReasonCodeV5::Success;
                return true;
            }

            memcpy(&m_header.reasonCode, buf, 1);
            ++buf; --remaining_length;

            if (Version::Ver_5_0 == protocolVersion && remaining_length > 0)
            {
                bool ok = unserializeProperties(buf, &remaining_length, &len, m_header.props); buf += len;
                if ( ok) { ok = isPropertiesValid(m_header.props); }
                if (!ok) {
                    m_unserializeReasonCode = ReasonCodeV5::ProtocolError;
                    m_unserializeReasonString = QStringLiteral("protocol error: invalid disconnect properties");
                    return false;
                }
            }

            return true;
        }
        default: break;
    }

    m_unserializeReasonCode = ReasonCodeV5::MalformedPacket;
    m_unserializeReasonString = QStringLiteral("malformed packet: this is not rigth disconnect packet");
    return false;
}

bool DisconnectPacket::isPropertiesValid(Properties & props)
{
    int session_expiry_interval_count = 0
       ,reason_string_count = 0
       ,server_reference_count = 0;

    for (int i = 0; i < props.count(); ++i)
    {
        switch (props[i].first)
        {
            case PropertyId::SessionExpiryInterval:
            {
                ++session_expiry_interval_count;
                if (session_expiry_interval_count > 1)
                    return false;
                break;
            }
            case PropertyId::ReasonString:
            {
                ++reason_string_count;
                if (reason_string_count > 1)
                    return false;
                break;
            }
            case PropertyId::ServerReference:
            {
                ++server_reference_count;
                if (server_reference_count > 1)
                    return false;
                break;
            }

            case PropertyId::UserProperty: break;

            default:                return false;

        }
    }

    return true;
}
