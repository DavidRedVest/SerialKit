#pragma once

#include "core/session.h"

#include <QObject>
#include <memory>
#include <vector>

namespace serialkit {

// Owns the set of live Sessions. M1 keeps this list at length 0 or 1 (no
// tabbed UI yet); M3 lifts that restriction to support multiple simultaneous
// connections without changing Session's internals (see
// docs/ARCHITECTURE.md §1.5).
class SessionManager : public QObject {
    Q_OBJECT
public:
    explicit SessionManager(QObject* parent = nullptr);

    // Takes ownership of `session`, appends it, and returns a non-owning
    // pointer. Emits sessionAdded().
    Session* addSession(std::unique_ptr<Session> session);

    // Destroys the session at `index`, if present. Emits sessionRemoved().
    void removeSession(int index);

    Session* sessionAt(int index) const;
    int count() const { return static_cast<int>(m_sessions.size()); }

signals:
    void sessionAdded(Session* session);
    void sessionRemoved(int index);

private:
    std::vector<std::unique_ptr<Session>> m_sessions;
};

} // namespace serialkit
