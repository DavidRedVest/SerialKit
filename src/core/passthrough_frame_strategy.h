#pragma once

#include "core/frame_strategy.h"

namespace serialkit {

// No-op framing: every feed() call is immediately re-emitted as one frame,
// with no internal buffering. Used by consumers that want frame-shaped input
// (a timestamped Frame object) without any actual boundary detection, e.g.
// as the default strategy before the user picks delimiter/timeout/protocol
// framing.
class PassthroughFrameStrategy : public IFrameStrategy {
    Q_OBJECT
public:
    explicit PassthroughFrameStrategy(QObject* parent = nullptr) : IFrameStrategy(parent) {}

    void feed(const QByteArray& data) override;
    void reset() override {}
};

} // namespace serialkit
