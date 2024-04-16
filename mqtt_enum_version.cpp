#include <mqtt_enum_version.h>

#include <QDebug>

QDebug operator << (QDebug d, Mqtt::Version version)
{
    switch (version)
    {
       case Mqtt::Version::Unused    : d << QStringLiteral("Unknown"); break;
       case Mqtt::Version::Ver_3_1   : d << QStringLiteral("3.1");     break;
       case Mqtt::Version::Ver_3_1_1 : d << QStringLiteral("3.1.1");   break;
       case Mqtt::Version::Ver_5_0   : d << QStringLiteral("5.0");     break;
    }

    return d;
}
