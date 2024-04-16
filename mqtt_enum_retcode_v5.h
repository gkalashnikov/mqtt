#ifndef MQTT_ENUM_RETURN_CODE_V5_H
#define MQTT_ENUM_RETURN_CODE_V5_H

#include <QtGlobal>

namespace Mqtt
{
    enum class ReasonCodeV5 : quint8
    {
        /* NOT ERRORS */
         NoErrors                             = 0
        ,Success                              = 0
        ,NormalDisconnection                  = 0
        ,GrantedQoS0                          = 0   /* SUBACK */
        ,GrantedQoS1                          = 1   /* SUBACK */
        ,GrantedQoS2                          = 2   /* SUBACK */
        ,DisconnectWithWillMessage            = 4   /* DISCONNECT */
        ,NoMatchingSubscribers                = 16  /* PUBACK, PUBREC */
        ,NoSubscriptionExisted                = 17  /* UNSUBACK */
        ,ContinueAuthentication               = 24  /* AUTH */
        ,ReAuthenticate                       = 25  /* AUTH */

        /* ERRORS */
        ,UnspecifiedError                     = 128 /* CONNACK, PUBACK, PUBREC, SUBACK, UNSUBACK, DISCONNECT */
        ,MalformedPacket                      = 129 /* CONNACK, DISCONNECT */
        ,ProtocolError                        = 130 /* CONNACK, DISCONNECT */
        ,ImplementationSpecificError          = 131 /* CONNACK, PUBACK, PUBREC, SUBACK, UNSUBACK, DISCONNECT */
        ,UnsupportedProtocolVersion           = 132 /* CONNACK */
        ,ClientIdentifierNotValid             = 133 /* CONNACK */
        ,BadUserNameOrPassword                = 134 /* CONNACK */
        ,NotAuthorized                        = 135 /* CONNACK, PUBACK, PUBREC, SUBACK, UNSUBACK, DISCONNECT */
        ,ServerUnavailable                    = 136 /* CONNACK */
        ,ServerBusy                           = 137 /* CONNACK, DISCONNECT */
        ,Banned                               = 138 /* CONNACK */
        ,ServerShuttingDown                   = 139 /* DISCONNECT */
        ,BadAuthenticationMethod              = 140 /* CONNACK, DISCONNECT */
        ,KeepAliveTimeout                     = 141 /* DISCONNECT */
        ,SessionTakenOver                     = 142 /* DISCONNECT */
        ,TopicFilterInvalid                   = 143 /* SUBACK, UNSUBACK, DISCONNECT */
        ,TopicNameInvalid                     = 144 /* CONNACK, PUBACK, PUBREC, DISCONNECT */
        ,PacketIdentifierInUse                = 145 /* PUBACK, PUBREC, SUBACK, UNSUBACK */
        ,PacketIdentifierNotFound             = 146 /* PUBREL, PUBCOMP */
        ,ReceiveMaximumExceeded               = 147 /* DISCONNECT */
        ,TopicAliasInvalid                    = 148 /* DISCONNECT */
        ,PacketTooLarge                       = 149 /* CONNACK, DISCONNECT */
        ,MessageRateTooHigh                   = 150 /* DISCONNECT */
        ,QuotaExceeded                        = 151 /* CONNACK, PUBACK, PUBREC, SUBACK, DISCONNECT */
        ,AdministrativeAction                 = 152 /* DISCONNECT */
        ,PayloadFormatInvalid                 = 153 /* CONNACK, PUBACK, PUBREC, DISCONNECT */
        ,RetainNotSupported                   = 154 /* CONNACK, DISCONNECT */
        ,QoSNotSupported                      = 155 /* CONNACK, DISCONNECT */
        ,UseAnotherServer                     = 156 /* CONNACK, DISCONNECT */
        ,ServerMoved                          = 157 /* CONNACK, DISCONNECT */
        ,SharedSubscriptionsNotSupported      = 158 /* SUBACK, DISCONNECT */
        ,ConnectionRateExceeded               = 159 /* CONNACK, DISCONNECT */
        ,MaximumConnectTime                   = 160 /* DISCONNECT */
        ,SubscriptionIdentifiersNotSupported  = 161 /* SUBACK, DISCONNECT */
        ,WildcardSubscriptionsNotSupported    = 162 /* SUBACK, DISCONNECT */
    };
}

class QDebug;
QDebug operator << (QDebug d, Mqtt::ReasonCodeV5 type);

#endif // MQTT_ENUM_RETURN_CODE_V5_H
