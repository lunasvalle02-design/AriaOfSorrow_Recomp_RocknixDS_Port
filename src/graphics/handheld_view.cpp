#include "graphics/handheld_view.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "runtime.h"

extern "C" int g_ws_pillarbox;
extern "C" int g_ws_pillarbox_left;
extern "C" int g_ws_pillarbox_right;
extern "C" int g_ws_letterbox_top;
extern "C" int g_ws_letterbox_bottom;

namespace {

enum WsTilemapProviderResult : int {
    kWsTilemapUnavailable = 0,
    kWsTilemapReplace = 1,
    kWsTilemapKeepWrapped = 2,
    kWsTilemapVoid = 3,
};

// EWRAM layout (third_party/cvaos/include/structs/ewram.h). Offsets are from
// the start of EWRAM; the struct comments there are absolute offsets.
constexpr uint32_t kGameMode        = 0x00010;  // 4 = GAME_MODE_IN_GAME
constexpr uint32_t kInGameState     = 0x00064;  // unk_60.unk_64: 1 = field
constexpr uint32_t kBgInfo          = 0x0A078;  // struct EwramData_unkA078[4]
constexpr uint32_t kBgInfoStride    = 0x1C;
constexpr uint32_t kBg1Metatiles    = 0x0A108;  // unk_A108 (RAM tileset, BG1)
constexpr uint32_t kOtherMetatiles  = 0x0C0EC;  // unk_C0EC (RAM tileset, others)

constexpr uint8_t kGameModeInGame = 4;
constexpr uint8_t kInGameField    = 1;

inline uint16_t Rd16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
inline uint32_t Rd32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

// World coordinate of hardware pixel 0 for a layer: congruent to the
// hardware scroll register (what the PPU actually shows) and closest to the
// game's camera variable, which may already hold the next frame's value.
int AlignToScroll(int cameraPos, unsigned hwScroll) {
    const int delta = ((cameraPos - static_cast<int>(hwScroll) + 128) & 0xFF) - 128;
    return cameraPos - delta;
}

}  // namespace

