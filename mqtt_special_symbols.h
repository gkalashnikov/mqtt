#ifndef MQTT_SPECIAL_SYMBOLS_H
#define MQTT_SPECIAL_SYMBOLS_H

#include <QString>

namespace Mqtt
{
    class SpecialSymbols
    {
    public:
        SpecialSymbols();

        enum Characters : ushort
        {
             Null   = 0x0000
            ,Hash   = 0x0023
            ,Dollar = 0x0024
            ,Plus   = 0x002B
            ,Slash  = 0x002F
        };
    };
}

#endif // MQTT_SPECIAL_SYMBOLS_H
