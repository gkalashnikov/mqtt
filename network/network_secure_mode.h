#ifndef NETWORK_SECURE_MODE_H
#define NETWORK_SECURE_MODE_H

#include <QtGlobal>

namespace Network
{
    enum class SecureMode : quint8
    {
         Unknown = 0
        ,NonSecured
        ,Secured
    };

    const char * secureModeCStr(SecureMode type);
}

class QDebug;
QDebug operator << (QDebug d, const Network::SecureMode & type);

#endif // NETWORK_SECURE_MODE_H
