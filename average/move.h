#ifndef AVERAGE_MOVE_H
#define AVERAGE_MOVE_H

#include <vector>
#include <QPair>
#include <QByteArray>

namespace Average
{
    // averaged values
    class Load
    {
    public:
        inline void increaseCount(qint64 v)           { m_count += v; }
        inline void valuesReserve(size_t count)       { m_values.reserve(count); }
        inline void valueAppend(size_t rateInSeconds) { m_values.push_back(std::move(QPair<double, size_t>{ 0, rateInSeconds })); }
        inline size_t valuesCount() const             { return m_values.size();    }
        inline double & value(size_t i)               { return m_values[i].first;  }
        inline double value(size_t i) const           { return m_values[i].first;  }
        inline size_t & valueRate(size_t i)           { return m_values[i].second; }
        inline size_t valueRate(size_t i) const       { return m_values[i].second; }

        QByteArray toJSON(const QByteArray & counterName) const;

    public:
        qint64 m_count = 0;
        std::vector< QPair<double, size_t> > m_values;
    };

    // circle buf
    template <size_t capacity, class ValueType>
    class Window
    {
    public:
        inline void append(ValueType value)      { m_last_idx == (capacity - 1) ? m_last_idx = 0 : ++m_last_idx;  if (m_size < capacity) ++m_size; m_array[m_last_idx] = value; }
        inline ValueType & operator [](size_t i) { if (m_size == 0) return m_array[0]; size_t idx = firstIndex() + i; while (idx >= m_size) idx -= m_size; return m_array[idx]; }
        inline ValueType & first()               { return m_array[firstIndex()]; }
        inline ValueType & last()                { return m_array[m_last_idx]; }
        inline size_t size() const               { return m_size; }

    private:
        inline size_t firstIndex()               { return ((m_size == capacity) ? ((m_last_idx == (capacity - 1)) ? 0 : (m_last_idx + 1)) : 0); }

    private:
        size_t    m_size = 0;
        size_t    m_last_idx = (capacity - 1);
        ValueType m_array[capacity] = { 0 };
    };

    template<size_t windowSize>
    class Move
    {
    public:

        inline Move()                   { m_window.append(0); }

        inline Load & load()            { return m_load; }
        inline const Load & load()const { return m_load; }


        inline void increase(qint64 count)
        {
            m_load.increaseCount(count);
            m_window.last() += count;
        }

        inline void oneSecondTimer()
        {
            for (size_t i = 0; i < m_load.valuesCount(); ++i)
                updateAverage(m_load.value(i), m_load.valueRate(i));
            m_window.append(0);
        }

    private:

        inline void updateAverage(double & load, quint32 rateInSeconds)
        {
            double load_multiplier = 1.0 / rateInSeconds;
            load += m_window.last() * load_multiplier;
            if (m_window.size() >= rateInSeconds)
                load -= m_window[m_window.size() - rateInSeconds] * load_multiplier;
            if (qAbs(load) < 0.00000001)
                load = 0.0;
        }

    private:
        Load m_load;
        Window<windowSize, qint64> m_window;
    };
}

#endif // AVERAGE_MOVE_H

