#ifndef MQTT_STORER_INTERFACE_H
#define MQTT_STORER_INTERFACE_H

#include <QString>

namespace Mqtt
{
    namespace Store
    {
        class PublishContainer;

        class IStorer
        {
        public:
            IStorer();
            virtual ~IStorer();

        public:
            virtual QByteArray load(const QString & key) = 0;
            virtual void store(const QString & key, const QByteArray & data) = 0;
            virtual void remove(const QString & key) = 0;
            virtual void beginReadKeys() = 0;
            virtual bool nextKeyAvailable() = 0;
            virtual QString nextKey() = 0;
            virtual void endReadKeys() = 0;
        };
    }
}

#endif // MQTT_STORER_INTERFACE_H
