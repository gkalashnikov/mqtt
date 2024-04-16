#ifndef MQTT_CHUNK_DATA_CONTROLLER_H
#define MQTT_CHUNK_DATA_CONTROLLER_H

#include <QByteArray>
#include <QElapsedTimer>

namespace Mqtt
{
    class ChunkDataController
    {
    public:
        ChunkDataController();

    public:
        void oneSecondTimer();

        void append(const QByteArray & chunk);
        bool packetAvailable();
        QByteArray takePacket();
        void clear();
        void setTimeout(qint64 secs);
        double load() const;

    private:
        quint64       m_packet_len;
        qint64        m_timeout;
        QByteArray    m_data;
        QByteArray    m_data_raw;
        QElapsedTimer m_timer;
    };

    inline void ChunkDataController::setTimeout(qint64 secs) { m_timeout = secs * 1000; }
}

#endif // MQTT_CHUNK_DATA_CONTROLLER_H
