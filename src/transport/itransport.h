#pragma once

#include <QByteArray>
#include <QObject>
#include <QVariantMap>

namespace serialkit {

// Abstraction over a byte-stream transport (serial today; TCP/UDP/SSH later).
// Implementations must be non-blocking: open()/write() return immediately and
// results are reported via the signals below. Do not give a transport its own
// QThread — see docs/ARCHITECTURE.md §1.3 for why that is unnecessary for M1-M3.
class ITransport : public QObject {
    Q_OBJECT
public:
    explicit ITransport(QObject* parent = nullptr) : QObject(parent) {}
    ~ITransport() override = default;

    // params are transport-specific (e.g. port name, baud rate for serial).
    virtual bool open(const QVariantMap& params) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;

    // Returns the number of bytes queued for writing, or -1 on error.
    virtual qint64 write(const QByteArray& data) = 0;

signals:
    void bytesReceived(const QByteArray& data);
    void errorOccurred(const QString& message);
    void openedChanged(bool opened);
};

} // namespace serialkit
