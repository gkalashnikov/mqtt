#ifndef MQTT_ENUM_VERSION_H
#define MQTT_ENUM_VERSION_H

#include <QtGlobal>

namespace Mqtt
{
    enum class Version : quint8
    {
         Unknown   = 0
        ,Unused    = 0
        ,Ver_3_1   = 3
        ,Ver_3_1_1 = 4
        ,Ver_5_0   = 5
    };
}

class QDebug;
QDebug operator << (QDebug d, Mqtt::Version type);

#endif // MQTT_ENUM_VERSION_H
