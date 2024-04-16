#include "mqtt_enum_retcode_v3.h"

#include <QDebug>

QDebug operator << (QDebug d, Mqtt::ReturnCodeV3 code)
{
    d << quint8(code);

    switch (code)
    {
       case Mqtt::ReturnCodeV3::Accepted                    : d << QStringLiteral("connection accepted");                                                                break;
       case Mqtt::ReturnCodeV3::UnacceptableProtocolVersion : d << QStringLiteral("the server does not support the level of the MQTT protocol requested by the client"); break;
       case Mqtt::ReturnCodeV3::IdentifierRejected          : d << QStringLiteral("the client identifier is correct UTF-8 but not allowed by the server");               break;
       case Mqtt::ReturnCodeV3::ServerUnavailable           : d << QStringLiteral("the network connection has been made but the MQTT service is unavailable");           break;
       case Mqtt::ReturnCodeV3::BadUserNameOrPassword       : d << QStringLiteral("the data in the user name or password is malformed");                                 break;
       case Mqtt::ReturnCodeV3::NotAuthorized               : d << QStringLiteral("the client is not authorized to connect");                                            break;
    }

    return d;
}
