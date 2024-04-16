#ifndef MQTT_ENUM_RETURN_CODE_V3_H
#define MQTT_ENUM_RETURN_CODE_V3_H

#include <QtGlobal>

namespace Mqtt
{
    enum class ReturnCodeV3 : quint8
    {
         Accepted                    = 0 /* Connection accepted */
        ,UnacceptableProtocolVersion = 1 /* The Server does not support the level of the MQTT protocol requested by the Client */
        ,IdentifierRejected          = 2 /* The Client identifier is correct UTF-8 but not allowed by the Server */
        ,ServerUnavailable           = 3 /* The Network Connection has been made but the MQTT service is unavailable */
        ,BadUserNameOrPassword       = 4 /* The data in the user name or password is malformed */
        ,NotAuthorized               = 5 /* The Client is not authorized to connect */
    };
}

class QDebug;
QDebug operator << (QDebug d, Mqtt::ReturnCodeV3 type);

#endif // MQTT_ENUM_RETURN_CODE_V3_H
