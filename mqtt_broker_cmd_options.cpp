#include "mqtt_broker_cmd_options.h"

#include <QCoreApplication>
#include <QRegularExpression>
#include <QDir>

#ifndef QT_NO_OPENSSL
#include <QSslCertificate>
#include <QSslKey>
#include <QFile>
#endif

using namespace Mqtt;
using namespace Network;

extern void Q_CORE_EXPORT qt_call_post_routines();

enum class ArgumentType : quint8
{
     General
    ,Connection
};

BrokerOptions::BrokerOptions()
{
    cmd.setOptionsAfterPositionalArgumentsMode(QCommandLineParser::ParseAsOptions);

    cmd.addOption(helpOption);
    cmd.addVersionOption();
    cmd.addOption(verboseOption);
    cmd.addOption(rootDirOption);
    cmd.addOption(qos0OffOption);
    cmd.addOption(qos0FlowOption);
    cmd.addOption(qos1FlowOption);
    cmd.addOption(qos2FlowOption);
    cmd.addOption(banDurationOpt);
    cmd.addOption(banTypeOption);
    cmd.addOption(passFileOption);
    cmd.addOption(serverNameOption);
    cmd.addOption(listenerOption);
#   ifndef QT_NO_OPENSSL
    cmd.addOption(certFileOption);
    cmd.addOption(keyFileOption);
#   endif
    cmd.addPositionalArgument(QStringLiteral("subcommand"), QStringLiteral("connect\n"), QStringLiteral("[subcommands...]"));
}

void BrokerOptions::parse(QStringList ARGS)
{
    const QString connectCommandName = QStringLiteral("connect");

    QStringList        generalARGS;
    QList<QStringList> connectionARGS;

    ArgumentType type = ArgumentType::General;

    while (!ARGS.isEmpty())
    {
        QString a = ARGS.takeFirst();
        if (a == connectCommandName)
        {
            connectionARGS << QStringList();
            connectionARGS.last() << generalARGS.at(0);
            type = ArgumentType::Connection;
            continue;
        }
        switch (type)
        {
            case ArgumentType::General:    { generalARGS           << a; break; }
            case ArgumentType::Connection: { connectionARGS.last() << a; break; }
        }
    }

    if (!cmd.parse(generalARGS)) {
        qDebug() << qPrintable(cmd.errorText()) << '\n';
        cmd.showHelp(1);
    }

    if (cmd.isSet(helpOption))
        cmd.showHelp();

    if (cmd.isSet("version"))
        cmd.showVersion();

    verboseLevel = cmd.value(verboseOption).toInt();

    bool ssl = false;
    parseSsl(&ssl);

    rootPath   = cmd.isSet(rootDirOption) ? cmd.value(rootDirOption) : QCoreApplication::applicationDirPath();
    passFile   = cmd.value(passFileOption);
    serverName = cmd.value(serverNameOption);

    QoS0OfflineEnabled = cmd.value(qos0OffOption).toUInt();

    maxFlowQoS0 = cmd.value(qos0FlowOption).toULong();
    maxFlowQoS1 = cmd.value(qos1FlowOption).toULong();
    maxFlowQoS2 = cmd.value(qos2FlowOption).toULong();

    banDuration     = cmd.value(banDurationOpt).toULong();
    banAccumulative = cmd.value(banTypeOption).toUInt();

    parseListeners(ssl);

    if (listeners.isEmpty()) {
        qDebug().noquote() << "can't start broker: listeners is empty!\n";
        cmd.showHelp(1);
    }

    parseConnections(connectionARGS, connectCommandName, ssl);

    if (!connections.isEmpty())
    {
        Host localHost = listeners[0];

        if (localHost.secureMode == SecureMode::Secured)
        {
            for (QList<Host>::size_type i = 1; i < listeners.count(); ++i) {
                if (listeners[i].secureMode != SecureMode::Secured) {
                    localHost = listeners[i];
                    break;
                }
            }
        }

        if (localHost.address == QStringLiteral("0.0.0.0"))
            localHost.address = QStringLiteral("127.0.0.1");

        for (QList<ConnectionPair>::size_type i = 0; i < connections.count(); ++i)
            connections[i].local.host = localHost;
    }
}

void BrokerOptions::showCommandHelp(const QCommandLineParser & cmd, const QString & command, int exitCode)
{
    QString help_text = cmd.helpText();
    help_text.remove(0, help_text.indexOf('\n') + 1);

    qDebug() << "Usage:" << qPrintable(QDir::toNativeSeparators(QCoreApplication::instance()->applicationFilePath())) << qPrintable(command) << "[options]";
    qDebug() << qPrintable(help_text);

    qt_call_post_routines();
    ::exit(exitCode);
}

std::tuple<SecureMode, ConnectionType, QString, quint16> BrokerOptions::parseConnectionReference(QString reference)
{
    QRegularExpression mqtt_url_regex(QStringLiteral("^((mqtts?)|(wss?))://.{0,}:[0-9]{1,5}"), QRegularExpression::NoPatternOption);
    auto result = mqtt_url_regex.match(reference);
    if (result.hasMatch())
    {
        QStringList    parts = result.captured(0).remove('/').replace(QStringLiteral("localhost"), QStringLiteral("127.0.0.1")).split(':');
        SecureMode     mode  = (parts[0] == QStringLiteral("mqtts") || parts[0] == QStringLiteral("wss")) ? SecureMode::Secured : SecureMode::NonSecured;
        ConnectionType type  = parts[0].startsWith(QStringLiteral("ws")) ? ConnectionType::WS : ConnectionType::TCP;
        return std::make_tuple(mode, type, parts[1], parts[2].toUShort());
    }
    return std::make_tuple(SecureMode::Unknown, ConnectionType::Unknown, QString(), 0);
}

