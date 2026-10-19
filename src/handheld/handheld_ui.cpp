#include "handheld/handheld_ui.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "config/config_system.hpp"
#include "gameplay/grant_system.hpp"
#include "gameplay/qol_system.hpp"
#include "graphics/handheld_view.hpp"
#include "ui/settings_rows.hpp"

#include "dual_screen.h"
#include "host_window.h"
#include "imgui.h"

namespace aria::handheld {
namespace {

using aria::config::HandheldScaling;
using aria::config::HandheldViewMode;

// --- Guest data (third_party/cvaos/include/structs/ewram.h) -----------------
constexpr uint32_t kMapPosition   = 0x0008C;  // u16 roomMapXPos:7, roomMapYPos:7
constexpr uint32_t kMapPlanes     = 0x000B4;  // unk_B4[2][40] {visited, seen}
constexpr uint32_t kBg1Info       = 0x0A078 + 0x1C;
constexpr uint32_t kPlayerEntity  = 0x13110;  // EntityData* (guest pointer)
constexpr uint32_t kLevel         = 0x13279;
constexpr uint32_t kHp            = 0x1327A;
constexpr uint32_t kMp            = 0x1327C;
constexpr uint32_t kMaxHp         = 0x1327E;
constexpr uint32_t kMaxMp         = 0x13280;
constexpr uint32_t kBaseStats     = 0x13282;  // STR, CON, INT, LCK
constexpr uint32_t kExperience    = 0x1328C;
constexpr uint32_t kGold          = 0x13290;
constexpr uint32_t kCharacter     = 0x13266;  // 0 = main character

constexpr int kMapW = aria::gameplay::grant_offsets::kRoomTableWidth;   // 64
constexpr int kMapH = aria::gameplay::grant_offsets::kRoomTableHeight;  // 35
constexpr uint16_t kNoRoom = aria::gameplay::grant_offsets::kNoRoom;

std::vector<uint16_t> g_roomTable;
int g_mapMinX = 0, g_mapMinY = 0, g_mapMaxX = kMapW - 1, g_mapMaxY = kMapH - 1;

enum class Tab { Status, Map, Settings };
Tab g_tab = Tab::Status;
bool g_confirmQuit = false;

template <typename T>
T Rd(const uint8_t* ewram, size_t size, uint32_t off) {
    T v{};
    if (ewram && off + sizeof(T) <= size) std::memcpy(&v, ewram + off, sizeof(T));
    return v;
}

uint16_t RoomCell(int x, int y) {
    if (x < 0 || y < 0 || x >= kMapW || y >= kMapH || g_roomTable.empty()) return kNoRoom;
    return g_roomTable[static_cast<size_t>(x) + static_cast<size_t>(y) * kMapW];
}

bool MapBit(const uint8_t* ewram, size_t size, int x, int y, int plane) {
    if (x < 0 || y < 0 || x >= kMapW || y >= 40) return false;
    const uint32_t off = kMapPlanes + static_cast<uint32_t>((x >> 5) * 40 + y) * 8 + plane * 4;
    return (Rd<uint32_t>(ewram, size, off) >> (x & 31)) & 1u;
}

// Player's map cell, or false when the game is not in a mapped room.
bool PlayerCell(const uint8_t* ewram, size_t size, int* cx, int* cy) {
    const auto& view = aria::graphics::HandheldView::Get();
    if (!view.InField()) return false;
    const uint32_t entity = Rd<uint32_t>(ewram, size, kPlayerEntity);
    if (entity < 0x02000000u || entity >= 0x02040000u) return false;
    const uint32_t e = entity - 0x02000000u;
    const int camX = Rd<int16_t>(ewram, size, kBg1Info + 0x06);
    const int camY = Rd<int16_t>(ewram, size, kBg1Info + 0x0A);
    const int roomX = camX + Rd<int16_t>(ewram, size, e + 0x42);
    const int roomY = camY + Rd<int16_t>(ewram, size, e + 0x46);
    const uint16_t pos = Rd<uint16_t>(ewram, size, kMapPosition);
    *cx = (pos & 0x7F) + (roomX >> 8);
    *cy = ((pos >> 7) & 0x7F) + (roomY >> 8);
    return *cx >= 0 && *cy >= 0 && *cx < kMapW && *cy < kMapH;
}

ImU32 Col(int r, int g, int b, int a = 255) { return IM_COL32(r, g, b, a); }

// --- Map ---------------------------------------------------------------------
// Draws cells [x0, x0+cols) x [y0, y0+rows) into the rectangle at `origin`.
void DrawMap(ImDrawList* dl, ImVec2 origin, ImVec2 size, int x0, int y0, int cols,
             int rows, const uint8_t* ewram, size_t ewramSize, uint64_t frame) {
    const float cell = std::floor(std::min(size.x / cols, size.y / rows));
    if (cell < 2.0f) return;
    const ImVec2 o(origin.x + std::floor((size.x - cell * cols) * 0.5f),
                   origin.y + std::floor((size.y - cell * rows) * 0.5f));
    dl->AddRectFilled(o, ImVec2(o.x + cell * cols, o.y + cell * rows), Col(8, 8, 20));

    const float pad = std::max(1.0f, std::floor(cell * 0.08f));
    const float wall = std::max(1.0f, std::floor(cell * 0.14f));
    for (int y = y0; y < y0 + rows; ++y) {
        for (int x = x0; x < x0 + cols; ++x) {
            const uint16_t room = RoomCell(x, y);
            if (room == kNoRoom) continue;
            const bool visited = MapBit(ewram, ewramSize, x, y, 0);
            const bool seen = visited || MapBit(ewram, ewramSize, x, y, 1);
            if (!seen) continue;
            const ImVec2 a(o.x + (x - x0) * cell, o.y + (y - y0) * cell);
            const ImVec2 b(a.x + cell, a.y + cell);
            ImU32 fill = Col(46, 46, 70);
            if (visited) {
                if (room & 0x8000) fill = Col(214, 168, 40);        // warp room
                else if (room & 0x4000) fill = Col(196, 36, 44);    // save room
                else fill = Col(36, 84, 196);
            }
            dl->AddRectFilled(ImVec2(a.x + pad * 0.5f, a.y + pad * 0.5f),
                              ImVec2(b.x - pad * 0.5f, b.y - pad * 0.5f), fill);
            // Walls where the neighbour is not part of the same room.
            const uint16_t key = room & 0x3FF;
            const ImU32 wc = visited ? Col(235, 235, 245) : Col(120, 120, 150);
            auto same = [&](int nx, int ny) {
                const uint16_t n = RoomCell(nx, ny);
                return n != kNoRoom && (n & 0x3FF) == key;
            };
            if (!same(x, y - 1)) dl->AddRectFilled(a, ImVec2(b.x, a.y + wall), wc);
            if (!same(x, y + 1)) dl->AddRectFilled(ImVec2(a.x, b.y - wall), b, wc);
            if (!same(x - 1, y)) dl->AddRectFilled(a, ImVec2(a.x + wall, b.y), wc);
            if (!same(x + 1, y)) dl->AddRectFilled(ImVec2(b.x - wall, a.y), b, wc);
        }
    }

    int px = 0, py = 0;
    if (PlayerCell(ewram, ewramSize, &px, &py) &&
        px >= x0 && py >= y0 && px < x0 + cols && py < y0 + rows) {
        const ImVec2 c(o.x + (px - x0 + 0.5f) * cell, o.y + (py - y0 + 0.5f) * cell);
        const bool blink = (frame / 16) & 1;
        const float r = std::max(2.0f, cell * 0.30f);
        dl->AddCircleFilled(c, r + 2.0f, Col(0, 0, 0));
        dl->AddCircleFilled(c, r, blink ? Col(255, 255, 255) : Col(255, 96, 96));
    }
}

void ComputeMapBounds() {
    int minX = kMapW, minY = kMapH, maxX = -1, maxY = -1;
    for (int y = 0; y < kMapH; ++y)
        for (int x = 0; x < kMapW; ++x)
            if (RoomCell(x, y) != kNoRoom) {
                minX = std::min(minX, x); maxX = std::max(maxX, x);
                minY = std::min(minY, y); maxY = std::max(maxY, y);
            }
    if (maxX >= 0) {
        g_mapMinX = minX; g_mapMinY = minY; g_mapMaxX = maxX; g_mapMaxY = maxY;
    }
}

// --- Widgets -----------------------------------------------------------------
void Bar(ImDrawList* dl, ImVec2 a, ImVec2 size, float ratio, ImU32 fill, ImU32 back) {
    ratio = std::clamp(ratio, 0.0f, 1.0f);
    dl->AddRectFilled(a, ImVec2(a.x + size.x, a.y + size.y), back, size.y * 0.3f);
    if (ratio > 0.0f)
        dl->AddRectFilled(a, ImVec2(a.x + size.x * ratio, a.y + size.y), fill, size.y * 0.3f);
    dl->AddRect(a, ImVec2(a.x + size.x, a.y + size.y), Col(200, 170, 90), size.y * 0.3f, 0,
                std::max(1.0f, size.y * 0.08f));
}

bool TabButton(const char* label, bool active, ImVec2 size) {
    ImGui::PushStyleColor(ImGuiCol_Button, active ? ImVec4(0.55f, 0.12f, 0.16f, 1.0f)
                                                  : ImVec4(0.16f, 0.12f, 0.20f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.65f, 0.20f, 0.24f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.75f, 0.25f, 0.28f, 1.0f));
    const bool pressed = ImGui::Button(label, size);
    ImGui::PopStyleColor(3);
    return pressed;
}

// Row of mutually exclusive choices; returns the clicked index or -1.
int Choice(const char* const* labels, int count, int current, float width, float height) {
    int clicked = -1;
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float w = (width - spacing * (count - 1)) / count;
    for (int i = 0; i < count; ++i) {
        if (i) ImGui::SameLine();
        ImGui::PushID(i);
        if (TabButton(labels[i], i == current, ImVec2(w, height))) clicked = i;
        ImGui::PopID();
    }
    return clicked;
}

void SaveAndApply() {
    aria::ui::SaveConfigNow();
    ApplyConfig();
}

// --- Pages -------------------------------------------------------------------
void StatusPage(ImVec2 origin, ImVec2 size, float s, const uint8_t* ewram, size_t ewramSize,
                uint64_t frame) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const bool inGame = aria::graphics::HandheldView::Get().InField() ||
                        Rd<uint8_t>(ewram, ewramSize, 0x10) == 4;
    const float colW = std::floor(size.x * 0.46f);

