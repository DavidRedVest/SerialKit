#include "transport/serial_transport.h"

#include <QTest>

using namespace serialkit;

class TestSerialTransport : public QObject {
    Q_OBJECT
private slots:
    void filterUsablePortNames_hidesTtyTwinsAndSystemVirtualPortsOnMac();
};

void TestSerialTransport::filterUsablePortNames_hidesTtyTwinsAndSystemVirtualPortsOnMac() {
    const QStringList raw = {
        QStringLiteral("cu.usbserial-11410"),
        QStringLiteral("tty.usbserial-11410"),
        QStringLiteral("cu.debug-console"),
        QStringLiteral("tty.debug-console"),
        QStringLiteral("cu.Bluetooth-Incoming-Port"),
        QStringLiteral("tty.Bluetooth-Incoming-Port"),
    };

#ifdef Q_OS_MACOS
    QCOMPARE(SerialTransport::filterUsablePortNames(raw),
             QStringList{QStringLiteral("cu.usbserial-11410")});
#else
    // The cu/tty naming convention is macOS/BSD-specific; on other
    // platforms the filter is a no-op passthrough.
    QCOMPARE(SerialTransport::filterUsablePortNames(raw), raw);
#endif
}

QTEST_MAIN(TestSerialTransport)
#include "test_serial_transport.moc"
