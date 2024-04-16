#include "mqtt_chunk_data_controller.h"
#include "mqtt_control_packet.h"
#include "mqtt_constants.h"

using namespace Mqtt;

ChunkDataController::ChunkDataController()
    :m_packet_len(0)
    ,m_timeout(Constants::DefaultKeepAliveInterval * 2 * 1000)
{
    m_timer.start();
}

void ChunkDataController::oneSecondTimer()
{
    if (m_data.isEmpty())
        return;

    if (m_timer.elapsed() > m_timeout) {
        clear();
        m_timer.restart();
    }
}

void ChunkDataController::append(const QByteArray & chunk)
{
    if (chunk.isEmpty())
        return;

    if ((chunk.size() + m_data.size()) > Constants::MaxIncomingDataLength) {
        clear();
        return;
    }

    m_data = m_data.isEmpty() ? chunk : m_data.append(chunk);
    m_data_raw = QByteArray::fromRawData(m_data.constData(), m_data.length());
    m_timer.restart();
}

bool ChunkDataController::packetAvailable()
{
    m_packet_len = 0;

    if (m_data_raw.length() < 2)
        return false;

    m_packet_len = ControlPacket::packetFullLength(m_data_raw);

    if (m_packet_len == -1)
        return false;

    if (m_packet_len > quint64(m_data_raw.length())) {
        m_data = QByteArray(m_data_raw.constData(), m_data_raw.length());
        m_data_raw = QByteArray::fromRawData(m_data.constData(), m_data.length());
        return false;
    }

    return true;
}

QByteArray ChunkDataController::takePacket()
{
    QByteArray packet;
    if (m_packet_len == m_data.length()) {
        packet = m_data;
        clear();
    } else if (m_packet_len == m_data_raw.length()) {
        packet = QByteArray(m_data_raw.constData(), m_packet_len);
        clear();
    } else if (m_packet_len < m_data_raw.length()) {
        packet = QByteArray(m_data_raw.constData(), m_packet_len);
        m_data_raw = QByteArray::fromRawData(m_data_raw.constData() + packet.length(), m_data_raw.length() - packet.length());
    }
    return packet;
}

void ChunkDataController::clear()
{
    m_data_raw.clear();
    m_data.clear();
}
