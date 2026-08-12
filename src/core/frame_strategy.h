#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QObject>

namespace serialkit {

struct Frame {
    QByteArray payload;
    QDateTime timestamp;
};

// Strategy for turning a raw byte stream into discrete frames. Implementations
// are fed incrementally via feed() and must buffer internally until a frame
// boundary is recognized. Pure QtCore, no QWidget dependency, so it is
// unit-testable headlessly (see tests/test_frame_strategy.cpp).
//
// This interface is intentionally stable across M1-M5: M1 ships
// DelimiterFrameStrategy and PassthroughFrameStrategy; M5's protocol-driven
// framing (ProtocolDrivenFrameStrategy, backed by IProtocolDecoder) is a new
// implementation of the same interface, not a signature change.
class IFrameStrategy : public QObject {
    Q_OBJECT
public:
    explicit IFrameStrategy(QObject* parent = nullptr) : QObject(parent) {}
    ~IFrameStrategy() override = default;

    virtual void feed(const QByteArray& data) = 0;

    // Discards any partially-buffered frame. Callers should call this when a
    // session is disconnected/reconnected to avoid stitching bytes from two
    // connections together into one frame.
    virtual void reset() = 0;

signals:
    void frameReady(const serialkit::Frame& frame);
};

} // namespace serialkit

Q_DECLARE_METATYPE(serialkit::Frame)
