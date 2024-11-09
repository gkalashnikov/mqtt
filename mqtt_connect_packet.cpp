#include "mqtt_connect_packet.h"
#include "mqtt_decoder.h"
#include "mqtt_encoder.h"
#include "mqtt_constants.h"

#define MQTT_PROTO_NAME_OLD QStringLiteral("MQIsdp")
#define MQTT_PROTO_NAME QStringLiteral("MQTT")

using namespace Mqtt;

enum class Parse : quint8
{
     ProtoName
    ,ProtoVersion
    ,Flags
    ,KeepAlive
    ,Properties
    ,ClientId
    ,WillProps
    ,WillTopic
    ,WillPayload
    ,User
    ,Password
    ,End
};

ConnectPacket::Header::Header()
    :protoName(MQTT_PROTO_NAME)
    ,protoVersion(Version::Ver_5_0)
    ,flags({ 0 })
    ,keepAlive(Constants::DefaultKeepAliveInterval)
{

}

ConnectPacket::ConnectPacket()
    :ControlPacket(FixedHeader { Mqtt::Flags::CONNECT , Mqtt::PacketType::CONNECT })
{

}

ConnectPacket::~ConnectPacket()
{

}

void ConnectPacket::setUsername(const QString & username)
{
    m_payload.username = username;
    m_header.flags.userFlag = !m_payload.username.isEmpty();
}

void ConnectPacket::setPassword(const QString & password)
{
    m_payload.password = password;
    m_header.flags.passFlag = !m_payload.password.isEmpty();
}

QByteArray ConnectPacket::serialize() const
{
    QByteArray packet;
    packet.reserve(512);

    packet.append(m_header.protoVersion != Version::Ver_3_1 ? Encoder::encodeUTF8(MQTT_PROTO_NAME) : Encoder::encodeUTF8(MQTT_PROTO_NAME_OLD));

    packet.append(reinterpret_cast<const char*>(&m_header.protoVersion), sizeof(m_header.protoVersion));
    packet.append(reinterpret_cast<const char*>(&m_header.flags), sizeof(m_header.flags));
    packet.append(Encoder::encodeTwoByteInteger(m_header.keepAlive));

    if (Version::Ver_5_0 == m_header.protoVersion)
        packet.append(serializeProperties(m_header.props));

    packet.append(Encoder::encodeUTF8(m_payload.clientId));

    if (m_header.flags.willFlag == 1) {
        if (Version::Ver_5_0 == m_header.protoVersion)
            packet.append(serializeProperties(m_payload.willProps));
        packet.append(Encoder::encodeUTF8(m_payload.willTopic));
        packet.append(Encoder::encodeUTF8(m_payload.willPayload));
    }

    if (m_header.flags.userFlag == 1)
        packet.append(Encoder::encodeUTF8(m_payload.username));
    if (m_header.flags.passFlag == 1)
        packet.append(Encoder::encodeUTF8(m_payload.password));

    QByteArray header_packlen = Encoder::encodeVariableByteInteger(packet.length());
    header_packlen.prepend(reinterpret_cast<const char *>(&m_headerFix), 1);

    packet.prepend(header_packlen);

    return packet;
}

