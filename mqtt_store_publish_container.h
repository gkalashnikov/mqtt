#ifndef MQTT_STORE_PUBLISH_CONTAINER_H
#define MQTT_STORE_PUBLISH_CONTAINER_H

#include "mqtt_store_publish_unit.h"
#include <QMap>
#include <QSet>

namespace Mqtt
{
    namespace Store
    {
        class IStorer;

        typedef QMap<QString, PublishUnit> BaseContainer;

        class PublishContainer : public QObject, private BaseContainer
        {
            Q_OBJECT
        public:
            explicit PublishContainer(QObject * parent = Q_NULLPTR);
            PublishContainer(IStorer * storer, QObject * parent = Q_NULLPTR);
            ~PublishContainer() override;

        public:
            using BaseContainer::iterator;
            using BaseContainer::const_iterator;
            using BaseContainer::size_type;

            using BaseContainer::begin;
            using BaseContainer::constBegin;
            using BaseContainer::end;
            using BaseContainer::constEnd;

            using BaseContainer::find;
            using BaseContainer::constFind;
            using BaseContainer::size;

        public:
            PublishUnit & operator[](const QString & key);
            void add(const QString & key, const QString & clientId, const PublishPacket & packet);
            size_type remove(const QString & key);
            void syncAll();
            void sync(const QString & key);
            BaseContainer::iterator erase(iterator it);
            BaseContainer::iterator insert(const QString & key, const PublishUnit & value);
            BaseContainer::iterator insert(const_iterator pos, const QString & key, const PublishUnit & value);
            void removeAll();
            QString nextOrderedKey() const;

        public:
            bool hasStorer() const;
            void setStorer(IStorer * storer);
            IStorer * storer();
            void scheduleSync(quint32 msDelay);
            void scheduleSync(const QString & key);
            void loadUnit(const QString & key, PublishUnit & unit);

        protected:
            void timerEvent(QTimerEvent * event) override;

        private:
            void clear();
            void preload();
            void executeSync();

        private:
            template <class T>
            class UniqueOrderedQueue : private QList<T>
            {
            public:
                using QList<T>::isEmpty;

                inline void enqueue(const T &t) { if (!unique.contains(t)) { QList<T>::append(t); unique.insert(t); } }
                inline T dequeue() { T t = QList<T>::takeFirst(); unique.remove(t); return t; }
                inline void clear() { QList<T>::clear(); unique.clear(); }

            private:
                QSet<T> unique;
            };

        private:
            IStorer * m_storer  = Q_NULLPTR;
            int m_sync_timer_id = 0;
            UniqueOrderedQueue<QString> m_sync;
        };

        inline IStorer * PublishContainer::storer() { return m_storer; }
    }
}

#endif // MQTT_STORE_PUBLISH_CONTAINER_H
