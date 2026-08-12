#pragma once

#include "core/frame_strategy.h"

namespace serialkit {

// Splits the incoming stream on a fixed delimiter (e.g. "\r\n" or a single
// byte like 0x7E). The delimiter itself is not included in the emitted
// frame payload. Bytes after the last delimiter are held back until either
// more data completes a frame, or reset() is called.
class DelimiterFrameStrategy : public IFrameStrategy {
    Q_OBJECT
public:
    explicit DelimiterFrameStrategy(QByteArray delimiter = QByteArrayLiteral("\r\n"),
                                     QObject* parent = nullptr);

    void feed(const QByteArray& data) override;
    void reset() override;

    QByteArray delimiter() const { return m_delimiter; }
    void setDelimiter(const QByteArray& delimiter) { m_delimiter = delimiter; }

private:
    QByteArray m_delimiter;
    QByteArray m_pending;
};

} // namespace serialkit
