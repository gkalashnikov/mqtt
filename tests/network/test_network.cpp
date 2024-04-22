#include <QTest>
#include <QTimer>
#include <network_server.h>
#include <QWebSocket>

namespace Test
{
    class Network : public QObject
    {
        Q_OBJECT
    private slots:
        void initTestCase();

        void testStartNetworListening();

        void testOpenTcpNetworkConnection();
        void testReadTcpNetworkData();
        void testWriteTcpNetworkData();
        void testCloseTcpNetworkConnection();
        void testCloseTcpNetworkConnectionByServer();

        void testOpenWsNetworkConnection();
        void testReadWsNetworkData();
        void testWriteWsNetworkData();
        void testCloseWsNetworkConnection();
        void testCloseWsNetworkConnectionByServer();

        void cleanupTestCase();

    public slots:
        void listeningStarted(QHostAddress address, quint16 port);
        void cantStartListening(QString error);

        void tcpSocketStateChanged(QAbstractSocket::SocketState state);
        void tcpSocketRead();

        void wsSocketStateChanged(QAbstractSocket::SocketState state);
        // server send binary messages only
        void wsSocketBinaryMessage(const QByteArray &message);

        void waitTimeout();

    protected:
        bool event(QEvent *event) override;

    protected:
        int  wait();
        void handleIncomingConnection(::Network::ServerClient client);
        void handleCloseConnection(quintptr connectionId);
        void handleIncomingNetworkData(quintptr connectionId, const QByteArray & data);
        void handleConnectionWillUpgraded(quintptr connectionId);
        void checkOpenConnectFinished();
        void checkCloseConnectFinished();

    private:
        enum class TestStep : quint8
        {
             TCP = 1
            ,WS
        };

    private:
        QHostAddress listenAddress = QHostAddress::LocalHost;
        quint16      listenPort    = 0;

        QEventLoop waitLoop;

        ::Network::Server * server;
        QTcpSocket * tcp_socket;
        QWebSocket * ws_socket;

        quint8 connectStages = 0;
        TestStep testStep    = TestStep::TCP;

        ::Network::ServerClient socketServerClient;

        QByteArrayList dataArray = QByteArrayList()
                                   << QByteArray::fromHex("101000044D5154540402003C000444494749")
                                   << QByteArray::fromHex("102400044d51545405c2003c05110000070800046d7950790006636c69656e74000a70617373")
                                   << QByteArray::fromHex("101e00044d51545405c2003c00046d7950790006636c69656e74000a70617373")
                                   << QByteArray::fromHex("820d00010000076d79746f70696301")
                                   << QByteArray::fromHex("820c000100076d79746f70696301")
                                   << QByteArray::fromHex("33140004696e666f000205020000001e436564616c6f")
                                   << QByteArray::fromHex("330e0004696e666f0002436564616c6f");

