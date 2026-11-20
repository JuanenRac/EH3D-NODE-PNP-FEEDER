// EH3D-NODE-PNP-FEEDER - core/version.hpp
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
// The version of the firmware. The firmware build and the host tests define PNP_FEEDER_VERSION from eh3d.project.json, the one place that says it;
// a test of the repository fails when the fallback below is not that version.
#pragma once

#ifndef PNP_FEEDER_VERSION
#define PNP_FEEDER_VERSION "0.0.1"
#endif

namespace pnp_feeder {
constexpr const char* kVersion = PNP_FEEDER_VERSION;
}
