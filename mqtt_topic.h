#ifndef MQTT_TOPIC_H
#define MQTT_TOPIC_H

#include <QString>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QStringRef>
#else
#include <QStringView>
#endif
#include <QVector>

namespace Mqtt
{

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    typedef QStringRef TopicPart;
    typedef QVector<TopicPart> TopicPartArray;
#else
    typedef QStringView TopicPart;
    typedef QList<TopicPart> TopicPartArray;
#endif

    class Topic
    {
    public:
        explicit Topic(const QString & topic);

    public:
        enum Destination : quint8
        {
             All
            ,Shared
            ,System
        };

    public:
        int partsCount() const;
        const TopicPart & operator[](int i) const;

        Destination destination() const;
        const QString & consumer() const;
        bool isValidForSubscribe() const;
        bool isValidForPublish() const;

    private:
        Destination m_destination;
        QString m_consumer;
        QString m_value;
        TopicPartArray m_parts;
    };

    inline Topic::Destination Topic::destination() const      { return m_destination;   }
    inline const QString & Topic::consumer() const            { return m_consumer;      }
    inline int Topic::partsCount() const                      { return m_parts.count(); }
    inline const TopicPart & Topic::operator [](int i) const  { return m_parts[i];      }

}

#endif // MQTT_TOPIC_H
