#include "core/raw_logger.h"
#include "core/ring_buffer.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace serialkit;

class TestRawLogger : public QObject {
    Q_OBJECT
private slots:
    void startOpensFileAndIsLoggingReflectsState();
    void attachedRingBufferBytesAreWrittenAsTimestampedHex();
    void stopClosesFileAndFurtherBytesAreNotWritten();
    void startWithUnwritablePathReturnsFalse();
};

void TestRawLogger::startOpensFileAndIsLoggingReflectsState() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("log.txt");

    RawLogger logger;
    QVERIFY(!logger.isLogging());
    QVERIFY(logger.start(path));
    QVERIFY(logger.isLogging());
    QVERIFY(QFile::exists(path));
}

void TestRawLogger::attachedRingBufferBytesAreWrittenAsTimestampedHex() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("log.txt");

    RingBuffer ringBuffer;
    RawLogger logger;
    logger.attachTo(&ringBuffer);
    QVERIFY(logger.start(path));

    ringBuffer.append(QByteArray("Hi"));
    logger.stop(); // flush/close before reading back

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QByteArray contents = file.readAll();
    // One line, starting with a bracketed timestamp, containing the
    // space-separated hex of "Hi" (0x48 0x69).
    QVERIFY(contents.startsWith('['));
    QVERIFY(contents.contains("48 69"));
}

void TestRawLogger::stopClosesFileAndFurtherBytesAreNotWritten() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("log.txt");

    RingBuffer ringBuffer;
    RawLogger logger;
    logger.attachTo(&ringBuffer);
    QVERIFY(logger.start(path));
    logger.stop();
    QVERIFY(!logger.isLogging());

    // Bytes arriving after stop() must not reopen or reappend -- the
    // logger should simply be inert until start() is called again.
    ringBuffer.append(QByteArray("ignored"));

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    QCOMPARE(file.readAll(), QByteArray());
}

void TestRawLogger::startWithUnwritablePathReturnsFalse() {
    RawLogger logger;
    // Parent directory does not exist, so QFile::open() must fail.
    QVERIFY(!logger.start(QStringLiteral("/no/such/directory/log.txt")));
    QVERIFY(!logger.isLogging());
}

QTEST_MAIN(TestRawLogger)
#include "test_raw_logger.moc"
