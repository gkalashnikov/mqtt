#ifndef MQTT_ENUM_QOS_H
#define MQTT_ENUM_QOS_H

#include <QtGlobal>

namespace Mqtt
{
    enum class QoS : quint8
    {
         Value_0  = 0
        ,Value_1  = 1
        ,Value_2  = 2
        ,Reserved = 3
    };
    inline bool operator>(QoS qos, quint8 value) { return static_cast<quint8>(qos) > value; }
}

class QDebug;
QDebug operator << (QDebug d, Mqtt::QoS qos);

#endif // MQTT_ENUM_QOS_H
