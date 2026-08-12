#include "core/delimiter_frame_strategy.h"
#include "core/passthrough_frame_strategy.h"

#include <QSignalSpy>
#include <QTest>

using namespace serialkit;

class TestFrameStrategy : public QObject {
    Q_OBJECT
private slots:
    void delimiterSplitsCompleteFrames();
    void delimiterHoldsPartialDataUntilDelimiterArrives();
    void delimiterHandlesDelimiterSplitAcrossMultipleFeeds();
    void delimiterResetDiscardsPendingBytes();
    void passthroughEmitsEachFeedAsOneFrame();
    void passthroughIgnoresEmptyFeed();
};

void TestFrameStrategy::delimiterSplitsCompleteFrames() {
    DelimiterFrameStrategy strategy(QByteArray("\r\n"));
    QSignalSpy spy(&strategy, &DelimiterFrameStrategy::frameReady);

    strategy.feed("AT+OK\r\nAT+ERROR\r\n");

    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(0).at(0).value<Frame>().payload, QByteArray("AT+OK"));
    QCOMPARE(spy.at(1).at(0).value<Frame>().payload, QByteArray("AT+ERROR"));
}

void TestFrameStrategy::delimiterHoldsPartialDataUntilDelimiterArrives() {
    DelimiterFrameStrategy strategy(QByteArray("\r\n"));
    QSignalSpy spy(&strategy, &DelimiterFrameStrategy::frameReady);

    strategy.feed("partial-frame-no-terminator");

    QCOMPARE(spy.count(), 0);
}

void TestFrameStrategy::delimiterHandlesDelimiterSplitAcrossMultipleFeeds() {
    DelimiterFrameStrategy strategy(QByteArray("\r\n"));
    QSignalSpy spy(&strategy, &DelimiterFrameStrategy::frameReady);

    // The delimiter itself is split across two feed() calls.
    strategy.feed("AT+OK\r");
    QCOMPARE(spy.count(), 0);
    strategy.feed("\n");

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).value<Frame>().payload, QByteArray("AT+OK"));
}

void TestFrameStrategy::delimiterResetDiscardsPendingBytes() {
    DelimiterFrameStrategy strategy(QByteArray("\r\n"));
    QSignalSpy spy(&strategy, &DelimiterFrameStrategy::frameReady);

    strategy.feed("half-a-fr");
    strategy.reset();
    strategy.feed("frame\r\n");

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).value<Frame>().payload, QByteArray("frame"));
}

void TestFrameStrategy::passthroughEmitsEachFeedAsOneFrame() {
    PassthroughFrameStrategy strategy;
    QSignalSpy spy(&strategy, &PassthroughFrameStrategy::frameReady);

    strategy.feed("chunk1");
    strategy.feed("chunk2");

    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(0).at(0).value<Frame>().payload, QByteArray("chunk1"));
    QCOMPARE(spy.at(1).at(0).value<Frame>().payload, QByteArray("chunk2"));
}

void TestFrameStrategy::passthroughIgnoresEmptyFeed() {
    PassthroughFrameStrategy strategy;
    QSignalSpy spy(&strategy, &PassthroughFrameStrategy::frameReady);

    strategy.feed(QByteArray());

    QCOMPARE(spy.count(), 0);
}

QTEST_MAIN(TestFrameStrategy)
#include "test_frame_strategy.moc"
