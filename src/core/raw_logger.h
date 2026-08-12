#pragma once

#include <QFile>
#include <QObject>
#include <memory>

namespace serialkit {

class RingBuffer;

// Subscribes to a RingBuffer's raw byte stream and appends every chunk to a
// log file, each prefixed with a millisecond timestamp. Always attached to
// the RingBuffer directly (see docs/ARCHITECTURE.md §1.4) so it captures
// everything regardless of what any view is currently able to render.
class RawLogger : public QObject {
    Q_OBJECT
public:
    explicit RawLogger(QObject* parent = nullptr);
    ~RawLogger() override;

    // Opens `path` for appending and starts logging. Returns false if the
    // file could not be opened.
    bool start(const QString& path);
    void stop();
    bool isLogging() const;

    // Connects/disconnects this logger to a RingBuffer's bytesAppended
    // signal. Passing nullptr detaches from the current source, if any.
    void attachTo(RingBuffer* source);

private slots:
    void handleBytesAppended(const QByteArray& chunk);

private:
    std::unique_ptr<QFile> m_file;
    RingBuffer* m_source = nullptr;
};

} // namespace serialkit
