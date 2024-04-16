#include "mqtt_control_packet.h"
#include "mqtt_decoder.h"
#include "mqtt_encoder.h"
#include "mqtt_connect_packet.h"
#include "mqtt_ping_packet.h"
#include "mqtt_disconnect_packet.h"

#include <QSharedPointer>

using namespace Mqtt;

ControlPacket::ControlPacket(FixedHeader fixedHeader)
    :m_headerFix(fixedHeader)
    ,m_unserializeReasonCode(ReasonCodeV5::NoErrors)
{

}

ControlPacket::~ControlPacket()
{

}

qint64 ControlPacket::unserialize(const QByteArray & data, size_t * bytesCount)
{
    m_unserializeReasonCode = ReasonCodeV5::Success;
    m_unserializeReasonString.clear();

    m_headerFix = *(reinterpret_cast<const FixedHeader*>(data.constData()));

    qint64 remaining_length = data.length() - 1;
    remaining_length = Decoder::decodeVariableByteInteger(reinterpret_cast<const quint8 *>(data.constData() + 1), &remaining_length, bytesCount);

    ++(*bytesCount);

    if (remaining_length != (data.length() - *bytesCount)) {
        m_unserializeReasonCode = ReasonCodeV5::MalformedPacket;
        m_unserializeReasonString = QStringLiteral("malformed packet: remaining packet length is not equal packet length witout fixed header");
        return -1;
    }

    return remaining_length;
}

qint64 ControlPacket::packetFullLength(const QByteArray & data)
{
    size_t len = 0;
    const quint8 * b = reinterpret_cast<const quint8*>(data.constData()) + 1;
    qint64 remaining_length = data.length() - 1;
           remaining_length = static_cast<qint64>(Decoder::decodeVariableByteInteger(b, &remaining_length, &len));
    if (remaining_length == 0 && b[0] != 0)
        return -1;
    return (remaining_length + 1 + len);
}

QVariant ControlPacket::property(const Properties & source, PropertyId target)
{
    for (auto & p : source)
        if (p.first == target)
            return p.second;
    return QVariant();
}

void ControlPacket::setProperty(Properties & source, PropertyId target, QVariant value)
{
    for (auto & p : source) {
        if (p.first == target) {
            p.second = value;
            return;
        }
    }
    source.append(Property { target, value });
}

void ControlPacket::removeProperty(Properties & source, PropertyId target)
{
    auto it = source.begin();
    while (it != source.end()) {
        if ((*it).first == target) {
            it = source.erase(it);
            continue;
        }
        ++it;
    }
}

QByteArray ControlPacket::serializeProperties(const Properties & props)
{
    QByteArray data;
    data.reserve(props.count() * 20 + 3);

    for (int i = 0; i < props.count(); ++i)
    {
        data.append(Encoder::encodeVariableByteInteger(quint64(props[i].first)));

        switch (props[i].first) {
            // Byte
            case PropertyId::PayloadFormatIndicator:
            case PropertyId::RequestProblemInformation:
            case PropertyId::RequestResponseInformation:
            case PropertyId::MaximumQoS:
            case PropertyId::RetainAvailable:
            case PropertyId::WildcardSubscriptionAvailable:
            case PropertyId::SubscriptionIdentifierAvailable:
            case PropertyId::SharedSubscriptionAvailable:
            {
                quint8 value = props[i].second.toUInt();
                data.append(reinterpret_cast<char*>(&value), sizeof(value));
                break;
            }
                // Four Byte Integer
            case PropertyId::MessageExpiryInterval:
            case PropertyId::SessionExpiryInterval:
            case PropertyId::WillDelayInterval:
            case PropertyId::MaximumPacketSize:
            {
                data.append(Encoder::encodeFourByteInteger(props[i].second.toUInt()));
                break;
            }
                // UTF8 Encoded String
            case PropertyId::ContentType:
            case PropertyId::ResponseTopic:
            case PropertyId::AssignedClientIdentifier:
            case PropertyId::AuthentificationMethod:
            case PropertyId::ResponseInformation:
            case PropertyId::ServerReference:
            case PropertyId::ReasonString:
            {
                data.append(Encoder::encodeUTF8(props[i].second.toString()));
                break;
            }
                // Binary Data
            case PropertyId::CorrelationData:
            case PropertyId::AuthentificationData:
            {
                data.append(Encoder::encodeBinaryData(props[i].second.toByteArray()));
                break;
            }
                // Variable Byte Integer
            case PropertyId::SubscriptionIdentifier:
            {
                data.append(Encoder::encodeVariableByteInteger(props[i].second.toULongLong()));
                break;
            }
                // Two Byte Integer
            case PropertyId::ServerKeepAlive:
            case PropertyId::ReceiveMaximum:
            case PropertyId::TopicAliasMaximum:
            case PropertyId::TopicAlias:
            {
                data.append(Encoder::encodeTwoByteInteger(props[i].second.toUInt()));
                break;
            }
                // UTF8 Encoded String Pair
            case PropertyId::UserProperty:
            {
                UserProperty p = props[i].second.value<UserProperty>();
                data.append(Encoder::encodeUTF8(p.first));
                data.append(Encoder::encodeUTF8(p.second));
                break;
            }
            default:
            {
                break;
            }
        }
    }

    return data.prepend(Encoder::encodeVariableByteInteger(data.length()));
}

