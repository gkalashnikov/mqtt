#ifndef MQTT_ENUM_FLAGS_H
#define MQTT_ENUM_FLAGS_H

#include <QtGlobal>

namespace Mqtt
{
    enum class Flags : quint8
    {
         CONNECT     = 0
        ,CONNACK     = 0
        ,PUBLISH     = 0  /* DUP:1 QOS:2  RETAIN:1 */
        ,PUBACK      = 0
        ,PUBREC      = 0
        ,PUBREL      = 2
        ,PUBCOMP     = 0
        ,SUBSCRIBE   = 2
        ,SUBACK      = 0
        ,UNSUBSCRIBE = 2
        ,UNSUBACK    = 0
        ,PINGREQ     = 0
        ,PINGRESP    = 0
        ,DISCONNECT  = 0
        ,AUTH        = 0
    };
}

#endif // MQTT_ENUM_FLAGS_H
