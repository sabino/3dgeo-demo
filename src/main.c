#include <ngdevkit/neogeo.h>
#include <ngdevkit/ng-video.h>

#define FIX_COLS 40
#define FIX_MAP_ROWS 32
#define FIX_VISIBLE_ROWS 28
#define FIX_VISIBLE_ROW_OFFSET 2

enum {
    TILE_BLANK = 0,
    TILE_LIGHT,
    TILE_MEDIUM,
    TILE_DARK,
    TILE_FULL,
};

/*
 * Cube art legend:
 *   ' ' = white background
 *   '.' = light shade  (░)
 *   ':' = medium shade (▒)
 *   '*' = dark shade   (▓)
 *   '#' = full block   (█)
 */
static const char cube_map[FIX_VISIBLE_ROWS][FIX_COLS + 1] = {
    "                                        ",
    "                                        ",
    "                                        ",
    "                                        ",
    "                                        ",
    "                                        ",
    "                                        ",
    "                                        ",
    "                                        ",
    "              ............              ",
    "        .....................***        ",
    "        ...............********.        ",
    "        ............***********.        ",
    "         .......... ***********         ",
    "         .........  ***********         ",
    "         ........   ***********         ",
    "         .......    ***********         ",
    "         .......    **********.         ",
    "           ......   *******.            ",
    "               ...  ****.               ",
    "                   ..                   ",
    "                                        ",
    "                                        ",
    "                                        ",
    "                                        ",
    "                                        ",
    "                                        ",
    "                                        ",
};

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

static u16 tile_for_art_char(char ch) {
    switch (ch) {
    case '.':
        return TILE_LIGHT;
    case ':':
        return TILE_MEDIUM;
    case '*':
        return TILE_DARK;
    case '#':
        return TILE_FULL;
    default:
        return TILE_BLANK;
    }
}

static void draw_cube(void) {
    for (u8 x = 0; x < FIX_COLS; x++) {
        for (u8 y = 0; y < FIX_MAP_ROWS; y++) {
            fix_poke(x, y, 0, TILE_BLANK);
        }
    }

    for (u8 x = 0; x < FIX_COLS; x++) {
        for (u8 y = 0; y < FIX_VISIBLE_ROWS; y++) {
            fix_poke(x, (u8)(y + FIX_VISIBLE_ROW_OFFSET), 0, tile_for_art_char(cube_map[y][x]));
        }
    }
}

int main(void) {
    init_palettes();
    draw_cube();

    for (;;) {
        ng_wait_vblank();
    }

    return 0;
}
