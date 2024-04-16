#ifndef MQTT_ENUM_PACKET_TYPE_H
#define MQTT_ENUM_PACKET_TYPE_H

#include <QtGlobal>

namespace Mqtt
{
    enum class PacketType : quint8
    {
         RESERVED    = 0  /* Reserved */
        ,CONNECT     = 1  /* Client to Server, Connection request  */
        ,CONNACK     = 2  /* Server to Client, Connect acknowledgment */
        ,PUBLISH     = 3  /* Client to Server or Server to Clien,  Publish message */
        ,PUBACK      = 4  /* Client to Server or Server to Client, Publish acknowledgment (QoS 1) */
        ,PUBREC      = 5  /* Client to Server or Server to Client, Publish received (QoS 2 delivery part 1) */
        ,PUBREL      = 6  /* Client to Server or Server to Client, Publish release  (QoS 2 delivery part 2) */
        ,PUBCOMP     = 7  /* Client to Server or Server to Client, Publish complete (QoS 2 delivery part 3) */
        ,SUBSCRIBE   = 8  /* Client to Server, Subscribe request */
        ,SUBACK      = 9  /* Server to Client, Subscribe acknowledgment */
        ,UNSUBSCRIBE = 10 /* Client to Server, Unsubscribe request */
        ,UNSUBACK    = 11 /* Server to Client, Unsubscribe acknowledgment */
        ,PINGREQ     = 12 /* Client to Server, PING request */
        ,PINGRESP    = 13 /* Server to Client, PING response */
        ,DISCONNECT  = 14 /* Client to Server or Server to Client, Disconnect notification */
        ,AUTH        = 15 /* Client to Server or Server to Client, Authentication exchange */
    };
}

class QDebug;
QDebug operator << (QDebug d, Mqtt::PacketType type);

#endif // MQTT_ENUM_PACKET_TYPE_H
