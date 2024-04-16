#include "mqtt_enum_retcode_v5.h"

#include <QDebug>

QDebug operator << (QDebug d, Mqtt::ReasonCodeV5 code)
{
    d << quint8(code);

    switch (code)
    {
        case Mqtt::ReasonCodeV5::Success                            : d << QStringLiteral("No Errors"); break;
        case Mqtt::ReasonCodeV5::GrantedQoS1                        : d << QStringLiteral("Granted QoS 1"); break;
        case Mqtt::ReasonCodeV5::GrantedQoS2                        : d << QStringLiteral("Granted QoS 2"); break;
        case Mqtt::ReasonCodeV5::DisconnectWithWillMessage          : d << QStringLiteral("Disconnect With Will Message"); break;
        case Mqtt::ReasonCodeV5::NoMatchingSubscribers              : d << QStringLiteral("No Matching Subscribers"); break;
        case Mqtt::ReasonCodeV5::NoSubscriptionExisted              : d << QStringLiteral("No Subscription Existed"); break;
        case Mqtt::ReasonCodeV5::ContinueAuthentication             : d << QStringLiteral("Continue Authentication"); break;
        case Mqtt::ReasonCodeV5::ReAuthenticate                     : d << QStringLiteral("ReAuthenticate"); break;

        case Mqtt::ReasonCodeV5::UnspecifiedError                   : d << QStringLiteral("Unspecified Error"); break;
        case Mqtt::ReasonCodeV5::MalformedPacket                    : d << QStringLiteral("Malformed Packet"); break;
        case Mqtt::ReasonCodeV5::ProtocolError                      : d << QStringLiteral("Protocol Error"); break;
        case Mqtt::ReasonCodeV5::ImplementationSpecificError        : d << QStringLiteral("Implementation Specific Error"); break;
        case Mqtt::ReasonCodeV5::UnsupportedProtocolVersion         : d << QStringLiteral("Unsupported Protocol Version"); break;
        case Mqtt::ReasonCodeV5::ClientIdentifierNotValid           : d << QStringLiteral("Client Identifier NotValid"); break;
        case Mqtt::ReasonCodeV5::BadUserNameOrPassword              : d << QStringLiteral("Bad UserName Or Password"); break;
        case Mqtt::ReasonCodeV5::NotAuthorized                      : d << QStringLiteral("Not Authorized"); break;
        case Mqtt::ReasonCodeV5::ServerUnavailable                  : d << QStringLiteral("Server Unavailable"); break;
        case Mqtt::ReasonCodeV5::ServerBusy                         : d << QStringLiteral("Server Busy"); break;
        case Mqtt::ReasonCodeV5::Banned                             : d << QStringLiteral("Banned"); break;
        case Mqtt::ReasonCodeV5::ServerShuttingDown                 : d << QStringLiteral("Server Shutting Down"); break;
        case Mqtt::ReasonCodeV5::BadAuthenticationMethod            : d << QStringLiteral("Bad Authentication Method"); break;
        case Mqtt::ReasonCodeV5::KeepAliveTimeout                   : d << QStringLiteral("Keep Alive Timeout"); break;
        case Mqtt::ReasonCodeV5::SessionTakenOver                   : d << QStringLiteral("Session Taken Over"); break;
        case Mqtt::ReasonCodeV5::TopicFilterInvalid                 : d << QStringLiteral("Topic Filter Invalid"); break;
        case Mqtt::ReasonCodeV5::TopicNameInvalid                   : d << QStringLiteral("Topic Name Invalid"); break;
        case Mqtt::ReasonCodeV5::PacketIdentifierInUse              : d << QStringLiteral("Packet Identifier In Use"); break;
        case Mqtt::ReasonCodeV5::PacketIdentifierNotFound           : d << QStringLiteral("Packet Identifier Not Found"); break;
        case Mqtt::ReasonCodeV5::ReceiveMaximumExceeded             : d << QStringLiteral("Receive Maximum Exceeded"); break;
        case Mqtt::ReasonCodeV5::TopicAliasInvalid                  : d << QStringLiteral("Topic Alias Invalid"); break;
        case Mqtt::ReasonCodeV5::PacketTooLarge                     : d << QStringLiteral("Packet Too Large"); break;
        case Mqtt::ReasonCodeV5::MessageRateTooHigh                 : d << QStringLiteral("Message Rate Too High"); break;
        case Mqtt::ReasonCodeV5::QuotaExceeded                      : d << QStringLiteral("Quota Exceeded"); break;
        case Mqtt::ReasonCodeV5::AdministrativeAction               : d << QStringLiteral("Administrative Action"); break;
        case Mqtt::ReasonCodeV5::PayloadFormatInvalid               : d << QStringLiteral("Payload Format Invalid"); break;
        case Mqtt::ReasonCodeV5::RetainNotSupported                 : d << QStringLiteral("Retain Not Supported"); break;
        case Mqtt::ReasonCodeV5::QoSNotSupported                    : d << QStringLiteral("QoS Not Supported"); break;
        case Mqtt::ReasonCodeV5::UseAnotherServer                   : d << QStringLiteral("Use Another Server"); break;
        case Mqtt::ReasonCodeV5::ServerMoved                        : d << QStringLiteral("Server Moved"); break;
        case Mqtt::ReasonCodeV5::SharedSubscriptionsNotSupported    : d << QStringLiteral("Shared Subscriptions Not Supported"); break;
        case Mqtt::ReasonCodeV5::ConnectionRateExceeded             : d << QStringLiteral("Connection Rate Exceeded"); break;
        case Mqtt::ReasonCodeV5::MaximumConnectTime                 : d << QStringLiteral("Maximum Connect Time"); break;
        case Mqtt::ReasonCodeV5::SubscriptionIdentifiersNotSupported: d << QStringLiteral("Subscription Identifiers Not Supported"); break;
        case Mqtt::ReasonCodeV5::WildcardSubscriptionsNotSupported  : d << QStringLiteral("Wildcard Subscriptions Not Supported"); break;
    }

    return d;
}
