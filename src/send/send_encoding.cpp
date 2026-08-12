#include "send/send_encoding.h"

namespace serialkit {

QByteArray decodeHexString(const QString& text) {
    return QByteArray::fromHex(text.toUtf8());
}

QByteArray toRawBytes(const QString& text) {
    return text.toUtf8();
}

QByteArray appendCrlfIfRequested(QByteArray data, bool enabled) {
    if (enabled) {
        data.append("\r\n");
    }
    return data;
}

} // namespace serialkit
