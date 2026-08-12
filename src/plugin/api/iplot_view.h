#pragma once

#include <QtGlobal>

namespace serialkit {

// Placeholder interface for the M5 waveform view. Defined now so the M5
// implementation slots in without touching callers, but deliberately has NO
// implementation anywhere in this repo before M5.
//
// IMPORTANT (see docs/ARCHITECTURE.md §1.1 and CLAUDE.md): the eventual
// implementation is expected to depend on QCustomPlot, which is
// GPL-3.0-or-later. This project targets a closed-source/commercial release,
// so QCustomPlot (or any GPL dependency) must only ever be referenced from
// the concrete IPlotView implementation file(s), never from src/core,
// src/transport, or src/send. Do not add an implementation of this interface
// as part of M1-M4 work.
class IPlotView {
public:
    virtual ~IPlotView() = default;
    virtual void pushSample(qint64 timestampMs, double value) = 0;
};

} // namespace serialkit