bool ConnectPacket::unserialize(const QByteArray & data)
{
    m_header  = Header();
    m_payload = Payload();

    size_t len = 0;
    qint64 remaining_length = ControlPacket::unserialize(data, &len);

    if (remaining_length == -1)
        return false;

    const quint8 * buf = reinterpret_cast<const quint8*>(data.constData() + len);

    switch (m_headerFix.packetType)
    {
        case PacketType::CONNECT:
        {
            if (m_headerFix.flags != Mqtt::Flags::CONNECT) {
                m_unserializeReasonCode = ReasonCodeV5::MalformedPacket;
                m_unserializeReasonString = QStringLiteral("malformed packet: CONNECT's flags illegal");
                return false;
            }

            Parse step = Parse::ProtoName;

            while (remaining_length > 0)
            {
                switch (step)
                {
                    case Parse::ProtoName: {
                        m_header.protoName = Decoder::decodeUTF8(buf, &remaining_length, &len); buf += len;
                        step = Parse::ProtoVersion;

                        if (m_header.protoName != MQTT_PROTO_NAME && m_header.protoName != MQTT_PROTO_NAME_OLD) {
                            m_unserializeReasonCode = ReasonCodeV5::UnsupportedProtocolVersion;
                            m_unserializeReasonString = QStringLiteral("invalid proto name");
                            return false;
                        }
                        break;
                    }
                    case Parse::ProtoVersion: {
                        m_header.protoVersion = *(reinterpret_cast<const Version*>(buf));
                        ++buf; --remaining_length;
                        step = Parse::Flags;

                        if (m_header.protoVersion < Version::Ver_3_1 || m_header.protoVersion > Version::Ver_5_0) {
                            m_unserializeReasonCode = ReasonCodeV5::UnsupportedProtocolVersion;
                            m_unserializeReasonString = QStringLiteral("invalid proto version");
                            return false;
                        }
                        break;
                    }
                    case Parse::Flags: {
                        m_header.flags = *(reinterpret_cast<const Flags*>(buf));
                        ++buf; --remaining_length;
                        step = Parse::KeepAlive;

                        if (m_header.flags.reserved != 0) {
                            m_unserializeReasonCode = ReasonCodeV5::MalformedPacket;
                            m_unserializeReasonString = QStringLiteral("malformed packet, reserved flag illegal (is not equal 0)");
                            return false;
                        }
                        if (m_header.flags.willFlag == 0) {
                            if (m_header.flags.willQoS != 0) {
                                m_unserializeReasonCode = ReasonCodeV5::MalformedPacket;
                                m_unserializeReasonString = QStringLiteral("malformed packet, will qos flag illegal (is not equal 0 when will flag is equal 0)");
                                return false;
                            }
                        } else {
                            if (m_header.flags.willQoS == 3) {
                                m_unserializeReasonCode = ReasonCodeV5::MalformedPacket;
                                m_unserializeReasonString = QStringLiteral("malformed packet, will qos flag illegal (equal 3)");
                                return false;
                            }
                        }
                        if (m_header.flags.willFlag == 0) {
                            if (m_header.flags.willRetain != 0) {
                                m_unserializeReasonCode = ReasonCodeV5::MalformedPacket;
                                m_unserializeReasonString = QStringLiteral("malformed packet, will retain flag illegal (is not equal 0 when will flag is equal 0)");
                                return false;
                            }
                        }
                        if (m_header.protoVersion < Version::Ver_5_0) {
                            bool auth_flags_ok = ((m_header.flags.userFlag == 0 && m_header.flags.passFlag == 0)
                                                  || (m_header.flags.userFlag == 1 && m_header.flags.passFlag == 1));
                            if (!auth_flags_ok) {
                                m_unserializeReasonCode = ReasonCodeV5::MalformedPacket;
                                m_unserializeReasonString = QStringLiteral("malformed packet, user and pass flags illegal use (is not equal 0 for both or is not equal 1 for both)");
                                return false;
                            }
                        }
                        break;
                    }
                    case Parse::KeepAlive: {
                        m_header.keepAlive = Decoder::decodeInteger<quint16>(buf, &remaining_length, &len); buf += len;
                        step = (Version::Ver_5_0 == m_header.protoVersion) ? Parse::Properties : Parse::ClientId;
                        break;
                    }
                    case Parse::Properties: {
                        bool ok = unserializeProperties(buf, &remaining_length, &len, m_header.props);      buf += len;
                        if ( ok) { ok = isPropertiesValid(m_header.props); }
                        if (!ok) {
                            m_unserializeReasonCode = ReasonCodeV5::ProtocolError;
                            m_unserializeReasonString = QStringLiteral("protocol error: invalid properties");
                            return false;
                        }
                        step = Parse::ClientId;
                        break;
                    }
                    // PAYLOAD in order Client Identifier, Will Properties(V 5), Will Topic, Will Message, User Name, Password
                    case Parse::ClientId: {
                        m_payload.clientId = Decoder::decodeUTF8(buf, &remaining_length, &len); buf += len;
                        step = (m_header.flags.willFlag == 1)
                               ? ((Version::Ver_5_0 == m_header.protoVersion) ? Parse::WillProps : Parse::WillTopic)
                               : Parse::User;
                        break;
                    }
                    case Parse::WillProps: {
                        bool ok = unserializeProperties(buf, &remaining_length, &len, m_payload.willProps);  buf += len;
                        if ( ok) { ok = isWillPropertiesValid(m_payload.willProps); }
                        if (!ok) {
                            m_unserializeReasonCode = ReasonCodeV5::ProtocolError;
                            m_unserializeReasonString = QStringLiteral("protocol error: invalid will properties");
                            return false;
                        }
                        step = Parse::WillTopic;
                        break;
                    }
                    case Parse::WillTopic: {
                        m_payload.willTopic = Decoder::decodeUTF8(buf, &remaining_length, &len);   buf += len;
                        step = Parse::WillPayload;
                        break;
                    }
                    case Parse::WillPayload: {
                        m_payload.willPayload = Decoder::decodeUTF8(buf, &remaining_length, &len); buf += len;
                        step = Parse::User;
                        break;
                    }
                    case Parse::User: {
                        m_payload.username = Decoder::decodeUTF8(buf, &remaining_length, &len);    buf += len;
                        step = Parse::Password;
                        break;
                    }
                    case Parse::Password: {
                        m_payload.password = Decoder::decodeUTF8(buf, &remaining_length, &len);
                        step = Parse::End;
                        break;
                    }
                    case Parse::End: {
                        remaining_length = 0;
                        break;
                    }
                }
            }
            return true;
        }
        default: break;
    }

    m_unserializeReasonCode = ReasonCodeV5::MalformedPacket;
    m_unserializeReasonString = QStringLiteral("malformed packet: this is not rigth connect packet");
    return false;
}

