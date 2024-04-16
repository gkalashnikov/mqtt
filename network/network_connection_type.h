#ifndef NETWORK_CONNECTION_TYPE_H
#define NETWORK_CONNECTION_TYPE_H

#include <QtGlobal>

namespace Network
{
    enum class ConnectionType : quint8
    {
         Unknown = 0
        ,TCP
        ,WS
    };

    const char * connectionTypeCStr(ConnectionType type);
}

class QDebug;
QDebug operator << (QDebug d, const Network::ConnectionType & type);

#endif // NETWORK_CONNECTION_TYPE_H
