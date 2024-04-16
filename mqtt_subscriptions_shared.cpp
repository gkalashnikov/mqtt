#include "mqtt_subscriptions_shared.h"

using namespace Mqtt;

SharedSubscriptionData::~SharedSubscriptionData()
{

}

void SharedSubscriptionData::add(const QString & consumer, SessionPtr session, SubscribeOptions options, quint32 subscriptionId)
{
    SharedSubscriptionSessionsList & sessions = m_consumers[consumer];
    auto it = sessions.begin();
    while (it != sessions.end()) {
        auto & p = (*it);
        SessionPtr s = p.session.toStrongRef();
        if (s.isNull()) {
            it = sessions.erase(it);
            continue;
        }
        if (s == session) {
            p.data.options    = options;
            p.data.identifier = subscriptionId;
            return;
        } ++it;
    }
    sessions.push_back({ session.toWeakRef(), { options, subscriptionId } });
}

int SharedSubscriptionData::remove(const QString & consumer, SessionPtr session)
{
    auto consumer_it = m_consumers.find(consumer);
    if (consumer_it == m_consumers.end())
        return 0;
    int count = 0;
    SharedSubscriptionSessionsList & sessions = *consumer_it;
    auto it = sessions.begin();
    while (it != sessions.end()) {
        auto & p = (*it);
        SessionPtr s = p.session.toStrongRef();
        if (s.isNull() || s == session) {
            it = sessions.erase(it);
            ++count;
            continue;
        } ++it;
    }
    if (sessions.size() == 0)
        m_consumers.erase(consumer_it);
    return count;
}