bool ConnectPacket::isPropertiesValid(Properties & props)
{
    int sessions_expiry_interval_count = 0
       ,receive_maximum_count = 0
       ,maximum_packet_size_count = 0
       ,topic_alias_maximum_count = 0
       ,request_response_info_count = 0
       ,request_problem_information_count = 0
       ,auth_method_count = 0
       ,auth_data_count = 0;

    for (int i = 0; i < props.count(); ++i)
    {
        switch (props[i].first)
        {
            case PropertyId::SessionExpiryInterval:
            {
                ++sessions_expiry_interval_count;
                if (sessions_expiry_interval_count > 1)
                    return false;
                break;
            }
            case PropertyId::ReceiveMaximum:
            {
                ++receive_maximum_count;
                if (receive_maximum_count > 1)
                    return false;
                if (props[i].second.toUInt() == 0)
                    return false;
                break;
            }
            case PropertyId::MaximumPacketSize:
            {
                ++maximum_packet_size_count;
                if (maximum_packet_size_count > 1)
                    return false;
                if (props[i].second.toUInt() == 0)
                    return false;
                break;
            }
            case PropertyId::TopicAliasMaximum:
            {
                ++topic_alias_maximum_count;
                if (topic_alias_maximum_count > 1)
                    return false;
                break;
            }
            case PropertyId::RequestResponseInformation:
            {
                ++request_response_info_count;
                if (request_response_info_count > 1)
                    return false;
                quint8 value = props[i].second.toUInt();
                if (value > 1)
                    return false;
                break;
            }
            case PropertyId::RequestProblemInformation:
            {
                ++request_problem_information_count;
                if (request_problem_information_count > 1)
                    return false;
                quint8 value = props[i].second.toUInt();
                if (value > 1)
                    return false;
                break;
            }
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

            case PropertyId::UserProperty:  break;

            default:                 return false;
        }
    }

    if (sessions_expiry_interval_count == 0) {
        props.append(Property(PropertyId::SessionExpiryInterval, 0));
    }

    if (receive_maximum_count == 0) {
        props.append(Property(PropertyId::ReceiveMaximum, Constants::DefaultReceiveMaximum));
    }

    if (topic_alias_maximum_count == 0) {
        props.append(Property(PropertyId::TopicAliasMaximum, 0));
    }

    if (request_response_info_count == 0) {
        props.append(Property(PropertyId::RequestResponseInformation, 0));
    }

    if (request_problem_information_count == 0) {
        props.append(Property(PropertyId::RequestProblemInformation, 1));
    }

    if (auth_data_count != 0 && auth_method_count == 0)
        return false;

    return true;
}

