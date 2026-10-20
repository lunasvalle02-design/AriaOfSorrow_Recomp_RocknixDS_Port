// dual_screen.cpp — see dual_screen.h.

#include "dual_screen.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#if !defined(_WIN32)
#include <unistd.h>
#endif

namespace gbarecomp::dual_screen {
namespace {

std::mutex g_mutex;
Config g_config;
// Panel rectangles relative to the window, in window (logical) units, as
// discovered from the compositor; empty when unknown.
Rect g_probe_game;
Rect g_probe_aux;
int g_probe_window_w = 0;
int g_probe_window_h = 0;
Rect g_last_game;
Rect g_last_aux;
bool g_last_valid = false;
// sway output names of the two panels (empty without sway) and whether the
// touchscreen mapping has been applied.
std::string g_game_output;
std::string g_aux_output;
bool g_touch_mapped = false;

bool parse_rect(const char* text, Rect* out) {
    if (!text || !*text) return false;
    Rect r;
    if (std::sscanf(text, "%d,%d,%d,%d", &r.x, &r.y, &r.w, &r.h) != 4 || r.empty())
        return false;
    *out = r;
    return true;
}

struct SwayOutput {
    std::string name;
    bool active = true;
    Rect rect;
};

// Minimal reader for `swaymsg -t get_outputs -r`: an array of objects whose
// top-level members include "name", "active" and "rect":{x,y,width,height}.
// Only depth-1 members of each output object are inspected.
std::vector<SwayOutput> parse_outputs(const std::string& json) {
    std::vector<SwayOutput> outputs;
    int depth = 0;
    bool in_string = false;
    SwayOutput current;
    std::string key;
    std::string str;
    bool reading_key = false;
    bool expecting_value = false;
    for (size_t i = 0; i < json.size(); ++i) {
        const char c = json[i];
        if (in_string) {
            if (c == '\\' && i + 1 < json.size()) { str += json[++i]; continue; }
            if (c == '"') {
                in_string = false;
                if (depth == 2 && reading_key) {
                    key = str;
                    reading_key = false;
                } else if (depth == 2 && expecting_value && key == "name") {
                    current.name = str;
                    expecting_value = false;
                }
                continue;
            }
            str += c;
            continue;
        }
        switch (c) {
            case '"':
                in_string = true;
                str.clear();
                if (depth == 2 && !expecting_value) reading_key = true;
                break;
            case ':':
                if (depth == 2) expecting_value = true;
                break;
            case ',':
                if (depth == 2) expecting_value = false;
                break;
            case '{':
                ++depth;
                if (depth == 2) { current = SwayOutput{}; expecting_value = false; }
                if (depth == 3 && key == "rect") {
                    // Parse the flat {"x":..,"y":..,"width":..,"height":..}.
                    const size_t end = json.find('}', i);
                    const std::string body = json.substr(i, end - i);
                    auto num = [&](const char* field) {
                        const std::string needle = std::string("\"") + field + "\"";
                        const size_t at = body.find(needle);
                        if (at == std::string::npos) return 0;
                        const size_t colon = body.find(':', at);
                        return std::atoi(body.c_str() + colon + 1);
                    };
                    current.rect = {num("x"), num("y"), num("width"), num("height")};
                    i = end;  // the matching '}' closes depth 3
                    --depth;
                    expecting_value = false;
                }
                break;
            case '}':
                if (depth == 2) outputs.push_back(current);
                --depth;
                break;
            case '[':
                ++depth;
                break;
            case ']':
                --depth;
                break;
            default:
                if (depth == 2 && expecting_value && key == "active" &&
                    json.compare(i, 5, "false") == 0) {
                    current.active = false;
                }
                break;
        }
    }
    return outputs;
}

std::string run_capture(const char* cmd) {
    std::string out;
#if !defined(_WIN32)
    if (FILE* p = popen(cmd, "r")) {
        char buf[4096];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof(buf), p)) > 0) out.append(buf, n);
        pclose(p);
    }
#else
    (void)cmd;
#endif
    return out;
}

