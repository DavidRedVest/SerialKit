#pragma once

#include "core/session.h"

#include <QObject>
#include <memory>
#include <vector>

namespace serialkit {

// Owns the set of live Sessions. M1-M2 kept this list at length 0 or 1 (no
// tabbed UI yet); M3 lifts that restriction -- MainWindow's outer session
// QTabWidget can add/remove SessionPanel tabs freely, without changing
// Session's internals (see docs/ARCHITECTURE.md §1.5).
class SessionManager : public QObject {
    Q_OBJECT
public:
    explicit SessionManager(QObject* parent = nullptr);

    // Takes ownership of `session`, appends it, and returns a non-owning
    // pointer. Emits sessionAdded().
    Session* addSession(std::unique_ptr<Session> session);

    // Destroys the session at `index`, if present. Emits sessionRemoved().
    void removeSession(int index);

    // Destroys `session`, if it is owned by this manager (no-op otherwise).
    // Callers that don't track their session's index into this manager's
    // internal vector -- e.g. a SessionPanel, whose index can drift as
    // sibling tabs are added/removed -- should use this instead of
    // removeSession(int) to avoid any index bookkeeping.
    void removeSession(Session* session);

    Session* sessionAt(int index) const;
    int count() const { return static_cast<int>(m_sessions.size()); }

signals:
    void sessionAdded(Session* session);
    void sessionRemoved(int index);

private:
    std::vector<std::unique_ptr<Session>> m_sessions;
};

} // namespace serialkit
