#include "core/delimiter_frame_strategy.h"

namespace serialkit {

DelimiterFrameStrategy::DelimiterFrameStrategy(QByteArray delimiter, QObject* parent)
    : IFrameStrategy(parent), m_delimiter(std::move(delimiter)) {
}

void DelimiterFrameStrategy::feed(const QByteArray& data) {
    if (m_delimiter.isEmpty()) {
        return;
    }

    m_pending.append(data);

    int splitAt;
    while ((splitAt = m_pending.indexOf(m_delimiter)) != -1) {
        const QByteArray payload = m_pending.left(splitAt);
        m_pending.remove(0, splitAt + m_delimiter.size());
        emit frameReady(Frame{payload, QDateTime::currentDateTime()});
    }
}

void DelimiterFrameStrategy::reset() {
    m_pending.clear();
}

} // namespace serialkit
