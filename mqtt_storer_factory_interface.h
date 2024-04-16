#ifndef MQTT_STORER_FACTORY_INTERFACE_H
#define MQTT_STORER_FACTORY_INTERFACE_H

#include "mqtt_storer_interface.h"

namespace Mqtt
{
    namespace Store
    {
        class IStorer;

        class IFactory
        {
        public:
            IFactory();
            virtual ~IFactory();

            virtual IStorer * createStorer(const QString & key = QString()) = 0;
        };
    }
}

#endif // MQTT_STORER_FACTORY_INTERFACE_H
