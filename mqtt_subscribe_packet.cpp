#include "mqtt_subscribe_packet.h"
#include "mqtt_encoder.h"
#include "mqtt_decoder.h"
#include "mqtt_special_symbols.h"

using namespace Mqtt;

SubscribePacket::SubscribePacket()
    :ControlPacket(FixedHeader { Mqtt::Flags::SUBSCRIBE, Mqtt::PacketType::SUBSCRIBE })
{

}

SubscribePacket::~SubscribePacket()
{

}

QByteArray SubscribePacket::serialize(Version protocolVersion) const
{
    QByteArray packet;
    packet.reserve(32);
    packet.append(Encoder::encodeTwoByteInteger(m_header.packetId));
    if (protocolVersion < Version::Ver_5_0)
    {
        for (int i = 0; i < m_topics.count(); ++i) {
             packet.append(Encoder::encodeUTF8(m_topics[i].first));
             SubscribeOptions opt { m_topics[i].second.maximumQoS(), false, false, SubscribeOptions::RetainHandling::SendAtSubscribe };
             packet.append(reinterpret_cast<const char*>(&opt), sizeof(opt));
        }
    }
    else
    {
        packet.append(serializeProperties(m_header.props));
        for (int i = 0; i < m_topics.count(); ++i) {
             packet.append(Encoder::encodeUTF8(m_topics[i].first));
             packet.append(reinterpret_cast<const char*>(&m_topics[i].second), sizeof(m_topics[i].second));
        }
    }
    QByteArray header_packlen = Encoder::encodeVariableByteInteger(packet.length());
    header_packlen.prepend(reinterpret_cast<const char *>(&m_headerFix), 1);
    packet.prepend(header_packlen);
    return packet;
}

bool SubscribePacket::unserialize(const QByteArray & data, Version protocolVersion)
{
    m_header = SubscribeVariableHeader();
    m_topics.clear();

    size_t len = 0;
    qint64 remaining_length = ControlPacket::unserialize(data, &len);

    if (remaining_length == -1)
        return false;

    const quint8 * buf = reinterpret_cast<const quint8*>(data.constData() + len);

    switch (m_headerFix.packetType)
    {
        case Mqtt::PacketType::SUBSCRIBE:
        {
            if (m_headerFix.flags != Mqtt::Flags::SUBSCRIBE) {
                m_unserializeReasonCode = ReasonCodeV5::MalformedPacket;
                m_unserializeReasonString = QStringLiteral("malformed packet: SUBSCRIBE's flags illegal");
                return false;
            }

            m_header.packetId = Decoder::decodeTwoByteInteger(buf, &remaining_length, &len);   buf += len;

            if (Version::Ver_5_0 == protocolVersion && remaining_length > 0)
            {
                bool ok = unserializeProperties(buf, &remaining_length, &len, m_header.props); buf += len;
                if ( ok) { ok = isPropertiesValid(m_header.props); }
                if (!ok) {
                    m_unserializeReasonCode = ReasonCodeV5::ProtocolError;
                    m_unserializeReasonString = QStringLiteral("protocol error: invalid properties");
                    return false;
                }
            }

            QList<QPair<QString, SubscribeOptions> > topics;

            while (remaining_length > 0) {
                QString topic = Decoder::decodeUTF8(buf, &remaining_length, &len); buf += len;
                if (topic.isEmpty()) {
                    m_unserializeReasonCode = ReasonCodeV5::ProtocolError;
                    m_unserializeReasonString = QStringLiteral("protocol error: invalid topic filter (is empty) in SUBSCRIBE packet");
                    return false;
                }
                SubscribeOptions opts = *reinterpret_cast<const SubscribeOptions*>(buf);
                ++buf; --remaining_length;
                if (opts.reservedValue() != 0) {
                    m_unserializeReasonCode = ReasonCodeV5::MalformedPacket;
                    m_unserializeReasonString = QStringLiteral("malformed packet: reserved flag is not zero");
                    return false;
                }
                // It is a Protocol Error to set the No Local bit to 1 on a Shared Subscription
                if (opts.noLocal() && topic.startsWith(QStringLiteral("$share/"))) {
                    m_unserializeReasonCode = ReasonCodeV5::ProtocolError;
                    m_unserializeReasonString = QStringLiteral("protocol error: NL = 1 for shared subcription");
                    return false;
                }
                topics.append(qMakePair(topic, opts));
            }

            if (topics.isEmpty()) {
                m_unserializeReasonCode = ReasonCodeV5::ProtocolError;
                m_unserializeReasonString = QStringLiteral("protocol error: invalid topic filter in SUDSCRIBE packet");
                return false;
            }

            m_topics = topics;

            return true;
        }
        default: break;
    }

    m_unserializeReasonCode = ReasonCodeV5::MalformedPacket;
    m_unserializeReasonString = QStringLiteral("malformed packet: this is not rigth subscribe packet");
    return false;
}

