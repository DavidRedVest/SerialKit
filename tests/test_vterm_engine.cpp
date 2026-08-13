#include "terminal/vterm_engine.h"

#include <QSignalSpy>
#include <QTest>

using namespace serialkit;

class TestVtermEngine : public QObject {
    Q_OBJECT
private slots:
    void feedsAndRendersPlainText();
    void handlesSgrForegroundColor();
    void handlesCursorPositioning();
    void resetClearsScreenAndScrollback();
    void scrollbackAccumulatesOnScroll();
};

void TestVtermEngine::feedsAndRendersPlainText() {
    VtermEngine engine(5, 20);
    engine.feed(QByteArrayLiteral("Hello"));

    QCOMPARE(engine.cellAt(0, 0).text, QStringLiteral("H"));
    QCOMPARE(engine.cellAt(0, 1).text, QStringLiteral("e"));
    QCOMPARE(engine.cellAt(0, 4).text, QStringLiteral("o"));
    // Untouched cells stay blank, not garbage.
    QVERIFY(engine.cellAt(0, 5).text.isEmpty());
}

void TestVtermEngine::handlesSgrForegroundColor() {
    VtermEngine engine(5, 20);
    // ESC[31m = set foreground red, ESC[0m = reset.
    engine.feed(QByteArrayLiteral("\x1b[31mRed\x1b[0mX"));

    const TerminalCell red = engine.cellAt(0, 0);
    QCOMPARE(red.text, QStringLiteral("R"));
    // libvterm's default ANSI palette uses a conventional muted red (not
    // necessarily pure 255,0,0 -- that's a terminal-emulator styling
    // choice), so assert "clearly red" rather than an exact RGB match.
    QVERIFY(red.foreground.red() > 150);
    QCOMPARE(red.foreground.green(), 0);
    QCOMPARE(red.foreground.blue(), 0);

    // After the reset, the following character should not still be red.
    const TerminalCell afterReset = engine.cellAt(0, 3);
    QCOMPARE(afterReset.text, QStringLiteral("X"));
    QVERIFY(afterReset.foreground != red.foreground);
}

void TestVtermEngine::handlesCursorPositioning() {
    VtermEngine engine(10, 20);
    QSignalSpy moved(&engine, &VtermEngine::cursorMoved);

    // CUP: move cursor to row 5, col 10 (1-indexed in the escape sequence).
    engine.feed(QByteArrayLiteral("\x1b[5;10H"));

    QVERIFY(moved.count() >= 1);
    QCOMPARE(engine.cursorPosition(), QPoint(9, 4)); // 0-indexed col, row
}

void TestVtermEngine::resetClearsScreenAndScrollback() {
    VtermEngine engine(2, 10);
    // Push more lines than the screen holds so something lands in
    // scrollback, then reset and confirm both screen and scrollback clear.
    engine.feed(QByteArrayLiteral("one\r\ntwo\r\nthree\r\nfour\r\n"));
    QVERIFY(engine.scrollbackLineCount() > 0);

    engine.reset();

    QCOMPARE(engine.scrollbackLineCount(), 0);
    QVERIFY(engine.cellAt(0, 0).text.isEmpty());
}

void TestVtermEngine::scrollbackAccumulatesOnScroll() {
    VtermEngine engine(2, 10);
    QSignalSpy scrollbackChanged(&engine, &VtermEngine::scrollbackChanged);

    engine.feed(QByteArrayLiteral("one\r\ntwo\r\nthree\r\n"));

    QVERIFY(scrollbackChanged.count() >= 1);
    QVERIFY(engine.scrollbackLineCount() >= 1);
    // The line scrolled off the top should be retrievable, not empty.
    const QList<TerminalCell> line = engine.scrollbackLine(0);
    QVERIFY(!line.isEmpty());
}

QTEST_MAIN(TestVtermEngine)
#include "test_vterm_engine.moc"
