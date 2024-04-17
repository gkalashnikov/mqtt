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
        void   setTimeout(qint64 secs);
        void   oneSecondTimer();

        qint32 maxDataSize() const;
        void   setMaxDataSize(qint32 bytesCount);

        void   append(const QByteArray & chunk);
        bool   packetAvailable();
        void   clear();
        bool   isEmpty() const;

        QByteArray takePacket();

    private:
        qint32        m_max_data_size;
        quint64       m_packet_len;
        qint64        m_timeout;
        QByteArray    m_data;
        QByteArray    m_data_raw;
        QElapsedTimer m_timer;
    };

    inline void ChunkDataController::setTimeout(qint64 secs)           { m_timeout = secs * 1000;      }
    inline qint32 ChunkDataController::maxDataSize() const             { return m_max_data_size;       }
    inline void ChunkDataController::setMaxDataSize(qint32 bytesCount) { m_max_data_size = bytesCount; }
    inline bool ChunkDataController::isEmpty() const                   { return m_data.isEmpty();      }
}

#endif // MQTT_CHUNK_DATA_CONTROLLER_H
