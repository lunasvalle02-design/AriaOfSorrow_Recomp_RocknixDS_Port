#pragma once

// Second-screen UI for the dual-screen handheld port: live castle map, the
// player status that replaces the original HUD, and the port's own menu. It
// is drawn with Dear ImGui into the auxiliary panel's rectangle and is fully
// usable by touch (only the RG DS Plus bottom panel has a touch layer).

#include <cstddef>
#include <cstdint>

namespace aria::handheld {

// Immutable ROM data the map needs (room table) — read once at startup.
void SetRomData(const uint8_t* rom, size_t romSize);

// Pushes the [Handheld] config to the runtime (dual-screen layout, scaling)
// and to the 4:3 view policy (HUD hiding). Call after any config change.
void ApplyConfig();

// ImGui pass, once per presented frame, on the host thread. `ewram` is the
// live guest EWRAM (read only).
void RenderAuxScreen(const uint8_t* ewram, size_t ewramSize, uint64_t frame);

}  // namespace aria::handheld