bool ConnectPacket::isWillPropertiesValid(Properties & props)
{
    int will_delay_interval_count = 0
       ,payload_format_indicator_count = 0
       ,message_expiry_interval_count = 0
       ,content_type_count = 0
       ,response_topic_count = 0
       ,correlation_data_count = 0;

    for (int i = 0; i < props.count(); ++i)
    {
        switch (props[i].first)
        {
            case PropertyId::WillDelayInterval:
            {
                ++will_delay_interval_count;
                if (will_delay_interval_count > 1)
                    return false;
                break;
            }
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
            case PropertyId::ContentType:
            {
                ++content_type_count;
                if (content_type_count > 1)
                    return false;
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
            default: break;
        }
    }

    return true;
}




ConnackPacket::Header::Header()
    :flags({ 0 })
{

}

ConnackPacket::ConnackPacket()
    :ControlPacket(FixedHeader { Mqtt::Flags::CONNACK, Mqtt::PacketType::CONNACK })
{

}

ConnackPacket::~ConnackPacket()
{

}

QByteArray ConnackPacket::serialize(Version protocolVersion, qint32 maxPacketSize) const
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
            packet.append(reinterpret_cast<const char*>(&m_header), 2);
            break;
        }

        case Version::Ver_5_0:
        {
            for (int i = 0; i < 2; ++i)
            {
                QByteArray props     = serializeProperties(m_header.props);
                QByteArray remainlen = Encoder::encodeVariableByteInteger(props.length() + 2);
                qint32 packlen       = props.length() + remainlen.length() + 3;
                if (packlen > maxPacketSize && i == 0) {
                    ControlPacket::removeProperty(m_header.props, PropertyId::ReasonString);
                    ControlPacket::removeProperty(m_header.props, PropertyId::UserProperty);
                    continue;
                }
                packet.reserve(packlen);
                packet.append(reinterpret_cast<const char *>(&m_headerFix), 1);
                packet.append(remainlen);
                packet.append(reinterpret_cast<const char*>(&m_header), 2);
                packet.append(props);
                break;
            }
            break;
        }

        default: break;
    }

    return packet;
}

bool ConnackPacket::unserialize(const QByteArray & data, Version protocolVersion)
{
    m_header = Header();

    size_t len = 0;
    qint64 remaining_length = ControlPacket::unserialize(data, &len);

    if (remaining_length == -1)
        return false;

    const quint8 * buf = reinterpret_cast<const quint8*>(data.constData() + len);

    switch (m_headerFix.packetType)
    {
        case PacketType::CONNACK:
        {
            if (m_headerFix.flags != Mqtt::Flags::CONNACK) {
                m_unserializeReasonCode = ReasonCodeV5::MalformedPacket;
                m_unserializeReasonString = QStringLiteral("malformed packet: CONNACK's flags illegal");
                return false;
            }

            memcpy(&m_header, buf, 2);
            buf += 2; remaining_length -= 2;

            if (Version::Ver_5_0 == protocolVersion)
            {
                bool ok = unserializeProperties(buf, &remaining_length, &len, m_header.props); buf += len;
                if ( ok) { ok = isPropertiesValid(m_header.props); }
                if (!ok) {
                    m_unserializeReasonCode = ReasonCodeV5::ProtocolError;
                    m_unserializeReasonString = QStringLiteral("protocol error: invalid connack properties");
                    return false;
                }
            }

            return true;
        }
        default: break;
    }

    m_unserializeReasonCode = ReasonCodeV5::MalformedPacket;
    m_unserializeReasonString = QStringLiteral("malformed packet: this is not rigth connack packet");
    return false;
}

