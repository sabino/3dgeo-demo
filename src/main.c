#include <ngdevkit/neogeo.h>
#include <ngdevkit/ng-video.h>

#include "cube_frames.h"

#define FIX_COLS 40
#define FIX_MAP_ROWS 32
#define FIX_VISIBLE_ROWS 28
#define FIX_VISIBLE_ROW_OFFSET 2
#define NO_DIRECTION 0xff

enum {
    TILE_BLANK = 0,
};

enum {
    PAL_PAPER = 0,
    PAL_FACE_LIGHT,
    PAL_FACE_MID,
    PAL_FACE_DARK,
    PAL_EDGE,
};

static inline u16 fix_word(u8 palette, u16 tile) {
    return (u16)(((u16)palette << 12) | tile);
}

static inline void fix_poke_addr(u16 addr, u16 word) {
    *REG_VRAMADDR = (u16)(ADDR_FIXMAP + addr);
    *REG_VRAMMOD = 1;
    *REG_VRAMRW = word;
}

static inline void fix_poke_word(u8 col, u8 row, u16 word) {
    fix_poke_addr((u16)(((u16)col << 5) + row), word);
}

static void init_one_palette(u8 palette, u16 paper) {
    volatile u16 *base = &MMAP_PALBANK1[(u16)palette << 4];

    base[0] = 0x8000;
    base[1] = paper;
    for (u8 i = 2; i < 16; i++) {
        base[i] = base[1];
    }
}

static void init_palettes(void) {
    init_one_palette(PAL_PAPER, 0x0fff);
    init_one_palette(PAL_FACE_LIGHT, 0x0fff);
    init_one_palette(PAL_FACE_MID, 0x0aaa);
    init_one_palette(PAL_FACE_DARK, 0x0555);
    init_one_palette(PAL_EDGE, 0x0222);
}

static void clear_fix_map(void) {
    u16 blank = fix_word(PAL_PAPER, TILE_BLANK);

    for (u8 x = 0; x < FIX_COLS; x++) {
        for (u8 y = 0; y < FIX_MAP_ROWS; y++) {
            fix_poke_word(x, y, blank);
        }
    }

}

static u16 frame_index(u8 yaw, u8 pitch) {
    return (u16)((u16)pitch * CUBE_YAW_STEPS + yaw);
}

static u8 read_p1_input(void) {
    return (u8)~(*REG_P1CNT);
}

static u8 update_angles(u8 input, u8 *yaw, u8 *pitch) {
    if (input & CNT_LEFT) {
        *yaw = (u8)((*yaw + CUBE_YAW_STEPS - 1) & (CUBE_YAW_STEPS - 1));
        return CUBE_DIR_LEFT;
    }
    if (input & CNT_RIGHT) {
        *yaw = (u8)((*yaw + 1) & (CUBE_YAW_STEPS - 1));
        return CUBE_DIR_RIGHT;
    }
    if (input & CNT_UP) {
        *pitch = (u8)((*pitch + CUBE_PITCH_STEPS - 1) & (CUBE_PITCH_STEPS - 1));
        return CUBE_DIR_UP;
    }
    if (input & CNT_DOWN) {
        *pitch = (u8)((*pitch + 1) & (CUBE_PITCH_STEPS - 1));
        return CUBE_DIR_DOWN;
    }

    return NO_DIRECTION;
}

static void apply_updates(const u16 *updates, u16 count) {
    for (u16 i = 0; i < count; i++) {
        fix_poke_addr(updates[0], updates[1]);
        updates += 2;
    }
}

int main(void) {
    u8 yaw = CUBE_START_YAW;
    u8 pitch = CUBE_START_PITCH;
    u16 displayed = CUBE_START_FRAME;

    init_palettes();
    clear_fix_map();
    ng_wait_vblank();
    apply_updates(cube_start_updates, CUBE_START_UPDATE_COUNT);

    for (;;) {
        ng_wait_vblank();

        u16 current = displayed;
        u8 direction = update_angles(read_p1_input(), &yaw, &pitch);
        if (direction != NO_DIRECTION) {
            u32 offset = cube_step_offsets[current][direction];
            apply_updates(&cube_step_updates[offset << 1], cube_step_counts[current][direction]);
            displayed = frame_index(yaw, pitch);
        }
    }

    return 0;
}
