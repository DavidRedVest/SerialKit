#pragma once

#include "transport/itransport.h"

#include <QSerialPort>
#include <memory>

namespace serialkit {

// Serial implementation of ITransport, backed by QSerialPort.
//
// Recognized keys in the params passed to open():
//   "portName"    QString   required, e.g. "/dev/tty.usbserial-0001" or "COM3"
//   "baudRate"    int       optional, default 115200. Non-standard values are
//                           forwarded as-is to QSerialPort::setBaudRate(); on
//                           Linux this relies on the termios2/BOTHER path,
//                           which is known to silently no-op on some USB
//                           serial drivers (see docs/ARCHITECTURE.md risk
//                           table) -- callers should verify by read-back when
//                           using a non-standard rate.
//   "dataBits"    int       optional, one of 5/6/7/8, default 8
//   "parity"      QString   optional, one of "none"/"even"/"odd"/"mark"/"space",
//                           default "none"
//   "stopBits"    double    optional, one of 1/1.5/2, default 1
//   "flowControl" QString   optional, one of "none"/"hardware"/"software",
//                           default "none"
//
// Runs entirely on the thread it is constructed on (normally the GUI thread);
// QSerialPort's readyRead is event-loop driven and does not block the caller.
class SerialTransport : public ITransport {
    Q_OBJECT
public:
    explicit SerialTransport(QObject* parent = nullptr);
    ~SerialTransport() override;

    bool open(const QVariantMap& params) override;
    void close() override;
    bool isOpen() const override;
    qint64 write(const QByteArray& data) override;

    // Convenience for UI port pickers; not part of ITransport since it is
    // serial-specific. Runs the raw QSerialPortInfo names through
    // filterUsablePortNames() before returning them.
    static QStringList availablePortNames();

    // Pulled out of availablePortNames() so the filtering rules can be unit
    // tested with synthetic input, without depending on real hardware or
    // QSerialPortInfo. On macOS, drops the tty.* "dial-in" twin of every
    // cu.* "dial-out" device (tty.* blocks open() waiting for DCD, which is
    // never what you want for a USB-serial debugging tool) and a small set
    // of macOS-standard virtual ports (Bluetooth stack, Apple Silicon debug
    // UART) that are never a real device to talk to. No-op on other
    // platforms, where the cu/tty naming convention doesn't exist -- see
    // docs/ARCHITECTURE.md "UI 交互约定".
    static QStringList filterUsablePortNames(const QStringList& rawNames);

private slots:
    void handleReadyRead();
    void handleErrorOccurred(QSerialPort::SerialPortError error);

private:
    std::unique_ptr<QSerialPort> m_port;
};

} // namespace serialkit
