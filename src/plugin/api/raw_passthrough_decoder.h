#pragma once

#include "plugin/api/iprotocol_decoder.h"

namespace serialkit {

// Trivial IProtocolDecoder that treats the entire buffered chunk as one
// frame. This is the only decoder that ships before M5; it exists so
// ProtocolDrivenFrameStrategy (M5) has a default to fall back to and so the
// interface is exercised (and testable) starting in M1.
class RawPassthroughDecoder : public IProtocolDecoder {
public:
    QString name() const override { return QStringLiteral("raw-passthrough"); }
    std::optional<Frame> tryDecode(const QByteArray& buffered) override;
};

} // namespace serialkit
