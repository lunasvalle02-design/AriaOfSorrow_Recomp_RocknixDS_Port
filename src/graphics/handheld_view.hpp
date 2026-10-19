#pragma once

// HandheldView — Aria's policy for the 4:3 dual-screen handheld presentation
// (ROCKNIX / Anbernic RG DS Plus).
//
// The runtime renders a taller-than-native frame (e.g. 240x180, 4:3). The
// extra rows are not invented: rows above hardware line 0 / below line 159 are
// sampled from the room's real block map, so scenery, enemies and projectiles
// that the game already keeps alive just outside its 240x160 camera become
// visible instead of popping in. Each frame this class
//
//  * decides whether the frame is field gameplay (anything else letterboxes),
//  * re-splits the extra rows between top and bottom so the 4:3 window stays
//    inside the room (a real 4:3 camera clamped to the room edges),
//  * supplies the true tilemap entries for the margins (all room layers),
//  * optionally hides the original HUD, which the bottom screen replaces.

#include <cstddef>
#include <cstdint>

namespace gbarecomp {
struct ExtendedViewFrameInfo;
}

namespace aria::graphics {

class HandheldView {
public:
    static HandheldView& Get();

    void SetRomData(const uint8_t* rom, size_t romSize);

    // Per-frame policy; installs the vertical split and letterbox flags.
    void UpdateFrame(const gbarecomp::ExtendedViewFrameInfo* frame,
                     const uint8_t* ewram, size_t ewramSize);

    // PPU hooks (see gba_ppu.h). `screen_y` is the signed hardware line.
    int ProvideTilemapEntry(int bg, int hw_x, int screen_y, uint16_t* out_entry);
    int FilterBgEntry(int bg, unsigned tileX, unsigned tileY, uint16_t entry) const;
    int FilterObj(int oamIndex, uint16_t attr0, uint16_t attr1, uint16_t attr2) const;

    void SetHideOriginalHud(bool hide) { m_hideHud = hide; }
    bool HideOriginalHud() const { return m_hideHud; }

    bool InField() const { return m_inField; }
    int CameraX() const { return m_layers[1].originX; }
    int CameraY() const { return m_layers[1].originY; }
    int RoomWidth() const { return m_roomW; }
    int RoomHeight() const { return m_roomH; }
    unsigned SplitTop() const { return m_splitTop; }

private:
    HandheldView() = default;

    struct Layer {
        uint32_t metaGuest = 0;   // struct EwramData_unkA078_0* (guest ptr)
        int originX = 0;          // world coordinate of hardware column 0
        int originY = 0;          // world coordinate of hardware line 0
        int width = 0;            // layer size in pixels (block map extent)
        int height = 0;
        bool streamed = false;    // larger than the 256x256 hardware ring
    };

    const uint8_t* Resolve(uint32_t guest, size_t bytes) const;

    const uint8_t* m_rom = nullptr;
    size_t m_romSize = 0;
    const uint8_t* m_ewram = nullptr;
    size_t m_ewramSize = 0;

    Layer m_layers[4];
    bool m_inField = false;
    bool m_hideHud = true;
    int m_roomW = 0;
    int m_roomH = 0;
    unsigned m_extraV = 0;
    unsigned m_splitTop = 0;
    bool m_haveFieldSplit = false;
};

}  // namespace aria::graphics
