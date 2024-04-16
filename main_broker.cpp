#include <csignal>
#include <QCoreApplication>
#include <QTimer>

#include "network_server.h"
#include "network_client_socket_controller.h"
#include "mqtt_broker_cmd_options.h"
#include "mqtt_broker.h"
#include "mqtt_storer_factory_files.h"
#include "mqtt_password_file.h"
#include "mqtt_bridge.h"
#include "version.h"
#include "logger.h"

using namespace Mqtt;
using namespace Network;

int maxloglevel = 3;

void signalHandler(int signal)
{
    log_important << "caught signal" << signal << end_log;
    std::signal(signal, SIG_DFL);
    QCoreApplication::instance()->exit(signal);
}

int main(int argc, char *argv[])
{
    int retcode = 0;
    {
        QCoreApplication app(argc, argv);

        std::signal(SIGINT,  signalHandler);
        std::signal(SIGTERM, signalHandler);

        app.setApplicationName(PROJECT_NAME);
        app.setApplicationVersion(QStringLiteral("%1 build at %2, rev. %3")
                                  .arg(PROJECT_VERSION_STR)
                                  .arg(PROJECT_BUILD_TIMESTAMP)
                                  .arg(PROJECT_GIT_REVISION_STR));

        const QStringList subProtocols {{
                 QStringLiteral("mqtt")
                ,QStringLiteral("mqttv3"), QStringLiteral("mqttv31"), QStringLiteral("mqttv311"), QStringLiteral("mqttv3.1")
                ,QStringLiteral("mqttv3.1.1"), QStringLiteral("mqttv5"), QStringLiteral("mqttv50"), QStringLiteral("mqttv5.0")
                                       }};

        BrokerOptions options;
        options.parse(app.arguments());

        maxloglevel = options.verboseLevel;

        BrokerPtr broker;
        QSharedPointer<Mqtt::Store::IFactory> storerFactory;

        storerFactory = QSharedPointer<Mqtt::Store::IFactory>(new Store::FilesStorerFactory(options.rootPath));
        broker        = BrokerPtr(new Broker(storerFactory.data()));

        if (!options.passFile.isEmpty() && !broker->setPasswordFile(options.passFile))
        {
            qDebug().noquote() << "can't read password file:" << broker->passwordFile()->errorString() << '\n';
            return 1;
        }

        broker->setQoS0OfflineEnabled(options.QoS0OfflineEnabled);
        broker->setMaxFlowPerSecond(QoS::Value_0, options.maxFlowQoS0);
        broker->setMaxFlowPerSecond(QoS::Value_1, options.maxFlowQoS1);
        broker->setMaxFlowPerSecond(QoS::Value_2, options.maxFlowQoS2);
        broker->setBanDuration(options.banDuration, options.banAccumulative);

        QList<ServerPtr> listeners;
        {
            for (auto listener: options.listeners)
            {
                listeners.append(ServerPtr(new Server(listener.secureMode, QHostAddress(listener.address), listener.port, options.serverName, subProtocols)));
                QObject::connect(listeners.last().data(), &Server::cantStartListening, &app, [&] { app.exit(1); }, Qt::QueuedConnection);
#               ifndef QT_NO_OPENSSL
                if (SecureMode::Secured == listener.secureMode)
                    listeners.last()->setSslConfiguration(options.ssl);
#               endif
                broker->addListener(listeners.last());
                listeners.last()->start();
            }
        }

        QList<Mqtt::BridgePtr> connections;
        ClientSocketControllerPtr clientSocketController;

        if (options.connections.size() > 0)
        {
            clientSocketController = ClientSocketControllerPtr(new ClientSocketController());
#           ifndef QT_NO_OPENSSL
            clientSocketController->setSslConfiguration(options.ssl);
#           endif
            clientSocketController->start();

            for (auto connect: options.connections)
            {
                Mqtt::Bridge * bridge = new Mqtt::Bridge(clientSocketController.data());
                connections.append(Mqtt::BridgePtr(bridge));

                bridge->setCleanStart(connect.cleanStart);
                bridge->setReconnectPeriod(connect.reconnectPeriod);
                bridge->setLocalConnectTimeout(300);
                bridge->setLocalSecureMode(connect.local.host.secureMode);
                bridge->setLocalConnectionType(connect.local.host.connectionType);
                bridge->setLocalHostAddress(connect.local.host.address);
                bridge->setLocalHostPort(connect.local.host.port);
                bridge->setLocalClientId(connect.local.clientId);
                bridge->setLocalUsername(connect.local.username);
                bridge->setLocalPassword(connect.local.password);

                bridge->setRemoteConnectTimeout(300);
                bridge->setRemoteSecureMode(connect.remote.host.secureMode);
                bridge->setRemoteConnectionType(connect.remote.host.connectionType);
                bridge->setRemoteHostAddress(connect.remote.host.address);
                bridge->setRemoteHostPort(connect.remote.host.port);
                bridge->setRemoteClientId(connect.remote.clientId);
                bridge->setRemoteUsername(connect.remote.username);
                bridge->setRemotePassword(connect.remote.password);

                for (QString sub: connect.remote.subscriptions)
                    bridge->remoteSubscribe(sub, QoS::Value_2, true, true, SubscribeOptions::RetainHandling::NotSendAtSubscribe);

                for (QString sub: connect.local.subscriptions)
                    bridge->localSubscribe(sub, QoS::Value_2, true, true, SubscribeOptions::RetainHandling::NotSendAtSubscribe);

                bridge->connectToHosts();
            }
        }

        retcode = app.exec();

        log_important << "internal data saving..." << end_log;

        connections.clear();
        listeners.clear();
        broker.clear();
        clientSocketController.clear();
        storerFactory.clear();

        log_important << "exit with return code" << retcode << end_log;
    }

    return retcode;
}
