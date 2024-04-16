#include "mqtt_statistic.h"
#include <QTimer>

using namespace Mqtt;

Statistic::Messages::Messages()
{
    set(received);
    set(sent);
    set(dropped);
}

void Statistic::Messages::set(Average::Move<900> & s)
{
    s.load().valuesReserve(3);

    s.load().valueAppend(60);
    s.load().valueAppend(300);
    s.load().valueAppend(900);
}

void Statistic::Messages::oneSecondTimer()
{
    received.oneSecondTimer();
    sent.oneSecondTimer();
    dropped.oneSecondTimer();
}

Statistic::Statistic(QObject * parent)
{
    QTimer * timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &Statistic::oneSecondTimer);
    timer->start(1000);
}

Statistic::~Statistic()
{

}

void Statistic::oneSecondTimer()
{
    broker.uptime++;
    publish.oneSecondTimer();
    allmessages.oneSecondTimer();
}

void Statistic::setTotalClients(int count)
{
    clients.total = count;
    if (clients.total > clients.maximum)
        clients.maximum = clients.total;
}
