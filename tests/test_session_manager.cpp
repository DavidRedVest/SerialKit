#include "core/session_manager.h"
#include "transport/serial_transport.h"

#include <QSignalSpy>
#include <QTest>

using namespace serialkit;

namespace {
std::unique_ptr<Session> makeSession() {
    return std::make_unique<Session>(std::make_unique<SerialTransport>());
}
} // namespace

class TestSessionManager : public QObject {
    Q_OBJECT
private slots:
    void addSessionIncrementsCountAndReturnsTheStoredPointer();
    void removeSessionByIndexErasesAndEmitsSessionRemoved();
    void removeSessionByPointerErasesTheRightOneRegardlessOfPosition();
    void removeSessionByPointerIsNoOpForAnUnknownOrNullPointer();
};

void TestSessionManager::addSessionIncrementsCountAndReturnsTheStoredPointer() {
    SessionManager manager;
    QSignalSpy addedSpy(&manager, &SessionManager::sessionAdded);

    Session* session = manager.addSession(makeSession());

    QCOMPARE(manager.count(), 1);
    QCOMPARE(manager.sessionAt(0), session);
    QCOMPARE(addedSpy.count(), 1);
    QCOMPARE(addedSpy.at(0).at(0).value<Session*>(), session);
}

void TestSessionManager::removeSessionByIndexErasesAndEmitsSessionRemoved() {
    SessionManager manager;
    manager.addSession(makeSession());
    QSignalSpy removedSpy(&manager, &SessionManager::sessionRemoved);

    manager.removeSession(0);

    QCOMPARE(manager.count(), 0);
    QCOMPARE(removedSpy.count(), 1);
    QCOMPARE(removedSpy.at(0).at(0).toInt(), 0);

    // Out-of-range index must be a no-op, not a crash.
    manager.removeSession(0);
    manager.removeSession(-1);
    QCOMPARE(manager.count(), 0);
}

void TestSessionManager::removeSessionByPointerErasesTheRightOneRegardlessOfPosition() {
    SessionManager manager;
    Session* first = manager.addSession(makeSession());
    Session* middle = manager.addSession(makeSession());
    Session* last = manager.addSession(makeSession());

    // SessionPanel doesn't track its own index into SessionManager's
    // internal vector (it drifts as sibling tabs close), so removal is
    // always by pointer -- this is the scenario that motivated adding this
    // overload for M3.
    manager.removeSession(middle);

    QCOMPARE(manager.count(), 2);
    QCOMPARE(manager.sessionAt(0), first);
    QCOMPARE(manager.sessionAt(1), last);
}

void TestSessionManager::removeSessionByPointerIsNoOpForAnUnknownOrNullPointer() {
    SessionManager manager;
    manager.addSession(makeSession());
    QSignalSpy removedSpy(&manager, &SessionManager::sessionRemoved);

    manager.removeSession(static_cast<Session*>(nullptr));
    Session unrelated(std::make_unique<SerialTransport>());
    manager.removeSession(&unrelated);

    QCOMPARE(manager.count(), 1);
    QCOMPARE(removedSpy.count(), 0);
}

QTEST_MAIN(TestSessionManager)
#include "test_session_manager.moc"