    if (inGame) {
        const int level = Rd<uint8_t>(ewram, ewramSize, kLevel);
        const int hp = Rd<int16_t>(ewram, ewramSize, kHp);
        const int mp = Rd<int16_t>(ewram, ewramSize, kMp);
        const int maxHp = std::max<int>(1, Rd<uint16_t>(ewram, ewramSize, kMaxHp));
        const int maxMp = std::max<int>(1, Rd<uint16_t>(ewram, ewramSize, kMaxMp));
        const uint32_t exp = Rd<uint32_t>(ewram, ewramSize, kExperience);
        const uint32_t gold = Rd<uint32_t>(ewram, ewramSize, kGold);

        ImGui::SetCursorScreenPos(origin);
        ImGui::BeginGroup();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.82f, 0.40f, 1.0f));
        ImGui::Text("NIVEL %d", level);
        ImGui::PopStyleColor();
        ImGui::Spacing();

        const float barH = std::floor(14.0f * s);
        auto statBar = [&](const char* name, int cur, int max, ImU32 color) {
            ImGui::Text("%s  %d / %d", name, std::max(cur, 0), max);
            const ImVec2 p = ImGui::GetCursorScreenPos();
            Bar(dl, p, ImVec2(colW, barH), static_cast<float>(cur) / max, color, Col(30, 20, 30));
            ImGui::Dummy(ImVec2(colW, barH + 4.0f * s));
        };
        statBar("HP", hp, maxHp, hp * 4 <= maxHp ? Col(240, 60, 40) : Col(214, 40, 56));
        statBar("MP", mp, maxMp, Col(52, 148, 232));
        ImGui::Spacing();
        ImGui::Text("EXP  %u", exp);
        ImGui::Text("ORO  %u", gold);
        if (Rd<uint8_t>(ewram, ewramSize, kCharacter) == 0) {
            ImGui::Spacing();
            static const char* const kStatNames[] = {"FUE", "CON", "INT", "SUE"};
            for (int i = 0; i < 4; ++i) {
                if (i & 1) ImGui::SameLine(colW * 0.5f + origin.x - ImGui::GetWindowPos().x);
                ImGui::Text("%s %3d", kStatNames[i],
                            Rd<uint16_t>(ewram, ewramSize, kBaseStats + i * 2));
            }
        }
        ImGui::EndGroup();