        QStringList stringArray = QStringList()
                                   << QStringLiteral("If the Will Flag is set to 1, the Will Properties, Will Topic, and Will Payload fields MUST be present in the Payload [MQTT-3.1.2-9]. The Will Message MUST be removed from the stored Session State in the Server once it has been published or the Server has received a DISCONNECT packet with a Reason Code of 0x00 (Normal disconnection) from the Client")
                                   << QStringLiteral("Followed by a Four Byte Integer representing the Maximum Packet Size the Client is willing to accept. If the Maximum Packet Size is not present, no limit on the packet size is imposed beyond the limitations in the protocol as a result of the remaining length encoding and the protocol header sizes.")
                                   << QStringLiteral("If the value of Request Problem Information is 0, the Server MAY return a Reason String or User Properties on a CONNACK or DISCONNECT packet, but MUST NOT send a Reason String or User Properties on any packet other than PUBLISH, CONNACK, or DISCONNECT [MQTT-3.1.2-29]. If the value is 0 and the Client receives a Reason String or User Properties in a packet other than PUBLISH, CONNACK, or DISCONNECT, it uses a DISCONNECT packet with Reason Code 0x82 (Protocol Error) as described in section 4.13 Handling errors.")
                                   << QStringLiteral("If a Server receives a CONNECT packet containing a Will Message with the Will Retain set to 1, and it does not support retained messages, the Server MUST reject the connection request. It SHOULD send CONNACK with Reason Code 0x9A (Retain not supported) and then it MUST close the Network Connection [MQTT-3.2.2-13].")
                                   << QStringLiteral("If the RETAIN flag is set to 1 in a PUBLISH packet sent by a Client to a Server, the Server MUST replace any existing retained message for this topic and store the Application Message [MQTT-3.3.1-5], so that it can be delivered to future subscribers whose subscriptions match its Topic Name. If the Payload contains zero bytes it is processed normally by the Server but any retained message with the same topic name MUST be removed and any future subscribers for the topic will not receive a retained message [MQTT1476 3.3.1-6]. A retained message with a Payload containing zero bytes MUST NOT be stored as a retained message on the Server [MQTT-3.3.1-7].")
                                   << QStringLiteral("• If the value of Retain As Published subscription option is set to 0, the Server MUST set the RETAIN flag to 0 when forwarding an Application Message regardless of how the RETAIN flag was set in the received PUBLISH packet [MQTT-3.3.1-12]. • If the value of Retain As Published subscription option is set to 1, the Server MUST set the RETAIN flag equal to the RETAIN flag in the received PUBLISH packet [MQTT-3.3.1-13].")
                                   << QStringLiteral("If the Client specified a Subscription Identifier for any of the overlapping subscriptions the Server MUST send those Subscription Identifiers in the message which is published as the result of the subscriptions [MQTT-3.3.4-3]. If the Server sends a single copy of the message it MUST include in the PUBLISH packet the Subscription Identifiers for all matching subscriptions which have a Subscription Identifiers, their order is not significant [MQTT-3.3.4-4]. If the Server sends multiple PUBLISH packets it MUST send, in each of them, the Subscription Identifier of the matching subscription if it has a Subscription Identifier [MQTT-3.3.4-5].");

         QByteArray infligthData;
         QByteArray incomingData;
    };
}

int maxloglevel = 12;

void Test::Network::waitTimeout()
{
    waitLoop.exit(1);
}

void Test::Network::initTestCase()
{
    server = new ::Network::Server(::Network::SecureMode::NonSecured, listenAddress, listenPort, QString(), QStringList());
    server->setNetworkEventsHandler(this);
    connect(server, &::Network::Server::listeningStarted,   this, &Test::Network::listeningStarted);
    connect(server, &::Network::Server::cantStartListening, this, &Test::Network::cantStartListening);

    tcp_socket = new QTcpSocket();
    connect(tcp_socket, &QTcpSocket::stateChanged, this, &Network::tcpSocketStateChanged);
    connect(tcp_socket, &QTcpSocket::readyRead   , this, &Network::tcpSocketRead);

    ws_socket = new QWebSocket();
    connect(ws_socket, &QWebSocket::stateChanged         , this, &Network::wsSocketStateChanged);
    connect(ws_socket, &QWebSocket::binaryMessageReceived, this, &Network::wsSocketBinaryMessage);
}

void Test::Network::testStartNetworListening()
{
    server->start();
    QVERIFY2(wait() == 0, "server must start listening");
}

void Test::Network::testOpenTcpNetworkConnection()
{
    testStep = TestStep::TCP;
    connectStages = 0;
    tcp_socket->connectToHost(listenAddress, listenPort);
    QVERIFY2(wait() == 0, "tcp socket must connect to server");
}

void Test::Network::testReadTcpNetworkData()
{
    for (QByteArray data: dataArray)
    {
        infligthData = data;
        tcp_socket->write(infligthData);
        QVERIFY2(wait() == 0, "tcp data must be written and received");
    }

    for (QString message: stringArray)
    {
        infligthData = message.toUtf8();
        tcp_socket->write(infligthData);
        QVERIFY2(wait() == 0, "tcp data must be written and received");
    }
}

void Test::Network::testWriteTcpNetworkData()
{
    for (QByteArray data: dataArray)
    {
        infligthData = data;
        socketServerClient.write(infligthData);
        QVERIFY2(wait() == 0, "tcp data must be written and received");
    }

    for (QString message: stringArray)
    {
        infligthData = message.toUtf8();
        socketServerClient.write(infligthData);
        QVERIFY2(wait() == 0, "tcp data must be written and received");
    }
}

