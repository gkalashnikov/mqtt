#ifndef MQTT_SUBSCRIPTIONS_H
#define MQTT_SUBSCRIPTIONS_H

#include "mqtt_topic.h"
#include "mqtt_special_symbols.h"
#include "mqtt_enum_qos.h"
#include <vector>
#include <QVariant>

namespace Mqtt
{
    class Subscriptions;

    class SubscriptionNodeData
    {
    public:
        SubscriptionNodeData() = default;
        virtual ~SubscriptionNodeData();
    };

    class SubscriptionNode
    {
    public:
        using List = std::vector<SubscriptionNode*>;
        using ConstList = std::vector<const SubscriptionNode*>;

        using Map = QMap<QString, SubscriptionNode*>;

    public:
        SubscriptionNode();
        ~SubscriptionNode();

    public:
        bool hasChildren() const;
        SubscriptionNode * parent() const;
        const QString & part() const;

        SubscriptionNode * provideNode(const TopicPart & part);
        SubscriptionNode * findNode(const TopicPart & part) const;
        void remove(SubscriptionNode *& node);
        void removeData();
        bool matchesWith(const Topic & topic) const;
        const Map & children() const;
        QString topic() const;

    public:
        SubscriptionNodeData *& data();

    private:
        void matchingNodes(const TopicPart & part, List * result) const;

    private:
        SubscriptionNode * m_parent;
        Map m_children;
        QString m_part;
        SubscriptionNodeData * m_data;

    private:
        friend class Subscriptions;
    };

    inline bool SubscriptionNode::hasChildren() const                        { return (m_children.size() != 0); }
    inline SubscriptionNode * SubscriptionNode::parent() const               { return m_parent;   }
    inline const QString & SubscriptionNode::part() const                    { return m_part;     }
    inline const SubscriptionNode::Map & SubscriptionNode::children() const  { return m_children; }
    inline SubscriptionNodeData *& SubscriptionNode::data()                  { return m_data;     }


    class Subscriptions
    {
    public:
        Subscriptions();
        ~Subscriptions();

    public:
        SubscriptionNode * findNode(const Topic & topic);
        SubscriptionNode * provide(const Topic & topic);
        void remove(SubscriptionNode * node);
        void remove(const Topic & topic);
        bool has(const Topic & topic) const;
        // for use after "has" method
        const SubscriptionNode::List & nodes() const;
        const SubscriptionNode * const rootNode() const;
        size_t count() const;
        QList<SubscriptionNode *> toTopicNodeList() const;

    private:
        SubscriptionNode m_node_root;
        mutable SubscriptionNode::List m_nodes_found;
    };

    inline void Subscriptions::remove(const Topic & topic)                  { return remove(findNode(topic)); }
    inline const SubscriptionNode::List & Subscriptions::nodes() const      { return m_nodes_found;           }
    inline const SubscriptionNode * const Subscriptions::rootNode() const   { return &m_node_root;            }
}

#endif // MQTT_SUBSCRIPTIONS_H
