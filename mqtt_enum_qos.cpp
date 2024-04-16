#include "mqtt_enum_qos.h"

#include <QDebug>

QDebug operator << (QDebug d, Mqtt::QoS qos)
{
    switch (qos)
    {
       case Mqtt::QoS::Value_0   : d << QStringLiteral("QoS 0");        break;
       case Mqtt::QoS::Value_1   : d << QStringLiteral("QoS 1");        break;
       case Mqtt::QoS::Value_2   : d << QStringLiteral("QoS 2");        break;
       case Mqtt::QoS::Reserved  : d << QStringLiteral("QoS Reserved"); break;
    }

    return d;
}
