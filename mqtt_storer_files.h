#ifndef MQTT_STORER_FILES_H
#define MQTT_STORER_FILES_H

#include "mqtt_storer_interface.h"
#include <QDir>
#include <QDirIterator>
#include <QSharedPointer>

namespace Mqtt
{
    namespace Store
    {
        class FilesStorer : public IStorer
        {
        private:
            class PathBuilder
            {
            public:
                explicit PathBuilder(const QString & workDir);

                bool canStore(const QString & key);
                QString makePath(const QString & key);
                QString makeKey(const QString & path);
                void changePath(const QString & path);

            private:
                static void ensurePathExists(QDir & dir, const QString & path);

            private:
                QDir workDir;
                friend class FilesStorer;
            };

        public:
            explicit FilesStorer(const QString & workDir);
            ~FilesStorer() override;

        public:
            bool canStore(const QString & key) override;
            QByteArray load(const QString & key) override;
            void store(const QString & key, const QByteArray & data) override;
            void remove(const QString & key) override;
            void beginReadKeys() override;
            bool nextKeyAvailable() override;
            QString nextKey() override;
            void endReadKeys() override;

        private:
            PathBuilder builder;
            QSharedPointer<QDirIterator> iterator;
        };
    }
}

#endif // MQTT_STORER_FILES_H
