#ifndef MQTT_ENUM_PROPERTY_ID_H
#define MQTT_ENUM_PROPERTY_ID_H

#include <QtGlobal>

namespace Mqtt
{
    enum class PropertyId : quint8
    {
         PayloadFormatIndicator          = 1
        ,MessageExpiryInterval           = 2
        ,ContentType                     = 3
        ,ResponseTopic                   = 8
        ,CorrelationData                 = 9
        ,SubscriptionIdentifier          = 11
        ,SessionExpiryInterval           = 17
        ,AssignedClientIdentifier        = 18
        ,ServerKeepAlive                 = 19
        ,AuthentificationMethod          = 21
        ,AuthentificationData            = 22
        ,RequestProblemInformation       = 23
        ,WillDelayInterval               = 24
        ,RequestResponseInformation      = 25
        ,ResponseInformation             = 26
        ,ServerReference                 = 28
        ,ReasonString                    = 31
        ,ReceiveMaximum                  = 33
        ,TopicAliasMaximum               = 34
        ,TopicAlias                      = 35
        ,MaximumQoS                      = 36
        ,RetainAvailable                 = 37
        ,UserProperty                    = 38
        ,MaximumPacketSize               = 39
        ,WildcardSubscriptionAvailable   = 40
        ,SubscriptionIdentifierAvailable = 41
        ,SharedSubscriptionAvailable     = 42
    };
}

#endif // MQTT_ENUM_PROPERTY_ID_H
