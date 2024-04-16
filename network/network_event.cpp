#include "network_event.h"

using namespace Network::Event;

Data::Data(quintptr connectionId, const QByteArray & data)
    :QEvent(static_cast<QEvent::Type>(Event::Type::Data))
    ,connectionId(connectionId)
    ,data(data)
{

}

Data::~Data()
{

}

IncomingConnection::IncomingConnection(ServerClient && connection)
    :QEvent(static_cast<QEvent::Type>(Event::Type::IncomingConnection))
    ,connection(std::move(connection))
{

}

IncomingConnection::~IncomingConnection()
{

}

OpenConnection::OpenConnection(const Client & connection)
    :QEvent(static_cast<QEvent::Type>(Event::Type::OpenConnection))
    ,connection(connection)
{

}

OpenConnection::~OpenConnection()
{

}

ConnectionEstablished::ConnectionEstablished(quintptr connectionId)
    :QEvent(static_cast<QEvent::Type>(Event::Type::ConnectionEstablished))
    ,connectionId(connectionId)
{

}

ConnectionEstablished::~ConnectionEstablished()
{

}

CloseConnection::CloseConnection(quintptr connectionId)
    :QEvent(static_cast<QEvent::Type>(Event::Type::CloseConnection))
    ,connectionId(connectionId)
{

}

CloseConnection::~CloseConnection()
{

}

WillUpgraded::WillUpgraded(quintptr connectionId)
    :QEvent(static_cast<QEvent::Type>(Event::Type::WillUpgraded))
    ,connectionId(connectionId)
{

}

WillUpgraded::~WillUpgraded()
{

}