bool SubscribePacket::isPropertiesValid(const Properties & props)
{
    int subscription_id_count = 0;

    for (int i = 0; i < props.count(); ++i)
    {
        switch (props[i].first)
        {
            case PropertyId::SubscriptionIdentifier:
            {
                ++subscription_id_count;
                if (subscription_id_count > 1)
                    return false;
                break;
            }

            case PropertyId::UserProperty: break;

            default:                return false;
        }
    }

    return true;
}



UnsubscribePacket::UnsubscribePacket()
    :ControlPacket(FixedHeader { Mqtt::Flags::UNSUBSCRIBE, Mqtt::PacketType::UNSUBSCRIBE })
{

}

UnsubscribePacket::~UnsubscribePacket()
{

}

QByteArray UnsubscribePacket::serialize(Version protocolVersion) const
{
    QByteArray packet;
    packet.reserve(32);
    packet.append(Encoder::encodeTwoByteInteger(m_header.packetId));
    if (Version::Ver_5_0 == protocolVersion) {
        packet.append(serializeProperties(m_header.props));
    }
    for (int i = 0; i < m_topics.count(); ++i) {
        if (!m_topics[i].isEmpty())
            packet.append(Encoder::encodeUTF8(m_topics[i]));
    }
    QByteArray header_packlen = Encoder::encodeVariableByteInteger(packet.length());
    header_packlen.prepend(reinterpret_cast<const char *>(&m_headerFix), 1);
    packet.prepend(header_packlen);
    return packet;
}

bool UnsubscribePacket::unserialize(const QByteArray & data, Version protocolVersion)
{
    m_header = SubscribeVariableHeader();
    m_topics.clear();

    size_t len = 0;
    qint64 remaining_length = ControlPacket::unserialize(data, &len);

    if (remaining_length == -1)
        return false;

    const quint8 * buf = reinterpret_cast<const quint8*>(data.constData() + len);

    switch (m_headerFix.packetType)
    {
        case Mqtt::PacketType::UNSUBSCRIBE:
        {
            if (m_headerFix.flags != Mqtt::Flags::UNSUBSCRIBE) {
                m_unserializeReasonCode = ReasonCodeV5::MalformedPacket;
                m_unserializeReasonString = QStringLiteral("malformed packet: UNSUBSCRIBE's flags illegal");
                return false;
            }

            m_header.packetId = Decoder::decodeTwoByteInteger(buf, &remaining_length, &len);   buf += len;

            if (Version::Ver_5_0 == protocolVersion && remaining_length > 0)
            {
                bool ok = unserializeProperties(buf, &remaining_length, &len, m_header.props); buf += len;
                if ( ok) { ok = isPropertiesValid(m_header.props); }
                if (!ok) {
                    m_unserializeReasonCode = ReasonCodeV5::ProtocolError;
                    m_unserializeReasonString = QStringLiteral("protocol error: invalid properties");
                    return false;
                }
            }

            QStringList topics;

            while (remaining_length > 0) {
                topics << Decoder::decodeUTF8(buf, &remaining_length, &len); buf += len;
            }

            m_topics = topics;

            if (topics.isEmpty()) {
                m_unserializeReasonCode = ReasonCodeV5::ProtocolError;
                m_unserializeReasonString = QStringLiteral("protocol error: invalid topic's list");
                return false;
            }

            return true;
        }
        default: break;
    }

    m_unserializeReasonCode = ReasonCodeV5::MalformedPacket;
    m_unserializeReasonString = QStringLiteral("malformed packet: this is not rigth unsubscribe packet");
    return false;
}

bool UnsubscribePacket::isPropertiesValid(const Properties & props)
{
    for (int i = 0; i < props.count(); ++i)
    {
        switch (props[i].first)
        {
            case PropertyId::UserProperty: break;

            default:                return false;
        }
    }

    return true;
}




SubscribeAckPacket::SubscribeAckPacket(FixedHeader fixedHeader)
    :ControlPacket(fixedHeader)
{

}

SubscribeAckPacket::SubscribeAckPacket()
    :ControlPacket(FixedHeader { Mqtt::Flags::SUBACK, Mqtt::PacketType::SUBACK })
{

}

SubscribeAckPacket::~SubscribeAckPacket()
{

}

