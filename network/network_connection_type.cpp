#include "network_connection_type.h"
#include <QDebug>

static constexpr char kTcp[4] = "tcp";
static constexpr char kWs[4]  = "ws ";
static constexpr char kUnk[8] = "unknown";

QDebug operator << (QDebug d, const Network::ConnectionType & type)
{
    const bool space = d.autoInsertSpaces();
    d.setAutoInsertSpaces(false);
    d << connectionTypeCStr(type);
    if (space)
    { d << ' '; }
    d.setAutoInsertSpaces(space);
    return d;
}

const char * Network::connectionTypeCStr(ConnectionType type)
{
    switch (type)
    {
        case Network::ConnectionType::TCP: { return kTcp; }
        case Network::ConnectionType::WS:  { return kWs;  }
        default:                                 { break; }
    }
    return kUnk;
}
