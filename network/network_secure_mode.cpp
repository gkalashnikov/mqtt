#include "network_secure_mode.h"
#include <QDebug>

static constexpr char kSecured[8]     = "secured";
static constexpr char kNonSecured[12] = "non secured";
static constexpr char kUnknown[8]     = "unknown";

QDebug operator << (QDebug d, const Network::SecureMode & type)
{
    const bool space = d.autoInsertSpaces();
    d.setAutoInsertSpaces(false);
    d << secureModeCStr(type);
    if (space)
    { d << ' '; }
    d.setAutoInsertSpaces(space);
    return d;
}

const char * Network::secureModeCStr(Network::SecureMode type)
{
    switch (type)
    {
        case Network::SecureMode::Secured:    { return kSecured;    }
        case Network::SecureMode::NonSecured: { return kNonSecured; }
        default:                              { break; }
    }
    return kUnknown;
}