        // Recently defeated enemy and its soul progress.
        auto& qol = aria::gameplay::QolSystem::Get();
        ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + size.y - ImGui::GetFrameHeight() * 2.2f));
        if (qol.HasRecentEnemy()) {
            const uint16_t enemy = qol.GetRecentTargetEnemy();
            const int souls = qol.GetEnemySoulCount(ewram, ewramSize, enemy);
            ImGui::TextColored(ImVec4(0.85f, 0.60f, 1.0f, 1.0f), "%s", qol.GetEnemyName(enemy).c_str());
            ImGui::TextColored(souls > 0 ? ImVec4(0.5f, 1.0f, 0.5f, 1.0f) : ImVec4(1.0f, 0.5f, 0.5f, 1.0f),
                               souls > 0 ? "Alma obtenida (%d)" : "Alma sin obtener", souls);
        } else if (qol.HasRecentLoadoutChange()) {
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f), "%s",
                               qol.GetActiveLoadoutNotification().c_str());
        } else {
            ImGui::TextDisabled("Preset de equipo %d", qol.GetActiveLoadoutIndex() + 1);
        }
    } else {
        ImGui::SetCursorScreenPos(origin);
        ImGui::TextDisabled("Esperando a la partida...");
    }

    // Live map around the player.
    const ImVec2 mapOrigin(origin.x + colW + 12.0f * s, origin.y);
    const ImVec2 mapSize(size.x - colW - 12.0f * s, size.y);
    int px = 0, py = 0;
    const bool havePlayer = PlayerCell(ewram, ewramSize, &px, &py);
    const int cols = 13, rows = 11;
    static int lastX = 32, lastY = 17;
    if (havePlayer) { lastX = px; lastY = py; }
    DrawMap(dl, mapOrigin, mapSize, lastX - cols / 2, lastY - rows / 2, cols, rows, ewram,
            ewramSize, frame);
    dl->AddRect(mapOrigin, ImVec2(mapOrigin.x + mapSize.x, mapOrigin.y + mapSize.y),
                Col(200, 170, 90), 4.0f * s, 0, 2.0f);
}