bool ConnackPacket::isPropertiesValid(Properties & props)
{
    int session_expiry_interval_count = 0
       ,receive_maximum_count = 0
       ,maximum_qos_count = 0
       ,retain_available_count = 0
       ,maximum_packet_size_count = 0
       ,assigned_client_id_count = 0
       ,topic_alias_maximum_count = 0
       ,reason_string_count = 0
       ,wildcard_subscriptions_available_count = 0
       ,subscriptions_id_available_count = 0
       ,shared_subscriptions_available_count = 0
       ,server_keep_alive_count = 0
       ,response_information_count = 0
       ,server_reference_count = 0
       ,auth_method_count = 0
       ,auth_data_count = 0;

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
            case PropertyId::ReceiveMaximum:
            {
                ++receive_maximum_count;
                if (receive_maximum_count > 1)
                    return false;
                if (props[i].second.toUInt() == 0)
                    return false;
                break;
            }
            case PropertyId::MaximumQoS:
            {
                ++maximum_qos_count;
                if (maximum_qos_count > 1)
                    return false;
                /*! It is a Protocol Error to include Maximum QoS more than
                 * once, or to have a value other than 0 or 1 */
                if (props[i].second.toUInt() > 1)
                    return false;
                break;
            }
            case PropertyId::RetainAvailable:
            {
                ++retain_available_count;
                if (retain_available_count > 1)
                    return false;
                break;
            }
            case PropertyId::MaximumPacketSize:
            {
                ++maximum_packet_size_count;
                if (maximum_packet_size_count > 1)
                    return false;
                if (props[i].second.toUInt() == 0)
                    return false;
                break;
            }
            case PropertyId::AssignedClientIdentifier:
            {
                ++assigned_client_id_count;
                if (assigned_client_id_count > 1)
                    return false;
                break;
            }
            case PropertyId::TopicAliasMaximum:
            {
                ++topic_alias_maximum_count;
                if (topic_alias_maximum_count > 1)
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
            case PropertyId::WildcardSubscriptionAvailable:
            {
                ++wildcard_subscriptions_available_count;
                if (wildcard_subscriptions_available_count > 1)
                    return false;
                break;
            }
            case PropertyId::SubscriptionIdentifierAvailable:
            {
                ++subscriptions_id_available_count;
                if (subscriptions_id_available_count > 1)
                    return false;
                break;
            }
            case PropertyId::SharedSubscriptionAvailable:
            {
                ++shared_subscriptions_available_count;
                if (shared_subscriptions_available_count > 1)
                    return false;
                break;
            }
            case PropertyId::ServerKeepAlive:
            {
                ++server_keep_alive_count;
                if (server_keep_alive_count > 1)
                    return false;
                break;
            }
            case PropertyId::ResponseInformation:
            {
                ++response_information_count;
                if (response_information_count > 1)
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

            case PropertyId::UserProperty:  break;

            default:                 return false;
        }
    }

    if (auth_data_count != 0 && auth_method_count == 0)
        return false;

    if (receive_maximum_count == 0)
        props.append(Property(PropertyId::ReceiveMaximum, Constants::DefaultReceiveMaximum));

    if (maximum_qos_count == 0)
        props.append(Property(PropertyId::MaximumQoS, static_cast<quint8>(QoS::Value_2)));

    if (retain_available_count == 0)
        props.append(Property(PropertyId::RetainAvailable, true));

    if (topic_alias_maximum_count == 0)
        props.append(Property(PropertyId::TopicAliasMaximum, 0));

    if (wildcard_subscriptions_available_count == 0)
        props.append(Property(PropertyId::WildcardSubscriptionAvailable, true));

    if (subscriptions_id_available_count == 0)
        props.append(Property(PropertyId::SubscriptionIdentifierAvailable, true));

    if (shared_subscriptions_available_count == 0)
        props.append(Property(PropertyId::SharedSubscriptionAvailable, true));

    return true;
}
