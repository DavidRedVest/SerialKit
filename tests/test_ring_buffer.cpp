#include "core/ring_buffer.h"

#include <QSignalSpy>
#include <QTest>

using namespace serialkit;

class TestRingBuffer : public QObject {
    Q_OBJECT
private slots:
    void appendEmitsSignalWithFullChunkEvenWhenOverCapacity();
    void capacityEvictsOldestBytesFromSnapshot();
    void clearResetsSizeAndEmitsSignal();
    void emptyAppendIsIgnored();
};

void TestRingBuffer::appendEmitsSignalWithFullChunkEvenWhenOverCapacity() {
    RingBuffer buffer(4); // tiny capacity to force eviction below
    QSignalSpy spy(&buffer, &RingBuffer::bytesAppended);

    const QByteArray chunk = "hello world"; // 11 bytes, over capacity
    buffer.append(chunk);

    // The signal must carry the full chunk that was appended, regardless of
    // what the retained window ends up keeping (see docs/ARCHITECTURE.md
    // §1.4: streaming subscribers like RawLogger must never miss bytes).
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toByteArray(), chunk);
}

void TestRingBuffer::capacityEvictsOldestBytesFromSnapshot() {
    RingBuffer buffer(4);

    buffer.append("abcdefgh"); // 8 bytes > capacity 4
    QCOMPARE(buffer.size(), qint64(4));
    QCOMPARE(buffer.snapshot(), QByteArray("efgh"));

    buffer.append("IJ");
    QCOMPARE(buffer.size(), qint64(4));
    QCOMPARE(buffer.snapshot(), QByteArray("ghIJ"));
}

void TestRingBuffer::clearResetsSizeAndEmitsSignal() {
    RingBuffer buffer(16);
    buffer.append("data");

    QSignalSpy spy(&buffer, &RingBuffer::cleared);
    buffer.clear();

    QCOMPARE(buffer.size(), qint64(0));
    QCOMPARE(spy.count(), 1);
}

void TestRingBuffer::emptyAppendIsIgnored() {
    RingBuffer buffer(16);
    QSignalSpy spy(&buffer, &RingBuffer::bytesAppended);

    buffer.append(QByteArray());

    QCOMPARE(spy.count(), 0);
    QCOMPARE(buffer.size(), qint64(0));
}

QTEST_MAIN(TestRingBuffer)
#include "test_ring_buffer.moc"