bool ControlPacket::unserializeProperties(const quint8 * buf, qint64 * remainingLength, size_t * bytesCount, Properties & props)
{
    *bytesCount = 0;

    size_t len = 0;
    qint64 props_len = 0;

    if (*remainingLength > 0)
    {
        props_len = Decoder::decodeVariableByteInteger(buf, remainingLength, &len);
        *remainingLength -= props_len;
    }

    if (*remainingLength < 0)
        return false;

    buf += len;
    *bytesCount += len;
    *bytesCount += props_len;

    while (props_len > 0)
    {
        PropertyId id = static_cast<PropertyId>(Decoder::decodeVariableByteInteger(buf, &props_len, &len));
        buf += len;

        switch (id)
        {
            // Byte
            case PropertyId::PayloadFormatIndicator:
            case PropertyId::RequestProblemInformation:
            case PropertyId::RequestResponseInformation:
            case PropertyId::MaximumQoS:
            case PropertyId::RetainAvailable:
            case PropertyId::WildcardSubscriptionAvailable:
            case PropertyId::SubscriptionIdentifierAvailable:
            case PropertyId::SharedSubscriptionAvailable:
            {
                props.append(Property(id, buf[0]));
                ++buf; --props_len;
                break;
            }
                // Four Byte Integer
            case PropertyId::MessageExpiryInterval:
            case PropertyId::SessionExpiryInterval:
            case PropertyId::WillDelayInterval:
            case PropertyId::MaximumPacketSize:
            {
                props.append(Property(id, Decoder::decodeInteger<quint32>(buf, &props_len, &len)));
                buf += len;
                break;
            }
                // UTF8 Encoded String
            case PropertyId::ContentType:
            case PropertyId::ResponseTopic:
            case PropertyId::AssignedClientIdentifier:
            case PropertyId::AuthentificationMethod:
            case PropertyId::ResponseInformation:
            case PropertyId::ServerReference:
            case PropertyId::ReasonString:
            {
                props.append(Property(id, Decoder::decodeUTF8(buf, &props_len, &len)));
                buf += len;
                break;
            }
                // Binary Data
            case PropertyId::CorrelationData:
            case PropertyId::AuthentificationData:
            {
                props.append(Property(id, Decoder::decodeBinaryData(buf, &props_len, &len)));
                buf += len;
                break;
            }
                // Variable Byte Integer
            case PropertyId::SubscriptionIdentifier:
            {
                props.append(Property(id, Decoder::decodeVariableByteInteger(buf, &props_len, &len)));
                buf += len;
                break;
            }
                // Two Byte Integer
            case PropertyId::ServerKeepAlive:
            case PropertyId::ReceiveMaximum:
            case PropertyId::TopicAliasMaximum:
            case PropertyId::TopicAlias:
            {
                props.append(Property(id, Decoder::decodeInteger<quint16>(buf, &props_len, &len)));
                buf += len;
                break;
            }
                // UTF8 Encoded String Pair
            case PropertyId::UserProperty:
            {
                QString property_name = Decoder::decodeUTF8(buf,  &props_len, &len); buf += len;
                QString property_value = Decoder::decodeUTF8(buf, &props_len, &len); buf += len;
                props.append(Property(id, QVariant::fromValue<UserProperty>(UserProperty(property_name, property_value))));
                break;
            }
            default:
            {
                return false;
            }
        }
    }

    return true;
}
