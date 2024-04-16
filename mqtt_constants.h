#ifndef MQTT_CONSTANTS_H
#define MQTT_CONSTANTS_H

#include "mqtt_enums.h"
#include <QtGlobal>

namespace Mqtt
{
    class Constants
    {
    public:
        static constexpr qint32  DefaultSessionTimeout     = 3600; /* secs */
        static constexpr QoS     DefaultMaxQoS             = QoS::Value_2;
        static constexpr qint32  DefaultMaxPacketSize      = std::numeric_limits<qint32>::max(); /* bytes count */
        static constexpr quint16 DefaultReceiveMaximum     = std::numeric_limits<quint16>::max(); /* messages count */
        static constexpr qint32  DefaultKeepAliveInterval  = 60; /* secs */
        static constexpr quint32 DefaultQoS0FlowRate       = 5000; /* packets per sec */
        static constexpr quint32 DefaultQoS1FlowRate       = 2500; /* packets per sec */
        static constexpr quint32 DefaultQoS2FlowRate       = 1250; /* packets per sec */
        static constexpr quint32 DefaultBanDuration        = 5; /* secs */
        static constexpr qint32  MaxIncomingDataLength     = 256 * 1024 * 1024; /* bytes count */
        static constexpr qint64  MinSubscriptionIdentifier = 1;
        static constexpr qint64  MaxSubscriptionIdentifier = 268435455;
        static constexpr quint32 ForeverSessionInterval    = std::numeric_limits<quint32>::max();
        static constexpr quint16 TopicAliasMaximum         = std::numeric_limits<quint16>::max() - 1;
    };
}

#endif // MQTT_CONSTANTS_H
