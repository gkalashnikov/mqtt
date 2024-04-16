#include "mqtt_publish_packet.h"
#include "mqtt_decoder.h"
#include "mqtt_encoder.h"
#include "mqtt_special_symbols.h"
#include "mqtt_constants.h"

struct PublishFlags
{
    quint8 retain   : 1;
    quint8 qos      : 2;
    quint8 dup      : 1;
    quint8 unused   : 4;
};

using namespace Mqtt;

PublishAckPacket::PublishAckPacket()
    :PublishAnswerPacket({ Mqtt::Flags::PUBACK, Mqtt::PacketType::PUBACK }) { }

PublishAckPacket::~PublishAckPacket() { }

PublishRecPacket::PublishRecPacket()
    :PublishAnswerPacket({ Mqtt::Flags::PUBREC, Mqtt::PacketType::PUBREC }) { }

PublishRecPacket::~PublishRecPacket() { }

PublishRelPacket::PublishRelPacket()
    :PublishAnswerPacket({ Mqtt::Flags::PUBREL, Mqtt::PacketType::PUBREL }) { }

PublishRelPacket::~PublishRelPacket() { }

PublishCompPacket::PublishCompPacket()
    :PublishAnswerPacket({ Mqtt::Flags::PUBCOMP, Mqtt::PacketType::PUBCOMP }) { }

PublishCompPacket::~PublishCompPacket() { }


PublishPacket::Header::Header()
    :packetId(0x1234)
{

}


PublishPacket::PublishPacket()
    :ControlPacket({ Mqtt::Flags::PUBLISH, Mqtt::PacketType::PUBLISH })
{

}

PublishPacket::~PublishPacket()
{

}

bool PublishPacket::isDuplicate() const
{
    return reinterpret_cast<const PublishFlags*>(&m_headerFix)->dup;
}

void PublishPacket::setDuplicate(bool dup)
{
    reinterpret_cast<PublishFlags*>(&m_headerFix)->dup = dup;
}

Mqtt::QoS PublishPacket::QoS() const
{
    return Mqtt::QoS(reinterpret_cast<const PublishFlags*>(&m_headerFix)->qos);
}

void PublishPacket::setQoS(Mqtt::QoS qos)
{
    reinterpret_cast<PublishFlags*>(&m_headerFix)->qos = static_cast<quint8>(qos);
}

bool PublishPacket::isRetained() const
{
    return reinterpret_cast<const PublishFlags*>(&m_headerFix)->retain;
}

void PublishPacket::setRetain(bool retain)
{
    reinterpret_cast<PublishFlags*>(&m_headerFix)->retain = retain;
}

QByteArray PublishPacket::serialize(Version protocolVersion) const
{
    QByteArray packet;

    switch (protocolVersion)
    {
        case Version::Ver_3_1:
        case Version::Ver_3_1_1:
        {
            QByteArray topic = Encoder::encodeUTF8(m_header.topicName);

            qint32 rlen = topic.length();
            if (QoS() != Mqtt::QoS::Value_0)
                rlen += 2; // packet id
            rlen += m_payload.length();

            QByteArray remainlen = Encoder::encodeVariableByteInteger(rlen);
            qint32 packlen = 1 + remainlen.length() + rlen;

            packet.reserve(packlen);
            packet.append(reinterpret_cast<const char *>(&m_headerFix), 1);
            packet.append(remainlen);
            packet.append(topic);
            if (QoS() != Mqtt::QoS::Value_0)
                packet.append(Encoder::encodeTwoByteInteger(m_header.packetId));
            packet.append(m_payload);

            break;
        }

        case Version::Ver_5_0:
        {
            QByteArray topic = Encoder::encodeUTF8(m_header.topicName);
            QByteArray props = serializeProperties(m_header.props);

            qint32 rlen = topic.length();
            if (QoS() != Mqtt::QoS::Value_0)
                rlen += 2; // packet id
            rlen += props.length();
            rlen += m_payload.length();

            QByteArray remainlen = Encoder::encodeVariableByteInteger(rlen);
            qint32 packlen = 1 + remainlen.length() + rlen;

            packet.reserve(packlen);
            packet.append(reinterpret_cast<const char *>(&m_headerFix), 1);
            packet.append(remainlen);
            packet.append(topic);
            if (QoS() != Mqtt::QoS::Value_0)
                packet.append(Encoder::encodeTwoByteInteger(m_header.packetId));
            packet.append(props);
            packet.append(m_payload);

            break;
        }

        default: break;
    }

    return packet;
}

