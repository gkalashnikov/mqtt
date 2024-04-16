#include "mqtt_enum_packet_type.h"

#include <QDebug>

QDebug operator << (QDebug d, Mqtt::PacketType type)
{
    switch (type)
    {
       case Mqtt::PacketType::RESERVED    : d << QStringLiteral("RESERVED");    break;
       case Mqtt::PacketType::CONNECT     : d << QStringLiteral("CONNECT");     break;
       case Mqtt::PacketType::CONNACK     : d << QStringLiteral("CONNACK");     break;
       case Mqtt::PacketType::PUBLISH     : d << QStringLiteral("PUBLISH");     break;
       case Mqtt::PacketType::PUBACK      : d << QStringLiteral("PUBACK");      break;
       case Mqtt::PacketType::PUBREC      : d << QStringLiteral("PUBREC");      break;
       case Mqtt::PacketType::PUBREL      : d << QStringLiteral("PUBREL");      break;
       case Mqtt::PacketType::PUBCOMP     : d << QStringLiteral("PUBCOMP");     break;
       case Mqtt::PacketType::SUBSCRIBE   : d << QStringLiteral("SUBSCRIBE");   break;
       case Mqtt::PacketType::SUBACK      : d << QStringLiteral("SUBACK");      break;
       case Mqtt::PacketType::UNSUBSCRIBE : d << QStringLiteral("UNSUBSCRIBE"); break;
       case Mqtt::PacketType::UNSUBACK    : d << QStringLiteral("UNSUBACK");    break;
       case Mqtt::PacketType::PINGREQ     : d << QStringLiteral("PINGREQ");     break;
       case Mqtt::PacketType::PINGRESP    : d << QStringLiteral("PINGRESP");    break;
       case Mqtt::PacketType::DISCONNECT  : d << QStringLiteral("DISCONNECT");  break;
       case Mqtt::PacketType::AUTH        : d << QStringLiteral("AUTH");        break;
    }

    return d;
}
