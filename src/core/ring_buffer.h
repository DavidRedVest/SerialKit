#pragma once

#include <QByteArray>
#include <QObject>

namespace serialkit {

// Bounded accumulator for raw bytes coming off a transport.
//
// Contract (see docs/ARCHITECTURE.md §1.4 / §2): every call to append() emits
// bytesAppended() with exactly the chunk that was passed in -- streaming
// subscribers (RawLogger, HexView) never miss a byte regardless of buffer
// capacity. Capacity only bounds the *retained* window returned by
// snapshot(), which exists for consumers that want "what's currently in the
// window" rather than a live stream (e.g. re-populating a view that attaches
// after data has already arrived). This is a pure QtCore class with no
// QWidget dependency so it can be unit-tested headlessly.
class RingBuffer : public QObject {
    Q_OBJECT
public:
    static constexpr qint64 kDefaultCapacity = 4 * 1024 * 1024; // 4 MiB

    explicit RingBuffer(qint64 capacity = kDefaultCapacity, QObject* parent = nullptr);

    void append(const QByteArray& chunk);
    void clear();

    QByteArray snapshot() const;
    qint64 size() const;
    qint64 capacity() const;

signals:
    void bytesAppended(const QByteArray& chunk);
    void cleared();

private:
    qint64 m_capacity;
    QByteArray m_data;
};

} // namespace serialkit
