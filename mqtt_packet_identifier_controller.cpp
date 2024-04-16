#include "mqtt_packet_identifier_controller.h"

using namespace Mqtt;

quint16 PacketIdController::generateId()
{
    for (quint16 i = 0; i < std::numeric_limits<quint16>::max(); ++i) {
        ++id;
        if (0 == id) id = 1;
        auto it = ids.find(id);
        if (it == ids.end()) {
            ids.insert(id);
            return id;
        }
    }
    return 0;
}

bool PacketIdController::addId(quint16 id)
{
    auto it = ids.find(id);
    bool ok = (it == ids.end());
    if (ok)
        ids.insert(id);
    return ok;
}

void PacketIdController::removeId(quint16 id)
{
    auto it = ids.find(id);
    if (it != ids.end())
        ids.erase(it);
}
