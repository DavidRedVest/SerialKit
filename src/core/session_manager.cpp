#include "core/session_manager.h"

#include <algorithm>

namespace serialkit {

SessionManager::SessionManager(QObject* parent) : QObject(parent) {
}

Session* SessionManager::addSession(std::unique_ptr<Session> session) {
    session->setParent(this);
    Session* raw = session.get();
    m_sessions.push_back(std::move(session));
    emit sessionAdded(raw);
    return raw;
}

void SessionManager::removeSession(int index) {
    if (index < 0 || index >= static_cast<int>(m_sessions.size())) {
        return;
    }
    m_sessions.erase(m_sessions.begin() + index);
    emit sessionRemoved(index);
}

void SessionManager::removeSession(Session* session) {
    const auto it = std::find_if(m_sessions.begin(), m_sessions.end(),
                                  [session](const std::unique_ptr<Session>& s) { return s.get() == session; });
    if (it == m_sessions.end()) {
        return;
    }
    const int index = static_cast<int>(std::distance(m_sessions.begin(), it));
    m_sessions.erase(it);
    emit sessionRemoved(index);
}

Session* SessionManager::sessionAt(int index) const {
    if (index < 0 || index >= static_cast<int>(m_sessions.size())) {
        return nullptr;
    }
    return m_sessions[index].get();
}

} // namespace serialkit
