#ifndef MQTT_PACKET_IDENTIFIER_CONTROLLER_H
#define MQTT_PACKET_IDENTIFIER_CONTROLLER_H

#include <QSet>

namespace Mqtt
{
    class PacketIdController
    {
    public:
        void removeId(quint16 id);

    public:
        quint16 generateId();
        bool addId(quint16 id);
        bool contains(quint16 id);

    private:
        quint16 id = 0;
        mutable QSet<quint16> ids;
    };

    inline bool PacketIdController::contains(quint16 id)  { return ids.contains(id); }
}

#endif // MQTT_PACKET_IDENTIFIER_CONTROLLER_H
