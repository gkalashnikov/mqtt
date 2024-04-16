#include "mqtt_subscriptions.h"
#include "mqtt_enum_qos.h"

using namespace Mqtt;

SubscriptionNodeData::~SubscriptionNodeData()
{

}

SubscriptionNode::SubscriptionNode()
    :m_parent(Q_NULLPTR)
    ,m_children()
    ,m_part()
    ,m_data(Q_NULLPTR)
{

}

SubscriptionNode::~SubscriptionNode()
{
    removeData();
    qDeleteAll(m_children);
}

SubscriptionNode * SubscriptionNode::provideNode(const TopicPart & part)
{
    auto it = m_children.find(QString::fromRawData(part.data(), part.length()));
    if (it == m_children.end()) {
        SubscriptionNode * node = new SubscriptionNode();
        node->m_parent  = this;
        node->m_part    = part.toString();
        it = m_children.insert(node->m_part, node);
    }
    return *it;
}

SubscriptionNode * SubscriptionNode::findNode(const TopicPart & part) const
{
    auto it = m_children.find(QString::fromRawData(part.data(), part.length()));
    if (it != m_children.end())
        return *it;
    return Q_NULLPTR;
}

void SubscriptionNode::remove(SubscriptionNode *& node)
{
    auto it = std::find(m_children.begin(), m_children.end(), node);
    if (it != m_children.end()) {
        delete (*it);
        m_children.erase(it);
        node = Q_NULLPTR;
    }
}

void SubscriptionNode::removeData()
{
    if (m_data != Q_NULLPTR) {
        delete m_data;
        m_data = Q_NULLPTR;
    }
}

void SubscriptionNode::matchingNodes(const TopicPart & part, List * result) const
{
    static thread_local QString PLUS { QChar { SpecialSymbols::Plus } };
    static thread_local QString HASH { QChar { SpecialSymbols::Hash } };

    auto it = m_children.find(QString::fromRawData(part.data(), part.length()));
    if (it != m_children.end())
        result->push_back(*it);

    it = m_children.find(PLUS);
    if (it != m_children.end())
        result->push_back(*it);

    it = m_children.find(HASH);
    if (it != m_children.end())
        result->push_back(*it);
}

bool SubscriptionNode::matchesWith(const Topic & topic) const
{
    static thread_local SubscriptionNode::ConstList nodes;

    if (m_data == Q_NULLPTR)
        return false;

    nodes.clear();

    const SubscriptionNode * n = this;
    while (n->parent() != Q_NULLPTR) {
        nodes.push_back(n);
        n = n->parent();
    }

    int i = 0;
    auto r_it = nodes.rbegin();
    auto r_e_it = nodes.rend();

    if (Topic::System == topic.destination()) {
        if ((*r_it)->part() != topic[0])
            return false;
        ++r_it; ++i;
    }

    while (r_it != r_e_it && i != topic.partsCount()) {
        const QString & node_part = (*r_it)->part();
        if (node_part == SpecialSymbols::Hash)
            return true;
        if (node_part != SpecialSymbols::Plus && node_part != topic[i])
            return false;
        ++r_it; ++i;
    }

    return ((r_it == r_e_it) && (i == topic.partsCount()));
}

QString SubscriptionNode::topic() const
{
    static thread_local SubscriptionNode::ConstList nodes;
    nodes.clear();

    size_t topiclen = 0;

    const SubscriptionNode * n = this;
    while(n->parent() != Q_NULLPTR) {
        topiclen += n->part().length();
        ++topiclen;
        nodes.push_back(n);
        n = n->parent();
    } --topiclen;

    QString topic;
    topic.reserve(topiclen);

    auto r_it = nodes.rbegin();
    auto r_b_it = r_it;
    auto r_e_it = nodes.rend();
    while (r_it != r_e_it) {
        if (r_it != r_b_it)
            topic.append('/');
        topic.append((*r_it)->part());
        ++r_it;
    }

    return topic;
}

Subscriptions::Subscriptions()
{

}

Subscriptions::~Subscriptions()
{

}

SubscriptionNode * Subscriptions::findNode(const Topic & topic)
{
    SubscriptionNode * n = &m_node_root;
    for (int i = 0; i < topic.partsCount(); ++i)
        n = n ? n->findNode(topic[i]) : Q_NULLPTR;
    return n;
}

SubscriptionNode * Subscriptions::provide(const Topic & topic)
{
    SubscriptionNode * n = &m_node_root;
    for (int i = 0; i < topic.partsCount(); ++i)
        n = n->provideNode(topic[i]);
    return n;
}

void Subscriptions::remove(SubscriptionNode * n)
{
    if (n == Q_NULLPTR)
        return;

    n->removeData();

    if (n->hasChildren())
        return;

    while (n->parent()) {
        SubscriptionNode * p = n->parent();
        p->remove(n);
        if (p->hasChildren())
            break;
        n = p;
    }
}

bool Subscriptions::has(const Topic & topic) const
{
    static thread_local SubscriptionNode::List nodes;
    static thread_local SubscriptionNode::List matching;

    m_nodes_found.clear();

    nodes.clear();
    matching.clear();

    int i = 0;

    if (Topic::System == topic.destination()) {
        SubscriptionNode * node = m_node_root.findNode(topic[0]);
        if (node == Q_NULLPTR)
            return false;
        nodes.push_back(node); ++i;
    } else {
        nodes.push_back(const_cast<SubscriptionNode*>(&m_node_root));
    }

    for ( ; i < topic.partsCount(); ++i) {
        for (auto node: nodes) {
            if (node->part() == SpecialSymbols::Hash) {
                matching.push_back(node);
            } else {
                node->matchingNodes(topic[i], const_cast<SubscriptionNode::List*>(&matching));
            }
        }
        std::swap(nodes, matching);
        matching.clear();
    }

    if (m_nodes_found.capacity() < nodes.size())
        m_nodes_found.reserve(nodes.size());

    for (auto node: nodes)
        if (node->data() != Q_NULLPTR)
            m_nodes_found.push_back(node);

    return (m_nodes_found.size() != 0);
}

size_t countFinalNodes(const SubscriptionNode * node)
{
    size_t c = 0;
    for (auto n: node->children()) {
        if (n->data() != Q_NULLPTR)
            ++c;
        c += countFinalNodes(n);
    }
    return c;
}

size_t Subscriptions::count() const
{
    return ::countFinalNodes(&m_node_root);
}

void collectFinalNodes(const SubscriptionNode * node, std::vector<SubscriptionNode*> & nodes)
{
    for (auto n: node->children()) {
        if (n->data() != Q_NULLPTR)
            nodes.push_back(n);
        collectFinalNodes(n, nodes);
    }
}

QList<SubscriptionNode*> Subscriptions::toTopicNodeList() const
{
    static thread_local std::vector<SubscriptionNode*> nodes;
    nodes.clear();
    ::collectFinalNodes(&m_node_root, nodes);
    QList<SubscriptionNode*> list;
    list.reserve(nodes.size());
    for (auto n: nodes)
        list.append(n);
    return list;
}
