#include <ngdevkit/neogeo.h>
#include <ngdevkit/ng-video.h>

#define FIX_COLS 40
#define FIX_ROWS 32
#define SHADE_TILE_COUNT 4

static inline void fix_poke(u8 col, u8 row, u8 palette, u16 tile) {
    *REG_VRAMADDR = (u16)(ADDR_FIXMAP + ((u16)col << 5) + row);
    *REG_VRAMMOD = 1;
    *REG_VRAMRW = (u16)(((u16)palette << 12) | tile);
}

static void init_palettes(void) {
    MMAP_PALBANK1[0] = 0x8000;
    MMAP_PALBANK1[1] = 0x0fff;
    for (u16 i = 2; i < 16; i++) {
        MMAP_PALBANK1[i] = MMAP_PALBANK1[1];
    }
}

static void draw_wallpaper(void) {
    for (u8 x = 0; x < FIX_COLS; x++) {
        for (u8 y = 0; y < FIX_ROWS; y++) {
            u16 cell = (u16)(y * FIX_COLS + x);
            fix_poke(x, y, 0, (u16)(cell % SHADE_TILE_COUNT));
        }
    }
}

int main(void) {
    init_palettes();
    draw_wallpaper();

    for (;;) {
        ng_wait_vblank();
    }

    return 0;
}
