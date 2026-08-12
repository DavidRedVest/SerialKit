#include "send/send_encoding.h"

#include <QTest>

using namespace serialkit;

class TestSendEncoding : public QObject {
    Q_OBJECT
private slots:
    void decodeHexStringIgnoresWhitespaceAndNewlines();
    void decodeHexStringZeroPadsOddTrailingNibble();
    void toRawBytesPassesTextThroughWithoutEscapeInterpretation();
    void appendCrlfIfRequestedAppendsOnlyWhenEnabled();
};

void TestSendEncoding::decodeHexStringIgnoresWhitespaceAndNewlines() {
    QCOMPARE(decodeHexString("48 65 6C 6C 6F"), QByteArray("Hello"));
    QCOMPARE(decodeHexString("48656C6C6F"), QByteArray("Hello"));
    QCOMPARE(decodeHexString("48 65\n6C 6C\n6F"), QByteArray("Hello"));
}

void TestSendEncoding::decodeHexStringZeroPadsOddTrailingNibble() {
    // "4865 6" -> whitespace stripped -> "48656", an odd digit count, which
    // QByteArray::fromHex treats as "048656" (leading zero), not "He" with
    // the trailing nibble dropped.
    QCOMPARE(decodeHexString("4865 6"), QByteArray::fromHex("048656"));
}

void TestSendEncoding::toRawBytesPassesTextThroughWithoutEscapeInterpretation() {
    // No escape-sequence parsing: a literal backslash-r-backslash-n stays as
    // those four characters, it is not turned into a CR/LF byte pair. See
    // docs/ARCHITECTURE.md "UI 交互约定" -- "what you see is what gets sent".
    QCOMPARE(toRawBytes("Hello\\r\\n"), QByteArray("Hello\\r\\n"));
    QCOMPARE(toRawBytes("Hello"), QByteArray("Hello"));
}

void TestSendEncoding::appendCrlfIfRequestedAppendsOnlyWhenEnabled() {
    QCOMPARE(appendCrlfIfRequested(QByteArray("Hello"), true), QByteArray("Hello\r\n"));
    QCOMPARE(appendCrlfIfRequested(QByteArray("Hello"), false), QByteArray("Hello"));
}

QTEST_MAIN(TestSendEncoding)
#include "test_send_encoding.moc"
