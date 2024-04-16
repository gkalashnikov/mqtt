#ifndef MQTT_PASSWORD_FILE_H
#define MQTT_PASSWORD_FILE_H

#include <QFile>
#include <QMap>

namespace Mqtt
{
    class PasswordFile : private QFile
    {
        Q_OBJECT
    public:
        PasswordFile();
        PasswordFile(const QString & name);
        PasswordFile(QObject * parent);
        PasswordFile(const QString & name, QObject * parent);
        ~PasswordFile();

    public:
        using QFile::errorString;

        bool sync();
        bool isUserAccepted(const QString & user, const QString & password);

    private:
        QMap<QString, QByteArray> users;
    };
}

#endif // MQTT_PASSWORD_FILE_H