bool PublishPacket::unserialize(const QByteArray & data, Version protocolVersion)
{
    m_header = Header();
    m_payload.clear();

    size_t len = 0;
    qint64 remaining_length = ControlPacket::unserialize(data, &len);

    if (remaining_length == -1)
        return false;

    const quint8 * buf = reinterpret_cast<const quint8*>(data.constData() + len);

    switch (m_headerFix.packetType)
    {
        case PacketType::PUBLISH:
        {
            Mqtt::QoS qos = QoS();

            if (Mqtt::QoS::Reserved == qos) {
                m_unserializeReasonCode   = ReasonCodeV5::MalformedPacket;
                m_unserializeReasonString = QStringLiteral("malformed packet: PUBLISH's QoS invalid");
                return false;
            }

            m_header.topicName = Decoder::decodeUTF8(buf, &remaining_length, &len); buf += len;

            if (m_header.topicName.contains(Mqtt::SpecialSymbols::Plus)
                    || m_header.topicName.contains(Mqtt::SpecialSymbols::Hash))
            {
                m_unserializeReasonCode = ReasonCodeV5::ProtocolError;
                m_unserializeReasonString = QStringLiteral("PUBLISH's topic must not contain wildcars characters");
                return false;
            }

            if (Mqtt::QoS::Value_1 == qos || Mqtt::QoS::Value_2 == qos) {
                m_header.packetId = Decoder::decodeTwoByteInteger(buf, &remaining_length, &len); buf += len;
            }

            if (Version::Ver_5_0 == protocolVersion) {
                bool ok = unserializeProperties(buf, &remaining_length, &len, m_header.props); buf += len;
                if ( ok) { ok = isPropertiesValid(m_header.props); }
                if (!ok) {
                    if (ReasonCodeV5::Success == m_unserializeReasonCode) {
                        m_unserializeReasonCode = ReasonCodeV5::ProtocolError;
                        m_unserializeReasonString = QStringLiteral("protocol error: invalid properties");
                    }
                    return false;
                }
            }

            m_payload = (remaining_length > 0)
                        ? QByteArray(reinterpret_cast<const char*>(buf), remaining_length)
                        : QByteArray();

            return true;
        }
        default: break;
    }

    m_unserializeReasonCode = ReasonCodeV5::MalformedPacket;
    m_unserializeReasonString = QStringLiteral("malformed packet: this is not rigth publish packet");
    return false;
}

bool PublishPacket::isPropertiesValid(Properties & props)
{
    int payload_format_indicator_count = 0
       ,message_expiry_interval_count = 0
       ,topic_alias_count = 0
       ,response_topic_count = 0
       ,correlation_data_count = 0
       ,content_type_count = 0;

    for (int i = 0; i < props.count(); ++i)
    {
        switch (props[i].first)
        {
            case PropertyId::PayloadFormatIndicator:
            {
                ++payload_format_indicator_count;
                if (payload_format_indicator_count > 1)
                    return false;
                break;
            }
            case PropertyId::MessageExpiryInterval:
            {
                ++message_expiry_interval_count;
                if (message_expiry_interval_count > 1)
                    return false;
                break;
            }
            case PropertyId::TopicAlias:
            {
                ++topic_alias_count;
                if (topic_alias_count > 1)
                    return false;
                quint16 ta = props[i].second.toUInt();
                if (ta == 0 || ta > Constants::TopicAliasMaximum) {
                    m_unserializeReasonCode = ReasonCodeV5::TopicAliasInvalid;
                    return false;
                }
                break;
            }
            case PropertyId::ResponseTopic:
            {
                ++response_topic_count;
                if (response_topic_count > 1)
                    return false;
                break;
            }
            case PropertyId::CorrelationData:
            {
                ++correlation_data_count;
                if (correlation_data_count > 1)
                    return false;
                break;
            }
            case PropertyId::SubscriptionIdentifier:
            {
                quint32 id = props[i].second.toUInt();
                if (id == 0 || id > Constants::MaxSubscriptionIdentifier)
                    return false;
                break;
            }
            case PropertyId::ContentType:
            {
                ++content_type_count;
                if (content_type_count > 1)
                    return false;
                break;
            }

            case PropertyId::UserProperty: break;

            default:                return false;
        }
    }

    return true;
}



