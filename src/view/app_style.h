#pragma once

#include <QString>

namespace serialkit {

// Global QSS applied once in main.cpp via QApplication::setStyleSheet().
// Purely visual polish (spacing/color/border-radius) -- see
// docs/ARCHITECTURE.md "UI 交互约定" for why: the interaction model
// (checkboxes vs dropdowns, what's shared vs per-row) is decided elsewhere
// and this file must not encode any of that logic, only appearance.
//
// Widgets that should render as a "primary" action (Connect/Disconnect, the
// Send buttons) get objectName "primaryButton" so this stylesheet can target
// them distinctly from secondary actions (Refresh, Clear, Open File...).
QString appStyleSheet();

} // namespace serialkit