void probe_sway() {
#if !defined(_WIN32)
    if (!std::getenv("SWAYSOCK")) return;
    const std::vector<SwayOutput> outs =
        parse_outputs(run_capture("swaymsg -t get_outputs -r 2>/dev/null"));
    std::vector<SwayOutput> active;
    for (const auto& o : outs)
        if (o.active && !o.rect.empty()) active.push_back(o);
    std::fprintf(stderr, "dual_screen: sway reports %zu active output(s)\n",
                 active.size());
    for (const auto& o : active)
        std::fprintf(stderr, "dual_screen:   %s %d,%d %dx%d\n", o.name.c_str(),
                     o.rect.x, o.rect.y, o.rect.w, o.rect.h);
    if (active.size() < 2) return;

    const char* game_env = std::getenv("ARIA_DS_GAME_OUTPUT");
    // ROCKNIX's RG DS / RG DS Plus sway setup names the top panel DSI-2 and
    // the bottom (Goodix touch) panel DSI-1.
    const std::string game_name = game_env && *game_env ? game_env : "DSI-2";
    size_t game_idx = 0;
    for (size_t i = 0; i < active.size(); ++i)
        if (active[i].name == game_name) game_idx = i;
    const size_t aux_idx = game_idx == 0 ? 1 : 0;

    int min_x = active[0].rect.x, min_y = active[0].rect.y;
    int max_x = min_x + active[0].rect.w, max_y = min_y + active[0].rect.h;
    for (const auto& o : active) {
        min_x = std::min(min_x, o.rect.x);
        min_y = std::min(min_y, o.rect.y);
        max_x = std::max(max_x, o.rect.x + o.rect.w);
        max_y = std::max(max_y, o.rect.y + o.rect.h);
    }
    std::lock_guard<std::mutex> lk(g_mutex);
    auto rel = [&](const Rect& r) { return Rect{r.x - min_x, r.y - min_y, r.w, r.h}; };
    g_probe_game = rel(active[game_idx].rect);
    g_probe_aux = rel(active[aux_idx].rect);
    g_probe_window_w = max_x - min_x;
    g_probe_window_h = max_y - min_y;
    g_game_output = active[game_idx].name;
    g_aux_output = active[aux_idx].name;
    std::fprintf(stderr, "dual_screen: game panel %s, aux panel %s\n",
                 g_game_output.c_str(), g_aux_output.c_str());
#endif
}

// Maps the touchscreen once. The digitizer is glued to one physical panel, so
// the mapping never follows the game/UI swap. ROCKNIX's RG DS quirk installs a
// udev LIBINPUT_CALIBRATION_MATRIX of "0.5 0 0.5 0 1 0", which already folds
// the touch range onto the right half of the whole output layout (the bottom
// panel, DSI-1 at x=1024). That matrix only lands on the panel when the device
// is mapped to the whole layout (`map_to_output *`, what RetroArch uses on
// ROCKNIXDS); mapping it to DSI-1 alone squeezes every tap toward the panel's
// right edge. ARIA_DS_TOUCH_OUTPUT overrides the target (e.g. DSI-1 on a
// setup without that calibration matrix).
void sync_touch_mapping() {
#if !defined(_WIN32)
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        if (g_game_output.empty() || g_touch_mapped) return;
        g_touch_mapped = true;
    }
    if (std::getenv("ARIA_DS_NO_SWAYMSG")) return;
    const char* env = std::getenv("ARIA_DS_TOUCH_OUTPUT");
    const std::string target = env && *env ? env : "*";
    std::fprintf(stderr, "dual_screen: touch mapped to output %s\n", target.c_str());
    const std::string cmd = "swaymsg 'input type:touch map_to_output " + target +
                            "' >/dev/null 2>&1";
    std::system(cmd.c_str());
#endif
}

}  // namespace

void set_config(const Config& cfg) {
    std::lock_guard<std::mutex> lk(g_mutex);
    g_config = cfg;
}

Config config() {
    std::lock_guard<std::mutex> lk(g_mutex);
    return g_config;
}

