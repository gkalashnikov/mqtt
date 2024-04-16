#ifndef MQTT_STORER_FACTORY_FILES_H
#define MQTT_STORER_FACTORY_FILES_H

#include "mqtt_storer_factory_interface.h"

namespace Mqtt
{
    namespace Store
    {
        class FilesStorerFactory : public IFactory
        {
        public:
            FilesStorerFactory(const QString & rootWorkDir);
            ~FilesStorerFactory() override;

            IStorer * createStorer(const QString & key) override;

        private:
            QString rootWorkDir;
        };
    }
}

#endif // MQTT_STORER_FACTORY_FILES_H
