#include "core/session.h"

#include "core/passthrough_frame_strategy.h"

namespace serialkit {

Session::Session(std::unique_ptr<ITransport> transport, QObject* parent)
    : QObject(parent),
      m_transport(std::move(transport)),
      m_ringBuffer(std::make_unique<RingBuffer>()),
      m_frameStrategy(std::make_unique<PassthroughFrameStrategy>()),
      m_rawLogger(std::make_unique<RawLogger>()) {
    connect(m_transport.get(), &ITransport::bytesReceived, m_ringBuffer.get(), &RingBuffer::append);
    connect(m_ringBuffer.get(), &RingBuffer::bytesAppended, m_frameStrategy.get(),
            &IFrameStrategy::feed);
    m_rawLogger->attachTo(m_ringBuffer.get());
}

Session::~Session() = default;

qint64 Session::send(const QByteArray& data) {
    const qint64 written = m_transport->write(data);
    if (written > 0) {
        emit bytesSent(written == data.size() ? data : data.left(static_cast<int>(written)));
    }
    return written;
}

void Session::setFrameStrategy(std::unique_ptr<IFrameStrategy> strategy) {
    disconnect(m_ringBuffer.get(), &RingBuffer::bytesAppended, m_frameStrategy.get(),
               &IFrameStrategy::feed);
    m_frameStrategy = std::move(strategy);
    connect(m_ringBuffer.get(), &RingBuffer::bytesAppended, m_frameStrategy.get(),
            &IFrameStrategy::feed);
}

} // namespace serialkit