void Test::Network::testCloseTcpNetworkConnection()
{
    connectStages = 0;
    tcp_socket->close();
    QVERIFY2(wait() == 0, "tcp socket must close connection");
}

void Test::Network::testCloseTcpNetworkConnectionByServer()
{
    testOpenTcpNetworkConnection();

    connectStages = 0;
    socketServerClient.close();
    QVERIFY2(wait() == 0, "tcp socket must be closed by server");

    QVERIFY2(tcp_socket->state() == QAbstractSocket::UnconnectedState, "tcp socket must be unconnected");
}

void Test::Network::testOpenWsNetworkConnection()
{
    testStep = TestStep::WS;
    connectStages = 0;
    ws_socket->open(QUrl(QString("ws://%1:%2").arg(listenAddress.toString()).arg(listenPort)));
    QVERIFY2(wait() == 0, "ws socket must connect to server");
}

void Test::Network::testReadWsNetworkData()
{
    for (QByteArray data: dataArray)
    {
        infligthData = data;
        ws_socket->sendBinaryMessage(infligthData);
        QVERIFY2(wait() == 0, "ws binary message must be written and received");
    }

    for (QString message: stringArray)
    {
        infligthData = message.toUtf8();
        ws_socket->sendTextMessage(message);
        QVERIFY2(wait() == 0, "ws text message must be written and received");
    }
}

void Test::Network::testWriteWsNetworkData()
{
    for (QByteArray data: dataArray)
    {
        infligthData = data;
        socketServerClient.write(data);
        QVERIFY2(wait() == 0, "ws data must be written and received");
    }

    for (QString message: stringArray)
    {
        infligthData = message.toUtf8();
        socketServerClient.write(infligthData);
        QVERIFY2(wait() == 0, "ws data must be written and received");
    }
}

void Test::Network::testCloseWsNetworkConnection()
{
    connectStages = 0;
    ws_socket->close();
    QVERIFY2(wait() == 0, "ws socket must close connection");
}

void Test::Network::testCloseWsNetworkConnectionByServer()
{
    testOpenWsNetworkConnection();

    connectStages = 0;
    socketServerClient.close();
    QVERIFY2(wait() == 0, "tcp socket must be closed by server");

    QVERIFY2(ws_socket->state() == QAbstractSocket::UnconnectedState, "ws socket must be unconnected");
}

void Test::Network::cleanupTestCase()
{
    connect(server, &::Network::Server::finished, &waitLoop, &QEventLoop::quit);

    server->quit();

    int serverStopped = (wait() == 0);

    delete server;
    delete tcp_socket;
    delete ws_socket;

    QVERIFY2(serverStopped, "server must stop listening normally");
}

void Test::Network::listeningStarted(QHostAddress address, quint16 port)
{
    listenPort = port;
    listenAddress = address;
    waitLoop.exit(0);
}

void Test::Network::cantStartListening(QString error)
{
    qDebug() << "network server can't start listening, error:" << error;
    waitLoop.exit(1);
}

void Test::Network::tcpSocketStateChanged(QAbstractSocket::SocketState state)
{
    switch (state)
    {
        case QAbstractSocket::ConnectedState:   { checkOpenConnectFinished();  return; }
        case QAbstractSocket::UnconnectedState: { checkCloseConnectFinished(); return; }
        default: break;
    }
}

void Test::Network::tcpSocketRead()
{
    auto count = tcp_socket->bytesAvailable();
    if (count > 0) {
        incomingData.append(tcp_socket->read(count));
        if (incomingData == infligthData) {
            incomingData.clear();
            waitLoop.exit(0);
        }
    }
}

void Test::Network::wsSocketStateChanged(QAbstractSocket::SocketState state)
{
    switch (state)
    {
        case QAbstractSocket::ConnectedState:   { checkOpenConnectFinished();  return; }
        case QAbstractSocket::UnconnectedState: { checkCloseConnectFinished(); return; }
        default: break;
    }
}

void Test::Network::wsSocketBinaryMessage(const QByteArray & message)
{
    incomingData.append(message);
    if (incomingData == infligthData) {
        incomingData.clear();
        waitLoop.exit(0);
    }
}

