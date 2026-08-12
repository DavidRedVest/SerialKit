#pragma once

#include "core/frame_strategy.h"

#include <QByteArray>
#include <QString>
#include <optional>

namespace serialkit {

// Placeholder interface for M5 protocol-decoder plugins (e.g. Modbus RTU).
// Defined now (M1) so IFrameStrategy's "protocol-driven" mode has a stable
// contract to build against later without changing IFrameStrategy's
// signature. Until M5, the only implementation is RawPassthroughDecoder.
//
// A decoder is handed the currently buffered, not-yet-framed bytes and
// either recognizes a complete frame at the start of the buffer (returning
// it) or returns nullopt to mean "not enough data yet". It does not own the
// buffer; the caller (a future ProtocolDrivenFrameStrategy) is responsible
// for removing consumed bytes.
class IProtocolDecoder {
public:
    virtual ~IProtocolDecoder() = default;

    virtual QString name() const = 0;

    // Returns the decoded frame and implicitly how many bytes it consumed
    // (via frame.payload.size() plus any protocol overhead the caller knows
    // to skip); returns std::nullopt if `buffered` does not yet contain a
    // complete frame.
    virtual std::optional<Frame> tryDecode(const QByteArray& buffered) = 0;
};

} // namespace serialkit