void attach_to_compositor() {
    probe_sway();
#if !defined(_WIN32)
    if (std::getenv("SWAYSOCK") && !std::getenv("ARIA_DS_NO_SWAYMSG")) {
        // One window across every output. `fullscreen global` is sway's
        // multi-output fullscreen; the rule covers the case where the window
        // is mapped after this call, the command the case where it already is.
        char cmd[256];
        const long pid = static_cast<long>(getpid());
        std::snprintf(cmd, sizeof(cmd),
                      "swaymsg 'for_window [pid=%ld] fullscreen enable global' "
                      ">/dev/null 2>&1", pid);
        std::system(cmd);
        std::snprintf(cmd, sizeof(cmd),
                      "swaymsg '[pid=%ld] fullscreen enable global' "
                      ">/dev/null 2>&1", pid);
        std::system(cmd);
    }
#endif
    sync_touch_mapping();
}

void preferred_window_size(int* w, int* h) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (g_probe_window_w > 0 && g_probe_window_h > 0) {
        *w = g_probe_window_w;
        *h = g_probe_window_h;
    } else {
        // Desktop testing: two 4:3 panels stacked, 2x a GBA-ish size.
        *w = 512;
        *h = 768;
    }
}

bool layout(int drawable_w, int drawable_h, Rect* game, Rect* aux) {
    Config cfg;
    Rect probe_game, probe_aux;
    int probe_w, probe_h;
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        cfg = g_config;
        probe_game = g_probe_game;
        probe_aux = g_probe_aux;
        probe_w = g_probe_window_w;
        probe_h = g_probe_window_h;
    }
    if (!cfg.enabled || drawable_w <= 0 || drawable_h <= 0) {
        std::lock_guard<std::mutex> lk(g_mutex);
        g_last_valid = false;
        return false;
    }

    Rect g, a;
    Rect env_g, env_a;
    if (parse_rect(std::getenv("ARIA_DS_GAME_RECT"), &env_g) &&
        parse_rect(std::getenv("ARIA_DS_AUX_RECT"), &env_a)) {
        g = env_g;
        a = env_a;
        probe_w = drawable_w;
        probe_h = drawable_h;
    } else if (!probe_game.empty() && !probe_aux.empty() &&
               probe_w > 0 && probe_h > 0) {
        g = probe_game;
        a = probe_aux;
    } else {
        // Geometry fallback: whichever axis is clearly doubled holds the two
        // panels; otherwise stack them (desktop window).
        probe_w = drawable_w;
        probe_h = drawable_h;
        if (drawable_w * 3 >= drawable_h * 5) {        // wider than ~5:3 -> side by side
            g = {0, 0, drawable_w / 2, drawable_h};
            a = {drawable_w / 2, 0, drawable_w - drawable_w / 2, drawable_h};
        } else {
            g = {0, 0, drawable_w, drawable_h / 2};
            a = {0, drawable_h / 2, drawable_w, drawable_h - drawable_h / 2};
        }
    }
    // Window units -> drawable pixels (HiDPI scale or a not-yet-resized window).
    auto scale = [&](const Rect& r) {
        return Rect{r.x * drawable_w / probe_w, r.y * drawable_h / probe_h,
                    r.w * drawable_w / probe_w, r.h * drawable_h / probe_h};
    };
    g = scale(g);
    a = scale(a);
    if (cfg.swap) std::swap(g, a);
    if (game) *game = g;
    if (aux) *aux = a;
    std::lock_guard<std::mutex> lk(g_mutex);
    g_last_game = g;
    g_last_aux = a;
    g_last_valid = true;
    return true;
}

bool last_layout(Rect* game, Rect* aux) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_last_valid) return false;
    if (game) *game = g_last_game;
    if (aux) *aux = g_last_aux;
    return true;
}

}  // namespace gbarecomp::dual_screen

extern "C" int gbarecomp_dual_screen_aux_rect(int* x, int* y, int* w, int* h) {
    gbarecomp::dual_screen::Rect aux;
    if (!gbarecomp::dual_screen::last_layout(nullptr, &aux)) return 0;
    if (x) *x = aux.x;
    if (y) *y = aux.y;
    if (w) *w = aux.w;
    if (h) *h = aux.h;
    return 1;
}

extern "C" int gbarecomp_dual_screen_game_rect(int* x, int* y, int* w, int* h) {
    gbarecomp::dual_screen::Rect game;
    if (!gbarecomp::dual_screen::last_layout(&game, nullptr)) return 0;
    if (x) *x = game.x;
    if (y) *y = game.y;
    if (w) *w = game.w;
    if (h) *h = game.h;
    return 1;
}
