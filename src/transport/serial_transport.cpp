#include "transport/serial_transport.h"

#include <QSerialPortInfo>
#include <QSet>

namespace serialkit {

namespace {

QSerialPort::DataBits toDataBits(int bits) {
    switch (bits) {
        case 5: return QSerialPort::Data5;
        case 6: return QSerialPort::Data6;
        case 7: return QSerialPort::Data7;
        default: return QSerialPort::Data8;
    }
}

QSerialPort::Parity toParity(const QString& parity) {
    const QString p = parity.toLower();
    if (p == "even") return QSerialPort::EvenParity;
    if (p == "odd") return QSerialPort::OddParity;
    if (p == "mark") return QSerialPort::MarkParity;
    if (p == "space") return QSerialPort::SpaceParity;
    return QSerialPort::NoParity;
}

QSerialPort::StopBits toStopBits(double bits) {
    if (bits >= 2.0) return QSerialPort::TwoStop;
    if (bits >= 1.5) return QSerialPort::OneAndHalfStop;
    return QSerialPort::OneStop;
}

QSerialPort::FlowControl toFlowControl(const QString& flow) {
    const QString f = flow.toLower();
    if (f == "hardware") return QSerialPort::HardwareControl;
    if (f == "software") return QSerialPort::SoftwareControl;
    return QSerialPort::NoFlowControl;
}

} // namespace

SerialTransport::SerialTransport(QObject* parent)
    : ITransport(parent), m_port(std::make_unique<QSerialPort>(this)) {
    connect(m_port.get(), &QSerialPort::readyRead, this, &SerialTransport::handleReadyRead);
    connect(m_port.get(), &QSerialPort::errorOccurred, this, &SerialTransport::handleErrorOccurred);
}

SerialTransport::~SerialTransport() {
    close();
}

bool SerialTransport::open(const QVariantMap& params) {
    if (isOpen()) {
        close();
    }

    const QString portName = params.value("portName").toString();
    if (portName.isEmpty()) {
        emit errorOccurred(tr("No port name specified"));
        return false;
    }

    m_port->setPortName(portName);
    m_port->setBaudRate(params.value("baudRate", 115200).toInt());
    m_port->setDataBits(toDataBits(params.value("dataBits", 8).toInt()));
    m_port->setParity(toParity(params.value("parity", "none").toString()));
    m_port->setStopBits(toStopBits(params.value("stopBits", 1.0).toDouble()));
    m_port->setFlowControl(toFlowControl(params.value("flowControl", "none").toString()));

    if (!m_port->open(QIODevice::ReadWrite)) {
        emit errorOccurred(m_port->errorString());
        return false;
    }

    // Discard whatever the OS driver's buffer already had queued (electrical
    // noise while the port sat closed, or leftovers from a previous
    // session) so the first bytes shown are genuinely new, not stale.
    m_port->clear(QSerialPort::AllDirections);

    emit openedChanged(true);
    return true;
}

void SerialTransport::close() {
    if (!isOpen()) {
        return;
    }
    m_port->close();
    emit openedChanged(false);
}

bool SerialTransport::isOpen() const {
    return m_port->isOpen();
}

qint64 SerialTransport::write(const QByteArray& data) {
    if (!isOpen()) {
        emit errorOccurred(tr("Cannot write: port is not open"));
        return -1;
    }
    return m_port->write(data);
}

#ifdef Q_OS_MACOS
namespace {
const QSet<QString>& hiddenMacPortNames() {
    static const QSet<QString> names = {
        QStringLiteral("cu.debug-console"),
        QStringLiteral("cu.Bluetooth-Incoming-Port"),
    };
    return names;
}
} // namespace
#endif

QStringList SerialTransport::filterUsablePortNames(const QStringList& rawNames) {
#ifdef Q_OS_MACOS
    QStringList result;
    result.reserve(rawNames.size());
    for (const QString& name : rawNames) {
        if (name.startsWith(QLatin1String("tty."))) {
            continue;
        }
        if (hiddenMacPortNames().contains(name)) {
            continue;
        }
        result.append(name);
    }
    return result;
#else
    return rawNames;
#endif
}

QStringList SerialTransport::availablePortNames() {
    QStringList names;
    const auto infos = QSerialPortInfo::availablePorts();
    names.reserve(infos.size());
    for (const auto& info : infos) {
        names.append(info.portName());
    }
    return filterUsablePortNames(names);
}

void SerialTransport::handleReadyRead() {
    emit bytesReceived(m_port->readAll());
}

void SerialTransport::handleErrorOccurred(QSerialPort::SerialPortError error) {
    if (error == QSerialPort::NoError) {
        return;
    }
    emit errorOccurred(m_port->errorString());
}

} // namespace serialkit
