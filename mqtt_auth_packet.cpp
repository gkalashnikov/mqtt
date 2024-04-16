#include "mqtt_auth_packet.h"
#include "mqtt_encoder.h"
#include "mqtt_decoder.h"

using namespace Mqtt;

AuthPacket::Header::Header()
    :reasonCode(ReasonCodeV5::Success)
{

}

AuthPacket::AuthPacket()
    :ControlPacket({ Flags::AUTH, PacketType::AUTH })
{

}

AuthPacket::~AuthPacket()
{

}

QByteArray AuthPacket::serialize(Version protocolVersion, qint32 maxPacketSize) const
{
    switch (protocolVersion)
    {
        case Version::Ver_5_0:
        {
            QByteArray packet;
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
            return packet;
        }

        default: break;
    }

    return QByteArray();
}

bool AuthPacket::unserialize(const QByteArray & data, Version protocolVersion)
{
    Q_UNUSED(protocolVersion)

    m_header = Header();

    size_t len = 0;
    qint64 remaining_length = ControlPacket::unserialize(data, &len);

    if (remaining_length == -1)
        return false;

    const quint8 * buf = reinterpret_cast<const quint8*>(data.constData() + len);

    switch (m_headerFix.packetType)
    {
        case PacketType::AUTH:
        {
            if (Mqtt::Flags::AUTH != m_headerFix.flags) {
                m_unserializeReasonCode = ReasonCodeV5::MalformedPacket;
                m_unserializeReasonString = QStringLiteral("malformed packet: AUTH's flags illegal");
                return false;
            }

            memcpy(&m_header.reasonCode, buf, 1);
            ++buf; --remaining_length;

            bool ok = unserializeProperties(buf, &remaining_length, &len, m_header.props); buf += len;
            if ( ok) { ok = isPropertiesValid(m_header.props); }
            if (!ok) {
                m_unserializeReasonCode = ReasonCodeV5::ProtocolError;
                m_unserializeReasonString = QStringLiteral("protocol error: invalid auth properties");
                return false;
            }

            return true;
        }

        default: break;
    }

    m_unserializeReasonCode = ReasonCodeV5::MalformedPacket;
    m_unserializeReasonString = QStringLiteral("malformed packet: this is not rigth auth packet");
    return false;
}

bool AuthPacket::isPropertiesValid(Properties & props)
{
    int auth_method_count = 0
       ,auth_data_count = 0
       ,reason_string_count = 0;

    for (int i = 0; i < props.count(); ++i)
    {
        switch (props[i].first)
        {
            case PropertyId::AuthentificationMethod:
            {
                ++auth_method_count;
                if (auth_method_count > 1)
                    return false;
                break;
            }
            case PropertyId::AuthentificationData:
            {
                ++auth_data_count;
                if (auth_data_count > 1)
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

            case PropertyId::UserProperty: break;

            default:                return false;
        }
    }

    return true;
}