PublishAnswerPacket::Header::Header()
    :packetId(0x1234)
    ,reasonCodeV5(ReasonCodeV5::Success)
{

}

PublishAnswerPacket::PublishAnswerPacket(FixedHeader fixedHeader)
    :ControlPacket(fixedHeader)
{

}

PublishAnswerPacket::~PublishAnswerPacket()
{

}

QByteArray PublishAnswerPacket::serialize(Version protocolVersion, qint32 maxPacketSize) const
{
    QByteArray packet;

    switch (protocolVersion)
    {
        case Version::Ver_3_1:
        case Version::Ver_3_1_1:
        {
            packet.reserve(4);
            packet.append(reinterpret_cast<const char *>(&m_headerFix), 1);
            packet.append(Encoder::encodeVariableByteInteger(2));
            packet.append(Encoder::encodeTwoByteInteger(m_header.packetId));
            break;
        }
        case Version::Ver_5_0:
        {
            for (int i = 0; i < 2; ++i)
            {
                QByteArray props     = serializeProperties(m_header.props);
                QByteArray remainlen = Encoder::encodeVariableByteInteger(props.length() + 3);
                qint32 packlen       = remainlen.length() + props.length() + 4;
                if (packlen > maxPacketSize && i == 0) {
                    ControlPacket::removeProperty(m_header.props, PropertyId::ReasonString);
                    ControlPacket::removeProperty(m_header.props, PropertyId::UserProperty);
                    continue;
                }
                packet.reserve(packlen);
                packet.append(reinterpret_cast<const char *>(&m_headerFix), 1);
                packet.append(remainlen);
                packet.append(Encoder::encodeTwoByteInteger(m_header.packetId));
                packet.append(reinterpret_cast<const char*>(&m_header.reasonCodeV5), 1);
                packet.append(props);
                break;
            }
        }

        default: break;
    }

    return packet;
}

bool PublishAnswerPacket::unserialize(const QByteArray & data, Version protocolVersion)
{
    m_header = Header();

    size_t len = 0;
    qint64 remaining_length = ControlPacket::unserialize(data, &len);

    if (remaining_length == -1)
        return false;

    const quint8 * buf = reinterpret_cast<const quint8*>(data.constData() + len);

    switch (m_headerFix.packetType)
    {
        case PacketType::PUBACK:
        case PacketType::PUBREC:
        case PacketType::PUBREL:
        case PacketType::PUBCOMP:
        {
            m_header.packetId = Decoder::decodeTwoByteInteger(buf, &remaining_length, &len);   buf += len;
            if (Version::Ver_5_0 == protocolVersion) {
                m_header.reasonCodeV5 = static_cast<ReasonCodeV5>(buf[0]);
                ++buf; --remaining_length;
                bool ok = unserializeProperties(buf, &remaining_length, &len, m_header.props); buf += len;
                if ( ok) { ok = isPropertiesValid(m_header.props); }
                if (!ok) {
                    m_unserializeReasonCode = ReasonCodeV5::ProtocolError;
                    m_unserializeReasonString = QStringLiteral("protocol error: invalid properties");
                    return false;
                }
            }
            return true;
        }
        default: break;
    }

    m_unserializeReasonCode = ReasonCodeV5::MalformedPacket;
    m_unserializeReasonString = QStringLiteral("malformed packet: this is not rigth pub answer packet");
    return false;
}

bool PublishAnswerPacket::isPropertiesValid(Properties & props) const
{
    int reason_string_count = 0;

    for (int i = 0; i < props.count(); ++i)
    {
        switch (props[i].first)
        {
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
