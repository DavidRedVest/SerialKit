#include "core/raw_logger.h"

#include "core/ring_buffer.h"

#include <QDateTime>

namespace serialkit {

RawLogger::RawLogger(QObject* parent) : QObject(parent) {
}

RawLogger::~RawLogger() {
    stop();
    attachTo(nullptr);
}

bool RawLogger::start(const QString& path) {
    stop();
    m_file = std::make_unique<QFile>(path);
    if (!m_file->open(QIODevice::Append | QIODevice::Text)) {
        m_file.reset();
        return false;
    }
    return true;
}

void RawLogger::stop() {
    if (m_file) {
        m_file->close();
        m_file.reset();
    }
}

bool RawLogger::isLogging() const {
    return m_file != nullptr;
}

void RawLogger::attachTo(RingBuffer* source) {
    if (m_source == source) {
        return;
    }
    if (m_source) {
        disconnect(m_source, nullptr, this, nullptr);
    }
    m_source = source;
    if (m_source) {
        connect(m_source, &RingBuffer::bytesAppended, this, &RawLogger::handleBytesAppended);
    }
}

void RawLogger::handleBytesAppended(const QByteArray& chunk) {
    if (!m_file) {
        return;
    }
    const QByteArray timestamp =
        QDateTime::currentDateTime().toString("[yyyy-MM-dd HH:mm:ss.zzz] ").toUtf8();
    m_file->write(timestamp);
    m_file->write(chunk.toHex(' '));
    m_file->write("\n");
    m_file->flush();
}

} // namespace serialkit