namespace aria::graphics {

HandheldView& HandheldView::Get() {
    static HandheldView instance;
    return instance;
}

void HandheldView::SetRomData(const uint8_t* rom, size_t romSize) {
    m_rom = rom;
    m_romSize = romSize;
}

const uint8_t* HandheldView::Resolve(uint32_t guest, size_t bytes) const {
    if (guest >= 0x02000000u && guest < 0x02040000u) {
        const size_t off = guest - 0x02000000u;
        if (m_ewram && off + bytes <= m_ewramSize) return m_ewram + off;
    } else if (guest >= 0x08000000u && guest < 0x0A000000u) {
        const size_t off = guest - 0x08000000u;
        if (m_rom && off + bytes <= m_romSize) return m_rom + off;
    }
    return nullptr;
}

void HandheldView::UpdateFrame(const gbarecomp::ExtendedViewFrameInfo* frame,
                               const uint8_t* ewram, size_t ewramSize) {
    m_ewram = ewram;
    m_ewramSize = ewramSize;
    const unsigned extraV = frame ? frame->extra_top + frame->extra_bottom : 0;
    const unsigned extraL = frame ? frame->extra_left : 0;
    const unsigned extraR = frame ? frame->extra_right : 0;
    m_extraV = extraV;

    auto notField = [&] {
        m_inField = false;
        g_ws_letterbox_top = 1;
        g_ws_letterbox_bottom = 1;
        g_ws_pillarbox = 1;
        g_ws_pillarbox_left = 0;
        g_ws_pillarbox_right = 0;
        // Menus and the map keep the field's last vertical placement so the
        // picture does not jump when pausing; boot/title screens center it.
        const bool inGame = ewram && ewramSize > kGameMode &&
                            ewram[kGameMode] == kGameModeInGame;
        if (!inGame || !m_haveFieldSplit) {
            m_splitTop = extraV / 2;
            m_haveFieldSplit = false;
        }
        gbarecomp_set_view_vertical_split(m_splitTop);
    };

    if (!ewram || ewramSize < 0x20000 || !frame || !frame->io) {
        notField();
        return;
    }
    if (ewram[kGameMode] != kGameModeInGame || ewram[kInGameState] != kInGameField) {
        notField();
        return;
    }
    const uint16_t dispcnt = Rd16(frame->io);
    if ((dispcnt & 0x7) != 0 || (dispcnt & 0x0200) == 0) {  // text mode, BG1 on
        notField();
        return;
    }

    for (int bg = 0; bg < 4; ++bg) {
        Layer& layer = m_layers[bg];
        const uint8_t* info = ewram + kBgInfo + bg * kBgInfoStride;
        layer.metaGuest = Rd32(info);
        const int posX = static_cast<int16_t>(Rd16(info + 0x06));
        const int posY = static_cast<int16_t>(Rd16(info + 0x0A));
        const unsigned hofs = Rd16(frame->io + 0x10 + bg * 4) & 0x1FF;
        const unsigned vofs = Rd16(frame->io + 0x12 + bg * 4) & 0x1FF;
        layer.originX = AlignToScroll(posX, hofs);
        layer.originY = AlignToScroll(posY, vofs);
        const uint8_t* meta = Resolve(layer.metaGuest, 0x10);
        if (meta) {
            // Block maps are 256px per screen in both axes; the game treats a
            // single-screen room as 240x208 (sub_08001A00), the rest is unused.
            layer.width = meta[0] == 1 ? 240 : meta[0] * 256;
            layer.height = meta[1] == 1 ? 208 : meta[1] * 256;
            layer.streamed = meta[0] > 1 || meta[1] > 1;
        } else {
            layer.width = layer.height = 0;
            layer.streamed = false;
        }
    }
    // Authored extent of each layer, in its own world coordinates: what the
    // game camera can ever show. It never shows a layer outside x in
    // [0, width) nor y in [48, screens*256 - 48) (camera y in
    // [48, height - 208]), and nothing past that is guaranteed: a fixed
    // single-screen backdrop (240x208) holds stale data past column 240 and
    // outside rows 48-207 of the hardware ring (duplicated moon, cyan blocks),
    // and the room's own block map has unfinished filler below the camera's
    // range (dark blocks under the floor). Samples outside the extent are
    // drawn black (kWsTilemapVoid) instead.
    for (int bg = 0; bg < 4; ++bg) {
        Layer& layer = m_layers[bg];
        const uint8_t* meta = Resolve(layer.metaGuest, 0x10);
        if (!meta) {
            layer.authX0 = layer.authX1 = layer.authY0 = layer.authY1 = 0;
            continue;
        }
        layer.authX0 = 0;
        layer.authX1 = layer.width;
        layer.authY0 = 48;
        layer.authY1 = std::max<int>(1, meta[1]) * 256 - 48;
    }
    if (std::getenv("ARIA_DEBUG_LAYERS")) {
        static unsigned s_n = 0;
        if ((s_n++ % 50) == 0) {
            for (int bg = 0; bg < 4; ++bg) {
                const Layer& l = m_layers[bg];
                const uint8_t* meta = Resolve(l.metaGuest, 0x10);
                const uint16_t cnt = Rd16(frame->io + 0x08 + bg * 2);
                std::fprintf(stderr,
                    "layers f%u bg%d on=%d cnt=%04x meta=%08x [%d %d flags=%04x tbl=%08x map=%08x] "
                    "origin=%d,%d size=%dx%d hw=%u,%u auth=[%d,%d)x[%d,%d) prio=%u\n",
                    s_n, bg, (dispcnt >> (8 + bg)) & 1, cnt, l.metaGuest,
                    meta ? meta[0] : -1, meta ? meta[1] : -1, meta ? Rd16(meta + 2) : 0,
                    meta ? Rd32(meta + 4) : 0, meta ? Rd32(meta + 0x0C) : 0,
                    l.originX, l.originY, l.width, l.height,
                    Rd16(frame->io + 0x10 + bg * 4) & 0x1FF, Rd16(frame->io + 0x12 + bg * 4) & 0x1FF,
                    l.authX0, l.authX1, l.authY0, l.authY1, cnt & 3u);
            }
        }
    }
    const Layer& field = m_layers[1];
    if (field.width <= 0 || field.height <= 0) {
        notField();
        return;
    }
    m_inField = true;
    m_roomW = field.width;
    m_roomH = field.height;

    // Vertical: a 4:3 camera clamped to the room's visible band, the rows
    // the game camera can reach (BG1's authored extent). The game camera's
    // 160 rows sit `top` rows below the top of the taller window. Next to the
    // band's ends the extra rows move to the other side; a room whose band
    // is shorter than the window (a single-screen-tall corridor is exactly
    // 160 rows) shows what exists and keeps the rest black, centered.
    const int camY = field.originY;
    const int viewH = 160 + static_cast<int>(extraV);
    const int bandTop = field.authY0;
    const int bandBottom = std::max(bandTop, field.authY1);
    int top = static_cast<int>(extraV) / 2;
    if (bandBottom - bandTop >= viewH) {
        const int effTop = std::clamp(camY - top, bandTop, bandBottom - viewH);
        top = std::clamp(camY - effTop, 0, static_cast<int>(extraV));
        g_ws_letterbox_top = 0;
        g_ws_letterbox_bottom = 0;
    } else {
        const int above = std::max(0, camY - bandTop);
        const int below = std::max(0, bandBottom - (camY + 160));
        const int deficit = std::max(0, static_cast<int>(extraV) - above - below);
        top = std::clamp(above + deficit / 2, 0, static_cast<int>(extraV));
        g_ws_letterbox_top = above == 0;
        g_ws_letterbox_bottom = below == 0;
    }
    if (const char* dbg = std::getenv("ARIA_DEBUG_SPLIT_TOP")) {
        top = std::clamp(std::atoi(dbg), 0, static_cast<int>(extraV));
        g_ws_letterbox_top = 0;
        g_ws_letterbox_bottom = 0;
    }
    m_splitTop = static_cast<unsigned>(top);
    m_haveFieldSplit = true;
    gbarecomp_set_view_vertical_split(m_splitTop);

    // Horizontal (optional 256-wide mode): the split is fixed, so a side whose
    // margin would leave the room is blacked out instead.
    g_ws_pillarbox = 0;
    g_ws_pillarbox_left = 0;
    g_ws_pillarbox_right = 0;
    if (extraL + extraR > 0) {
        const int camX = field.originX;
        g_ws_pillarbox_left = camX < static_cast<int>(extraL);
        g_ws_pillarbox_right = camX + 240 + static_cast<int>(extraR) > m_roomW;
    }
}

int HandheldView::ProvideTilemapEntry(int bg, int hw_x, int screen_y, uint16_t* out_entry) {
    if (!out_entry || !m_inField || bg < 0 || bg > 3) return kWsTilemapUnavailable;
    // BG0 is screen-space UI (HUD, dialog, banners): it never continues.
    if (bg == 0) return kWsTilemapUnavailable;
    const Layer& layer = m_layers[bg];
    const uint8_t* meta = Resolve(layer.metaGuest, 0x10);
    // Not one of the game's described layers: no continuation at all.
    if (!meta) return kWsTilemapUnavailable;

    const int wx = layer.originX + hw_x;
    const int wy = layer.originY + screen_y;
    // Outside the layer's authored extent: never sample the ring there.
    if (wx < layer.authX0 || wx >= layer.authX1 ||
        wy < layer.authY0 || wy >= layer.authY1)
        return kWsTilemapVoid;
    // A single-screen layer is fully resident in the hardware ring.
    if (!layer.streamed && bg != 1) return kWsTilemapKeepWrapped;

    const int blocksW = meta[0] * 8;
    const int blocksH = meta[1] * 8;
    if (wx < 0 || wy < 0 || (wx >> 5) >= blocksW || (wy >> 5) >= blocksH)
        return kWsTilemapVoid;

    // sub_08001800 / sub_08001994 (cvaos code_08001194.c).
    const int tileX = wx >> 3;
    const int tileY = wy >> 3;
    const size_t blockIdx = static_cast<size_t>(tileX >> 2) +
                            static_cast<size_t>(tileY >> 2) * static_cast<size_t>(blocksW);
    const uint8_t* blockMap = Resolve(Rd32(meta + 0x0C), (blockIdx + 1) * 2);
    if (!blockMap) return kWsTilemapVoid;
    const uint16_t block = Rd16(blockMap + blockIdx * 2);
    const uint16_t flags = block & 0xC000;
    uint16_t id = block & 0x3FFF;
    if (id == 0) {
        *out_entry = 0;
        return kWsTilemapReplace;
    }
    id -= 1;
    int subX = tileX & 3;
    int subY = tileY & 3;
    if (flags & 0x4000) subX = 3 - subX;
    if (flags & 0x8000) subY = 3 - subY;
    const size_t cell = (static_cast<size_t>(id) << 4) + subX + (subY << 2);

    const uint8_t* table = nullptr;
    if (!(Rd16(meta + 2) & 2)) {
        table = Resolve(Rd32(meta + 4), (cell + 1) * 2);
    } else {
        const uint32_t base = bg == 1 ? kBg1Metatiles : kOtherMetatiles;
        if (base + (cell + 1) * 2 <= m_ewramSize) table = m_ewram + base;
    }
    if (!table) return kWsTilemapVoid;
    *out_entry = static_cast<uint16_t>(((flags >> 12) << 8) ^ Rd16(table + cell * 2));
    return kWsTilemapReplace;
}

int HandheldView::FilterBgEntry(int bg, unsigned tileX, unsigned tileY, uint16_t entry) const {
    if (!m_hideHud || !m_inField || bg != 0) return 0;
    // HP/MP frame and bars: BG0 map rows 0-4, columns 0-12, palette 13,
    // tiles 0x200-0x24F (written by the field HUD in VRAM 0xE000).
    const unsigned tile = entry & 0x3FF;
    return tileY <= 4 && tileX <= 12 && (entry >> 12) == 13 &&
           tile >= 0x200 && tile < 0x250;
}

int HandheldView::FilterObj(int /*oamIndex*/, uint16_t attr0, uint16_t attr1,
                            uint16_t attr2) const {
    if (!m_hideHud || !m_inField) return 0;
    // The HP counter digits: 8x8 OBJs, palette 7, tiles 0x80-0x8F, parked in
    // the HUD's top-left corner.
    const unsigned tile = attr2 & 0x3FF;
    const unsigned y = attr0 & 0xFF;
    const unsigned x = attr1 & 0x1FF;
    return (attr2 >> 12) == 7 && tile >= 0x80 && tile < 0x90 && y < 24 && x < 40;
}

}  // namespace aria::graphics
