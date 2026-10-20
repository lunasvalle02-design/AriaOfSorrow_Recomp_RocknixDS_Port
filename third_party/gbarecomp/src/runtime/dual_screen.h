// dual_screen.h — presentation of one game window across two physical panels.
//
// Dual-screen handhelds running a Wayland compositor (ROCKNIX on the Anbernic
// RG DS / RG DS Plus uses sway) expose each panel as its own output. The
// simplest robust model is one borderless window covering the bounding box of
// both outputs: the game frame is drawn into the "game" panel's rectangle and
// the host's auxiliary UI (map, status, menus) into the other one.
//
// Panel rectangles come from, in order of preference:
//   1. ARIA_DS_GAME_RECT / ARIA_DS_AUX_RECT = "x,y,w,h" (window pixels),
//   2. sway's output layout (`swaymsg -t get_outputs`), with the game panel
//      named by ARIA_DS_GAME_OUTPUT (default DSI-2, the RG DS top panel; DSI-1 is the touch panel),
//   3. a geometric split of the drawable (side by side or stacked halves).
// `swap` exchanges the two panels at presentation time.

#pragma once

namespace gbarecomp::dual_screen {

struct Rect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    bool empty() const { return w <= 0 || h <= 0; }
};

enum class Scaling : int {
    Smooth = 0,   // integer prescale, then a short linear pass to fill
    Nearest = 1,  // nearest neighbour to fill (uneven pixel sizes)
    Integer = 2,  // largest whole multiple, centered
};

struct Config {
    bool enabled = false;
    bool swap = false;
    Scaling scaling = Scaling::Smooth;
};

// Live configuration; safe to change at any time from the UI thread.
void set_config(const Config& cfg);
Config config();

// Compositor integration. Called once after the window exists: probes the
// output layout and, under sway, asks the compositor to make this process's
// window a borderless floating window covering every output.
void attach_to_compositor();

// Suggested initial window size when the caller has nothing better.
void preferred_window_size(int* w, int* h);

// Resolves both panel rectangles for a drawable of the given size (pixels).
// Returns false when dual-screen presentation is disabled.
bool layout(int drawable_w, int drawable_h, Rect* game, Rect* aux);

// The last layout() result, in drawable pixels, for UI code that runs later
// in the same frame (e.g. an ImGui pass drawing the auxiliary screen).
bool last_layout(Rect* game, Rect* aux);

}  // namespace gbarecomp::dual_screen

// C entry points for game code that should not include runtime headers.
extern "C" int gbarecomp_dual_screen_aux_rect(int* x, int* y, int* w, int* h);
extern "C" int gbarecomp_dual_screen_game_rect(int* x, int* y, int* w, int* h);