void MapPage(ImVec2 origin, ImVec2 size, float s, const uint8_t* ewram, size_t ewramSize,
             uint64_t frame) {
    auto& hh = aria::config::ConfigSystem::Get().GetConfig().handheld;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    int px = 0, py = 0;
    const bool havePlayer = PlayerCell(ewram, ewramSize, &px, &py);
    if (hh.mapZoom == 0) {
        DrawMap(dl, origin, size, g_mapMinX, g_mapMinY, g_mapMaxX - g_mapMinX + 1,
                g_mapMaxY - g_mapMinY + 1, ewram, ewramSize, frame);
    } else {
        static int lastX = 32, lastY = 17;
        if (havePlayer) { lastX = px; lastY = py; }
        const int cols = 25, rows = 17;
        DrawMap(dl, origin, size, lastX - cols / 2, lastY - rows / 2, cols, rows, ewram,
                ewramSize, frame);
    }
    // Tapping the map toggles the zoom level.
    ImGui::SetCursorScreenPos(origin);
    if (ImGui::InvisibleButton("##map", size)) {
        hh.mapZoom = hh.mapZoom ? 0 : 1;
        SaveAndApply();
    }
    ImGui::SetCursorScreenPos(ImVec2(origin.x + 4.0f * s, origin.y + size.y - ImGui::GetFontSize() - 4.0f * s));
    ImGui::TextDisabled(hh.mapZoom ? "Toca el mapa: castillo completo" : "Toca el mapa: acercar");
}