QByteArray SubscribeAckPacket::serialize(Version protocolVersion, qint32 maxPacketSize) const
{
    QByteArray packet;

    switch (protocolVersion)
    {
        case Version::Ver_3_1:
        case Version::Ver_3_1_1:
        {
            switch (m_headerFix.packetType)
            {
                case PacketType::SUBACK:
                {
                    QByteArray remainlen = Encoder::encodeVariableByteInteger(2 + m_returnCodes.count());
                    packet.reserve(1 + remainlen.length() + 2 + m_returnCodes.count());
                    packet.append(reinterpret_cast<const char *>(&m_headerFix), 1);
                    packet.append(remainlen);
                    packet.append(Encoder::encodeTwoByteInteger(m_header.packetId));
                    for (int i = 0; i < m_returnCodes.count(); ++i) {
                        ReasonCodeV5 code = m_returnCodes[i];
                        if (ReasonCodeV5::UnspecifiedError < code)
                            code = ReasonCodeV5::UnspecifiedError;
                        packet.append(reinterpret_cast<const char*>(&code), 1);
                    }
                    break;
                }
                case PacketType::UNSUBACK:
                {   // V3 The UNSUBACK Packet has no payload.
                    packet.reserve(4);
                    packet.append(reinterpret_cast<const char *>(&m_headerFix), 1);
                    packet.append(Encoder::encodeVariableByteInteger(2));
                    packet.append(Encoder::encodeTwoByteInteger(m_header.packetId));
                    break;
                }
            }
            break;
        }
        case Version::Ver_5_0:
        {
            for (int i = 0; i < 2; ++i)
            {
                QByteArray props     = serializeProperties(m_header.props);
                QByteArray remainlen = Encoder::encodeVariableByteInteger(2 + props.length() + m_returnCodes.count());
                qint32 packlen       = 3 + remainlen.length() + props.length() + m_returnCodes.count();
                if (packlen > maxPacketSize && i == 0) {
                    ControlPacket::removeProperty(m_header.props, PropertyId::ReasonString);
                    ControlPacket::removeProperty(m_header.props, PropertyId::UserProperty);
                    continue;
                }
                packet.reserve(packlen);
                packet.append(reinterpret_cast<const char *>(&m_headerFix), 1);
                packet.append(remainlen);
                packet.append(Encoder::encodeTwoByteInteger(m_header.packetId));
                packet.append(props);
                for (int i = 0; i < m_returnCodes.count(); ++i)
                    packet.append(reinterpret_cast<const char*>(&m_returnCodes[i]), 1);
                break;
            }
            break;
        }
        default: break;
    }

    return packet;
}

bool SubscribeAckPacket::unserialize(const QByteArray & data, Version protocolVersion)
{
    m_header = SubscribeVariableHeader();
    m_returnCodes.clear();

    size_t len = 0;
    qint64 remaining_length = ControlPacket::unserialize(data, &len);

    if (remaining_length == -1)
        return false;

    const quint8 * buf = reinterpret_cast<const quint8*>(data.constData() + len);

    switch (m_headerFix.packetType)
    {
        case Mqtt::PacketType::SUBACK:
        {
            if (m_headerFix.flags != Mqtt::Flags::SUBACK) {
                m_unserializeReasonCode = ReasonCodeV5::MalformedPacket;
                m_unserializeReasonString = QStringLiteral("malformed packet: SUBACK's flags illegal");
                return false;
            }
            break;
        }
        case Mqtt::PacketType::UNSUBACK:
        {
            if (m_headerFix.flags != Mqtt::Flags::UNSUBACK) {
                m_unserializeReasonCode = ReasonCodeV5::MalformedPacket;
                m_unserializeReasonString = QStringLiteral("malformed packet: UNSUBACK's flags illegal");
                return false;
            }
            break;
        }
        default:
        {
            m_unserializeReasonCode = ReasonCodeV5::MalformedPacket;
            m_unserializeReasonString = QStringLiteral("malformed packet: this is not rigth suback/unsuback packet");
            return false;
        }
    }

    m_header.packetId = Decoder::decodeTwoByteInteger(buf, &remaining_length, &len); buf += len;

    if (Version::Ver_5_0 == protocolVersion && remaining_length > 0)
    {
        bool ok = unserializeProperties(buf, &remaining_length, &len, m_header.props); buf += len;
        if ( ok) { ok = isPropertiesValid(m_header.props); }
        if (!ok) {
            m_unserializeReasonCode = ReasonCodeV5::ProtocolError;
            m_unserializeReasonString = QStringLiteral("protocol error: invalid properties");
            return false;
        }
    }

    if (Version::Ver_5_0 == protocolVersion || PacketType::SUBACK == m_headerFix.packetType)
    {
        m_returnCodes.reserve(remaining_length);
        while (remaining_length > 0) {
            m_returnCodes.append(*reinterpret_cast<const ReasonCodeV5*>(buf));
            ++buf; --remaining_length;
        }
    }

    return true;
}

bool SubscribeAckPacket::isPropertiesValid(const Properties & props)
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


UnsubscribeAckPacket::UnsubscribeAckPacket()
    :SubscribeAckPacket(FixedHeader { Mqtt::Flags::UNSUBACK, Mqtt::PacketType::UNSUBACK })
{

}

UnsubscribeAckPacket::~UnsubscribeAckPacket()
{

}