void BrokerOptions::parseListeners(bool sslEnabled)
{
    QStringList values = cmd.values(listenerOption);

    for (QString listener: values)
    {
        auto result = BrokerOptions::parseConnectionReference(listener);

        SecureMode mode  = std::get<0>(result);

        if (mode != SecureMode::Unknown)
        {
            if (SecureMode::Secured == mode && !sslEnabled)
            {
#               ifndef QT_NO_OPENSSL
                qDebug() << "to use secured listener you need cert and key files." << '\n';
#               else
                qDebug() << "build not supports ssl." << '\n';
#               endif
                cmd.showHelp(1);
            }
            Host host;
            host.secureMode     = mode;
            host.connectionType = std::get<1>(result);
            host.address        = std::get<2>(result);
            host.port           = std::get<3>(result);
            listeners.append(host);
        }
    }
}

void BrokerOptions::parseConnections(const QList<QStringList> & ARGS, const QString & commandName, bool sslEnabled)
{
    for (QStringList arguments: ARGS)
    {
        QCommandLineParser cmd;

        cmd.addOption(helpOption         );
        cmd.addOption(connCleanStart     );
        cmd.addOption(connReconnectPeriod);
        cmd.addOption(localConnClientId  );
        cmd.addOption(localConnUsername  );
        cmd.addOption(localConnPassword  );
        cmd.addOption(localConnSubOption );
        cmd.addOption(remoteConnOption   );
        cmd.addOption(remoteConnClientId );
        cmd.addOption(remoteConnUsername );
        cmd.addOption(remoteConnPassword );
        cmd.addOption(remoteConnSubOption);

        if (!cmd.parse(arguments)) {
            qDebug() << qPrintable(cmd.errorText()) << '\n';
            showCommandHelp(cmd, commandName, 1);
        }

        if (cmd.isSet(helpOption))
            showCommandHelp(cmd, commandName, 0);

        QString connection = cmd.value(remoteConnOption);

        auto result = BrokerOptions::parseConnectionReference(connection);

        SecureMode mode  = std::get<0>(result);

        if (mode != SecureMode::Unknown)
        {
            if (SecureMode::Secured == mode && !sslEnabled)
            {
#               ifndef QT_NO_OPENSSL
                qDebug() << "to use secured connection you need cert and key files." << '\n';
#               else
                qDebug() << "build not supports ssl." << '\n';
#               endif
                showCommandHelp(cmd, commandName, 1);
            }

            ConnectionPair opt;
            opt.cleanStart                 = cmd.value  (connCleanStart).toInt();
            opt.reconnectPeriod            = cmd.value  (connReconnectPeriod).toULongLong();
            opt.remote.host.secureMode     = mode;
            opt.remote.host.connectionType = std::get<1>(result);
            opt.remote.host.address        = std::get<2>(result);
            opt.remote.host.port           = std::get<3>(result);
            opt.remote.clientId            = cmd.value  (remoteConnClientId);
            opt.remote.username            = cmd.value  (remoteConnUsername);
            opt.remote.password            = cmd.value  (remoteConnPassword);
            opt.remote.subscriptions       = cmd.values (remoteConnSubOption);
            opt.local.clientId             = cmd.value  (localConnClientId);
            opt.local.username             = cmd.value  (localConnUsername);
            opt.local.password             = cmd.value  (localConnPassword);
            opt.local.subscriptions        = cmd.values (localConnSubOption);
            connections.append(opt);
        }
    }
}

void BrokerOptions::parseSsl(bool * sslEnabled)
{
    *sslEnabled = false;
#ifndef QT_NO_OPENSSL
    const QString cer_path = cmd.value(certFileOption);
    const QString key_path = cmd.value(keyFileOption);
    if (!cer_path.isEmpty() && !key_path.isEmpty())
    {
        QFile cert_file(cer_path);
        if (!cert_file.open(QIODevice::ReadOnly)) {
            qDebug().noquote() << "cannot open cert file:" << cer_path << cert_file.errorString();
            cmd.showHelp(1);
        }
        QSslCertificate ssl_certificate(&cert_file, QSsl::Pem);
        cert_file.close();

        QFile key_file(key_path);
        if (!key_file.open(QIODevice::ReadOnly)) {
            qDebug().noquote() << "cannot open key file:" << key_path << key_file.errorString();
            cmd.showHelp(1);
        }
        QSslKey ssl_key(&key_file, QSsl::Rsa, QSsl::Pem);
        key_file.close();

        *sslEnabled = true;
        ssl = QSharedPointer<QSslConfiguration>(new QSslConfiguration());
        ssl->setLocalCertificate(ssl_certificate);
        ssl->setPrivateKey(ssl_key);
        ssl->setPeerVerifyMode(QSslSocket::VerifyNone);
        ssl->setProtocol(QSsl::TlsV1_2OrLater);
    }
#endif
}
