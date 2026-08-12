#include "core/passthrough_frame_strategy.h"

namespace serialkit {

void PassthroughFrameStrategy::feed(const QByteArray& data) {
    if (data.isEmpty()) {
        return;
    }
    emit frameReady(Frame{data, QDateTime::currentDateTime()});
}

} // namespace serialkit