bool Test::Network::event(QEvent * event)
{
    ::Network::Event::Type type = static_cast<::Network::Event::Type>(event->type());

    switch (type)
    {
        case ::Network::Event::Type::IncomingConnection:
        {
            ::Network::Event::IncomingConnection * e = dynamic_cast<::Network::Event::IncomingConnection*>(event);
            handleIncomingConnection(e->connection);
            e->accept();
            return true;
        }

        case ::Network::Event::Type::CloseConnection:
        {
            ::Network::Event::CloseConnection * e = dynamic_cast<::Network::Event::CloseConnection*>(event);
            handleCloseConnection(e->connectionId);
            e->accept();
            return true;
        }

        case ::Network::Event::Type::Data:
        {
            ::Network::Event::Data * e = dynamic_cast<::Network::Event::Data*>(event);
            handleIncomingNetworkData(e->connectionId, e->data);
            e->accept();
            return true;
        }

        case ::Network::Event::Type::WillUpgraded:
        {
            ::Network::Event::WillUpgraded * e = dynamic_cast<::Network::Event::WillUpgraded*>(event);
            handleConnectionWillUpgraded(e->connectionId);
            e->accept();
            return true;
        }

        default: break;
    }

    return QObject::event(event);
}

int Test::Network::wait()
{
    QTimer * timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &Test::Network::waitTimeout);
    timer->start(std::chrono::seconds(5));
    int retcode = waitLoop.exec();
    if (retcode == 0)
        timer->stop();
    delete timer;
    return retcode;
}

void Test::Network::handleIncomingConnection(::Network::ServerClient client)
{
    socketServerClient = client;
    checkOpenConnectFinished();
}

void Test::Network::checkOpenConnectFinished()
{
    if (++connectStages != 2)
        return;

    QVERIFY2(socketServerClient.serverInstance() != Q_NULLPTR   , "server in client must be assigned");
    QVERIFY2(socketServerClient.serverInstance() == this->server, "server in client must be assigned to this server");

    switch (testStep)
    {
        case TestStep::TCP:
        {
            QVERIFY2(socketServerClient.ip() == tcp_socket->localAddress()  , "client ip and tcp socket local address must be equals");
            QVERIFY2(socketServerClient.port() == tcp_socket->localPort()   , "client port and tcp socket local port must be equals");
            QVERIFY2(tcp_socket->state() == QAbstractSocket::ConnectedState , "tcp socket state must be connected");
            break;
        }
        case TestStep::WS:
        {
            QVERIFY2(socketServerClient.ip() == ws_socket->localAddress()  , "client ip and ws socket local address must be equals");
            QVERIFY2(socketServerClient.port() == ws_socket->localPort()   , "client port and ws socket local port must be equals");
            QVERIFY2(ws_socket->state() == QAbstractSocket::ConnectedState , "ws socket state must be connected");
            break;
        }
        default: break;
    }

    waitLoop.exit(0);
}

void Test::Network::handleCloseConnection(quintptr connectionId)
{
    QVERIFY2(socketServerClient.id() == connectionId, "connection ids must be equals");
    checkCloseConnectFinished();
}

void Test::Network::checkCloseConnectFinished()
{
    if (++connectStages != 2)
        return;

    socketServerClient = ::Network::ServerClient();

    switch (testStep)
    {
        case TestStep::TCP:
        {
            QVERIFY2(tcp_socket->state() == QAbstractSocket::UnconnectedState , "tcp socket state must be unconnected");
            break;
        }

        case TestStep::WS:
        {
            QVERIFY2(ws_socket->state() == QAbstractSocket::UnconnectedState , "ws socket state must be unconnected");
            break;
        }
    }

    waitLoop.exit(0);
}

void Test::Network::handleIncomingNetworkData(quintptr connectionId, const QByteArray & data)
{
    QVERIFY2 (socketServerClient.id() == connectionId, "connection ids must be equals");

    incomingData.append(data);
    if (incomingData == infligthData) {
        incomingData.clear();
        waitLoop.exit(0);
    }
}

void Test::Network::handleConnectionWillUpgraded(quintptr connectionId)
{
    QVERIFY2 (socketServerClient.id() == connectionId, "connection ids must be equals");
    // after that new socket will be connected and current server client will be obsolete
    connectStages = 0;
    socketServerClient = ::Network::ServerClient();
}

QTEST_MAIN(Test::Network)
#include "test_network.moc"
