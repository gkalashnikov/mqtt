#ifndef MQTT_BROKER_CMD_OPTIONS_H
#define MQTT_BROKER_CMD_OPTIONS_H

#include <QCommandLineParser>

#ifndef QT_NO_OPENSSL
#include <QSslConfiguration>
#endif

#include "mqtt_constants.h"
#include "network_secure_mode.h"
#include "network_connection_type.h"

namespace Mqtt
{
    class BrokerOptions
    {
    public:
        BrokerOptions();
        void parse(QStringList ARGS);

        static std::tuple<Network::SecureMode, Network::ConnectionType, QString, quint16>
        parseConnectionReference(QString referense);

    private:
        void parseSsl(bool * sslEnabled);
        void parseListeners(bool sslEnabled);
        void parseConnections(const QList<QStringList> & ARGS, const QString & commandName, bool sslEnabled);
        void showCommandHelp(const QCommandLineParser & cmd, const QString & command, int exitCode);

    public:
        quint32 verboseLevel = 0;

    #ifndef QT_NO_OPENSSL
        QSharedPointer<QSslConfiguration> ssl;
    #endif

        QString rootPath;
        QString passFile;
        QString serverName;

        bool QoS0OfflineEnabled = false;

        quint32 maxFlowQoS0 = 0;
        quint32 maxFlowQoS1 = 0;
        quint32 maxFlowQoS2 = 0;

        quint32 banDuration      = 0;
        bool    banAccumulative = false;


        class Host
        {
        public:
            Network::SecureMode     secureMode;
            Network::ConnectionType connectionType;
            QString                 address;
            quint16                 port;
        };

        class Connection
        {
        public:
            Host                    host;
            QString                 clientId;
            QString                 username;
            QString                 password;
            QStringList             subscriptions;
        };

        class ConnectionPair
        {
        public:
            Connection local;
            Connection remote;
            bool       cleanStart;
            quint32    reconnectPeriod;
        };

        QList<Host> listeners;
        QList<ConnectionPair> connections;

    private:
        QCommandLineParser cmd;

        QCommandLineOption helpOption          {{
#                                                ifdef Q_OS_WIN
                                                 "?",
#                                                endif
                                                      "h", "help" } , "Displays this text." };
        QCommandLineOption rootDirOption       {{"d", "directory"}  , "Root directory path where broker data will be saved.", "name"};
        QCommandLineOption qos0OffOption       {"qos0-offline-queue", "Enables QoS 0 offline queue (1 enable, 0 disable, default 0).", "value", "0"};
        QCommandLineOption qos0FlowOption      {"qos0-max-flow"     , QString("QoS %1 messages max flow rate per second from client (default %2).").arg(0).arg(Constants::DefaultQoS0FlowRate), "count", QString::number(Constants::DefaultQoS0FlowRate)};
        QCommandLineOption qos1FlowOption      {"qos1-max-flow"     , QString("QoS %1 messages max flow rate per second from client (default %2).").arg(1).arg(Constants::DefaultQoS1FlowRate), "count", QString::number(Constants::DefaultQoS1FlowRate)};
        QCommandLineOption qos2FlowOption      {"qos2-max-flow"     , QString("QoS %1 messages max flow rate per second from client (default %2).").arg(2).arg(Constants::DefaultQoS2FlowRate), "count", QString::number(Constants::DefaultQoS2FlowRate)};
        QCommandLineOption banDurationOpt      {"ban-duration"      , QString("Client ban duration when max flow rate reached (default %1).").arg(QString::number(Constants::DefaultBanDuration)), "seconds", QString::number(Constants::DefaultBanDuration)};
        QCommandLineOption banTypeOption       {"ban-accumulative"  , "Ban duration accumulative (1 enable, 0 disable, default 0) ", "value", "0"};
        QCommandLineOption verboseOption       {"verbose"           , "Verbose level (from 0 to 12, default 3).", "value", "3"};
        QCommandLineOption passFileOption      {{"p", "pass-file"}  , "Passwords file path.",  "file"};
        QCommandLineOption certFileOption      {{"c", "cert-file"}  , "Certificate file path (*.public.pem).",  "file"};
        QCommandLineOption keyFileOption       {{"k", "key-file"}   , "Private key file path (*.private.pem).", "file"};
        QCommandLineOption serverNameOption    {{"s", "server-name"}, "Server name of broker used with websockets handshake.", "name", "mqtt"};
        QCommandLineOption listenerOption      {{"l", "listener"}   , "Network listener to start: mqtt(s)://localhost:1883", "name"};

        QCommandLineOption connCleanStart      {"clean-start"       , "MQTT clean start on connect. (1 enable, 0 disable, default 1)",  "value", "1"};
        QCommandLineOption connReconnectPeriod {"reconnect-period"  , "MQTT reconnect period in seconds", "value", "5"};
        QCommandLineOption localConnClientId   {"local-client-id"   , "MQTT client id for local connection.", "value"};
        QCommandLineOption localConnUsername   {"local-user"        , "MQTT username for local connection." , "value"};
        QCommandLineOption localConnPassword   {"local-password"    , "MQTT password for local connection." , "value"};
        QCommandLineOption localConnSubOption  {"local-subscribe"   , "Subscribe topic on local connection.", "topic"};
        QCommandLineOption remoteConnOption    {"remote-connection" , "Network connection to remote broker:\nmqtt(s)://remotehost:1883\nws(s)://remotehost:1883.", "name"};
        QCommandLineOption remoteConnClientId  {"remote-client-id"  , "MQTT client id for remote connection.", "value"};
        QCommandLineOption remoteConnUsername  {"remote-user"       , "MQTT username for remote connection." , "value"};
        QCommandLineOption remoteConnPassword  {"remote-password"   , "MQTT password for remote connection." , "value"};
        QCommandLineOption remoteConnSubOption {"remote-subscribe"  , "Subscribe topic on remote connection.", "topic"};

    };
}

#endif // MQTT_BROKER_CMD_OPTIONS_H
