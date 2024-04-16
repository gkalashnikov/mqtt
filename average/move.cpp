#include "move.h"

QByteArray Average::Load::toJSON(const QByteArray & counterName) const
{
    QByteArray name = counterName.isEmpty() ? QByteArrayLiteral("count") : counterName;
    QByteArray json;
    json.reserve(128 + counterName.length());
    json.append('{');
    json.append('\"'); json.append(name); json.append("\":"); json.append(QByteArray::number(m_count));
    if (m_values.size() > 0) {
        json.append(",\"load (");   json.append(name[0]); json.append("/s)\":{");
        for (size_t i = 0; i < m_values.size(); ++i) {
            if (i > 0) json.append(',');
            json.append('\"');
            if (m_values[i].second < 60) {
                json.append(QByteArray::number(m_values[i].second));
                json.append("sec\":");
            } else {
                json.append(QByteArray::number(m_values[i].second / 60));
                json.append("min\":");
            }
            json.append(QByteArray::number(m_values[i].first));
        }
        json.append('}');
    }
    json.append('}');
    return json;
}
