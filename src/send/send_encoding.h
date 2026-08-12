#pragma once

#include <QByteArray>
#include <QString>

namespace serialkit {

// Decodes a hex string like "48 65 6C 6C 6F" or "48656C6C6F" into bytes.
// Whitespace and any non-hex-digit characters (including newlines, so a
// multi-line paste works) are ignored. Thin wrapper around
// QByteArray::fromHex, which zero-pads an odd trailing nibble rather than
// dropping it (e.g. "4865 6" decodes as if it were "048656").
QByteArray decodeHexString(const QString& text);

// Encodes text to bytes exactly as typed (UTF-8), with no escape-sequence
// interpretation: what you see in the box is what gets sent. Combined with
// appendCrlfIfRequested() below for the optional "+CRLF" checkbox instead of
// requiring users to type \r\n by hand.
QByteArray toRawBytes(const QString& text);

// Appends a literal "\r\n" to `data` when `enabled` is true; returns `data`
// unchanged otherwise. Kept as a separate step (rather than baked into
// toRawBytes/decodeHexString) so both the HEX and text send paths share the
// same "+CRLF" checkbox behavior.
QByteArray appendCrlfIfRequested(QByteArray data, bool enabled);

} // namespace serialkit
