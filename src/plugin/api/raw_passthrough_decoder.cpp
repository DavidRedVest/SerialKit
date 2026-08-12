#include "plugin/api/raw_passthrough_decoder.h"

#include <QDateTime>

namespace serialkit {

std::optional<Frame> RawPassthroughDecoder::tryDecode(const QByteArray& buffered) {
    if (buffered.isEmpty()) {
        return std::nullopt;
    }
    return Frame{buffered, QDateTime::currentDateTime()};
}

} // namespace serialkit
