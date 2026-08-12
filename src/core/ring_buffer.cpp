#include "core/ring_buffer.h"

namespace serialkit {

RingBuffer::RingBuffer(qint64 capacity, QObject* parent)
    : QObject(parent), m_capacity(capacity) {
}

void RingBuffer::append(const QByteArray& chunk) {
    if (chunk.isEmpty()) {
        return;
    }

    m_data.append(chunk);
    if (m_data.size() > m_capacity) {
        const qint64 excess = m_data.size() - m_capacity;
        m_data.remove(0, static_cast<int>(excess));
    }

    emit bytesAppended(chunk);
}

void RingBuffer::clear() {
    m_data.clear();
    emit cleared();
}

QByteArray RingBuffer::snapshot() const {
    return m_data;
}

qint64 RingBuffer::size() const {
    return m_data.size();
}

qint64 RingBuffer::capacity() const {
    return m_capacity;
}

} // namespace serialkit