void SettingsPage(ImVec2 origin, ImVec2 size, float s) {
    auto& hh = aria::config::ConfigSystem::Get().GetConfig().handheld;
    const float rowH = std::floor(30.0f * s);
    ImGui::SetCursorScreenPos(origin);
    ImGui::BeginChild("##settings", size, false);
    const float w = ImGui::GetContentRegionAvail().x;

    ImGui::TextUnformatted("Escalado de la pantalla superior");
    static const char* const kScaling[] = {"Suavizado", "Sin filtro", "Entero 4x"};
    if (int c = Choice(kScaling, 3, static_cast<int>(hh.scaling), w, rowH); c >= 0) {
        hh.scaling = static_cast<HandheldScaling>(c);
        SaveAndApply();
    }
    ImGui::Spacing();
    ImGui::TextUnformatted("Formato de imagen (al reiniciar)");
    static const char* const kViews[] = {"4:3 240x180", "4:3 256x192", "Original 3:2"};
    if (int c = Choice(kViews, 3, static_cast<int>(hh.viewMode), w, rowH); c >= 0) {
        hh.viewMode = static_cast<HandheldViewMode>(c);
        SaveAndApply();
    }
    ImGui::Spacing();
    ImGui::TextUnformatted("HUD original en la pantalla superior");
    static const char* const kHud[] = {"Oculto", "Visible"};
    if (int c = Choice(kHud, 2, hh.hideOriginalHud ? 0 : 1, w, rowH); c >= 0) {
        hh.hideOriginalHud = c == 0;
        SaveAndApply();
    }
    ImGui::Spacing();
    ImGui::TextUnformatted("Pantalla del juego");
    static const char* const kSwap[] = {"Superior", "Inferior"};
    if (int c = Choice(kSwap, 2, hh.swapScreens ? 1 : 0, w, rowH); c >= 0) {
        hh.swapScreens = c == 1;
        SaveAndApply();
    }
    ImGui::Spacing();
    ImGui::TextUnformatted("Contador de FPS");
    static const char* const kFps[] = {"Oculto", "Visible"};
    if (int c = Choice(kFps, 2, hh.showFps ? 1 : 0, w, rowH); c >= 0) {
        hh.showFps = c == 1;
        SaveAndApply();
    }
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    const float half = (w - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    if (TabButton("Más opciones / partidas", false, ImVec2(half, rowH))) gbarecomp_runtime_ui_open();
    ImGui::SameLine();
    if (!g_confirmQuit) {
        if (TabButton("Salir del juego", false, ImVec2(half, rowH))) g_confirmQuit = true;
    } else if (TabButton("Confirmar salida", true, ImVec2(half, rowH))) {
        gbarecomp_request_quit();
    }
    ImGui::EndChild();
}

}  // namespace

void SetRomData(const uint8_t* rom, size_t romSize) {
    using aria::gameplay::grant_offsets::kRoomTableFileOffset;
    using aria::gameplay::grant_offsets::kRoomTableCells;
    g_roomTable.clear();
    if (rom && kRoomTableFileOffset + kRoomTableCells * 2 <= romSize) {
        g_roomTable.resize(kRoomTableCells);
        std::memcpy(g_roomTable.data(), rom + kRoomTableFileOffset, kRoomTableCells * 2);
        ComputeMapBounds();
    }
}

