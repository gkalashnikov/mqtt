#ifndef NETWORK_EVENT_H
#define NETWORK_EVENT_H

#include "network_client.h"
#include <QEvent>
#include <QAbstractSocket>

namespace Network
{
    namespace Event
    {
        enum class Type
        {
             Data = QEvent::User + 1
            ,IncomingConnection
            ,OpenConnection
            ,ConnectionEstablished
            ,CloseConnection
            ,WillUpgraded
        };

        class Data : public QEvent
        {
        public:
            Data(quintptr connectionId, const QByteArray & data);
            ~Data();

        public:
            quintptr connectionId;
            QByteArray data;
        };

        class IncomingConnection : public QEvent
        {
        public:
            IncomingConnection(ServerClient && connection);
            ~IncomingConnection();

        public:
            ServerClient connection;
        };

        class OpenConnection : public QEvent
        {
        public:
            OpenConnection(const Client & connection);
            ~OpenConnection();

        public:
            Client connection;
        };

        class ConnectionEstablished : public QEvent
        {
        public:
            ConnectionEstablished(quintptr connectionId);
            ~ConnectionEstablished();

        public:
            quintptr connectionId;
        };

        class CloseConnection : public QEvent
        {
        public:
            CloseConnection(quintptr connectionId);
            ~CloseConnection();

        public:
            quintptr connectionId;
        };

        class WillUpgraded : public QEvent
        {
        public:
            WillUpgraded(quintptr connectionId);
            ~WillUpgraded();

        public:
            quintptr connectionId;
        };
    }
}

#endif // NETWORK_EVENT_H
