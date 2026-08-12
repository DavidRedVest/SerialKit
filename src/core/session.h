#pragma once

#include "core/frame_strategy.h"
#include "core/raw_logger.h"
#include "core/ring_buffer.h"
#include "transport/itransport.h"

#include <QObject>
#include <memory>

namespace serialkit {

// Composition root for one connection: one transport + one RingBuffer + one
// active IFrameStrategy + one RawLogger. This is deliberately the unit of
// state that SessionManager multiplies when multi-session/multi-tab (M3)
// lands; M1 has exactly one Session in existence.
//
// Ownership: Session owns all four collaborators. Views do not own anything
// here; they subscribe to session->ringBuffer() and session->frameStrategy()
// signals and call session->transport() to send.
class Session : public QObject {
    Q_OBJECT
public:
    explicit Session(std::unique_ptr<ITransport> transport, QObject* parent = nullptr);
    ~Session() override;

    ITransport* transport() const { return m_transport.get(); }
    RingBuffer* ringBuffer() const { return m_ringBuffer.get(); }
    IFrameStrategy* frameStrategy() const { return m_frameStrategy.get(); }
    RawLogger* rawLogger() const { return m_rawLogger.get(); }

    // Replaces the active framing strategy. The previous strategy is
    // destroyed. Existing subscribers must reconnect to frameStrategy().
    void setFrameStrategy(std::unique_ptr<IFrameStrategy> strategy);

    QString name() const { return m_name; }
    void setName(const QString& name) { m_name = name; }

    // Writes through to the transport and, on success, emits bytesSent() so
    // TX-side consumers (HexView's coloring, future logging) can observe
    // what was sent without ITransport itself needing a "sent" concept.
    qint64 send(const QByteArray& data);

signals:
    void bytesSent(const QByteArray& data);

private:
    std::unique_ptr<ITransport> m_transport;
    std::unique_ptr<RingBuffer> m_ringBuffer;
    std::unique_ptr<IFrameStrategy> m_frameStrategy;
    std::unique_ptr<RawLogger> m_rawLogger;
    QString m_name;
};

} // namespace serialkit