void ApplyConfig() {
    const auto& hh = aria::config::ConfigSystem::Get().GetConfig().handheld;
    gbarecomp::dual_screen::Config ds;
    ds.enabled = hh.enabled;
    ds.swap = hh.swapScreens;
    ds.scaling = static_cast<gbarecomp::dual_screen::Scaling>(hh.scaling);
    gbarecomp::dual_screen::set_config(ds);
    aria::graphics::HandheldView::Get().SetHideOriginalHud(hh.hideOriginalHud);
}

void RenderAuxScreen(const uint8_t* ewram, size_t ewramSize, uint64_t frame) {
    static bool s_initialTab = [] {
        // Development aid: start on a given page (0 status, 1 map, 2 settings).
        if (const char* t = std::getenv("ARIA_DEBUG_AUX_TAB"))
            g_tab = static_cast<Tab>(std::clamp(std::atoi(t), 0, 2));
        return true;
    }();
    (void)s_initialTab;
    int rx = 0, ry = 0, rw = 0, rh = 0;
    if (!gbarecomp_dual_screen_aux_rect(&rx, &ry, &rw, &rh) || rw <= 0 || rh <= 0) return;
    const ImGuiIO& io = ImGui::GetIO();
    const float fx = io.DisplayFramebufferScale.x > 0.0f ? io.DisplayFramebufferScale.x : 1.0f;
    const float fy = io.DisplayFramebufferScale.y > 0.0f ? io.DisplayFramebufferScale.y : 1.0f;
    const ImVec2 pos(rx / fx, ry / fy);
    const ImVec2 size(rw / fx, rh / fy);
    // Layout is authored for a 512x384 panel and scaled to the real one.
    const float s = std::min(size.x / 512.0f, size.y / 384.0f);

    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f * s, 8.0f * s));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f * s, 4.0f * s));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f * s);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.04f, 0.08f, 1.0f));
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoSavedSettings |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus |
                                   ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoScrollWithMouse;
    if (ImGui::Begin("##AriaAuxScreen", nullptr, flags)) {
        // Fonts are sized for a 384px-tall panel when no TTF is loaded.
        const float fontScale = ImGui::GetFont()->FontSize < 16.0f ? s : 1.0f;
        ImGui::SetWindowFontScale(fontScale);

        const float tabH = std::floor(30.0f * s);
        const float tabW = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 2) / 3.0f;
        if (TabButton("ESTADO", g_tab == Tab::Status, ImVec2(tabW, tabH))) g_tab = Tab::Status;
        ImGui::SameLine();
        if (TabButton("MAPA", g_tab == Tab::Map, ImVec2(tabW, tabH))) g_tab = Tab::Map;
        ImGui::SameLine();
        if (TabButton("AJUSTES", g_tab == Tab::Settings, ImVec2(tabW, tabH))) {
            g_tab = Tab::Settings;
            g_confirmQuit = false;
        }
        if (aria::config::ConfigSystem::Get().GetConfig().handheld.showFps) {
            // Presented frames per second (the game targets 59.73).
            char fps[32];
            std::snprintf(fps, sizeof(fps), "%.1f FPS", ImGui::GetIO().Framerate);
            const ImVec2 p(pos.x + size.x - ImGui::CalcTextSize(fps).x - 10.0f * s,
                           pos.y + size.y - ImGui::GetFontSize() - 6.0f * s);
            ImGui::GetForegroundDrawList()->AddText(p, IM_COL32(120, 255, 120, 255), fps);
        }
        ImGui::Spacing();
        const ImVec2 contentPos = ImGui::GetCursorScreenPos();
        const ImVec2 contentSize = ImGui::GetContentRegionAvail();
        switch (g_tab) {
            case Tab::Status: StatusPage(contentPos, contentSize, s, ewram, ewramSize, frame); break;
            case Tab::Map: MapPage(contentPos, contentSize, s, ewram, ewramSize, frame); break;
            case Tab::Settings: SettingsPage(contentPos, contentSize, s); break;
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(4);
}

}  // namespace aria::handheld
