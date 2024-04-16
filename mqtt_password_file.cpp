#include "mqtt_password_file.h"
#include <QTextStream>
#include <QCryptographicHash>

using namespace Mqtt;

PasswordFile::PasswordFile()
    :QFile()
{

}

PasswordFile::PasswordFile(const QString & name)
    :QFile(name)
{

}

PasswordFile::PasswordFile(QObject * parent)
    :QFile(parent)
{

}

PasswordFile::PasswordFile(const QString & name, QObject * parent)
    :QFile(name, parent)
{

}

PasswordFile::~PasswordFile()
{

}

bool PasswordFile::sync()
{
    users.clear();

    if (!open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QTextStream in(this);
    while (!in.atEnd()) {
        QStringList pair = in.readLine().simplified().split(' ');
        if (pair.count() > 1)
            users[pair[0]] = pair[1].toUtf8();
    }

    close();

    return true;
}

bool PasswordFile::isUserAccepted(const QString & user, const QString & password)
{
    auto it = users.find(user);
    if (it != users.end()) {
        QByteArray pass = password.toUtf8();
        if (pass == *it)
            return true;
        return (QCryptographicHash::hash(pass, QCryptographicHash::Md5) == *it);
    }
    return false;
}
