#include <ngdevkit/neogeo.h>
#include <ngdevkit/ng-video.h>

#define FIX_COLS 40
#define FIX_MAP_ROWS 32
#define FIX_VISIBLE_ROWS 28
#define FIX_VISIBLE_ROW_OFFSET 2

#ifdef RENDERER_SPRITE40
#define FRAME_W FIX_COLS
#define FRAME_H FIX_VISIBLE_ROWS
#define PROJECT_SCALE_X 46
#define PROJECT_SCALE_Y 38
#define SPRITE_ZOOM 0x077f
#elif defined(RENDERER_SPRITE)
#define FRAME_W 20
#define FRAME_H 14
#define PROJECT_SCALE_X 23
#define PROJECT_SCALE_Y 19
#define SPRITE_ZOOM 0x0fff
#else
#define FRAME_W FIX_COLS
#define FRAME_H FIX_VISIBLE_ROWS
#define PROJECT_SCALE_X 46
#define PROJECT_SCALE_Y 38
#endif

#define Q 8
#define ONE (1 << Q)
#define CUBE_RADIUS 120
#define CAMERA_DISTANCE 700
#define ANGLE_MASK 63

#define ADDR_SCB1 0
#define ADDR_SCB2 0x8000
#define ADDR_SCB3 0x8200
#define SPRITE_START_TILE 256
#define SPRITE_SCREEN_X 0
#define SPRITE_SCREEN_Y_REGISTER 496

#if defined(RENDERER_SPRITE) || defined(RENDERER_SPRITE40)
#define RENDERER_USES_SPRITES 1
#endif

enum {
    TILE_CLEAR = 0,
    TILE_SHADE_1,
    TILE_SHADE_2,
    TILE_SHADE_3,
    TILE_SHADE_4,
    TILE_SHADE_5,
    TILE_SHADE_6,
    TILE_SHADE_7,
    TILE_SHADE_8,
    TILE_SHADE_9,
    TILE_PAPER,
    TILE_EDGE_H,
    TILE_EDGE_V,
    TILE_EDGE_DIAG_DOWN,
    TILE_EDGE_DIAG_UP,
    TILE_EDGE_SHALLOW_DOWN,
    TILE_EDGE_SHALLOW_UP,
    TILE_EDGE_STEEP_DOWN,
    TILE_EDGE_STEEP_UP,
};

enum {
    PAL_PAPER = 0,
    PAL_FACE_LIGHT,
    PAL_FACE_MID,
    PAL_FACE_DARK,
    PAL_EDGE,
};

typedef struct {
    s16 x;
    s16 y;
    s16 z;
} Vec3;

typedef struct {
    s16 x;
    s16 y;
    s16 z;
    s16 sx;
    s16 sy;
} ProjectedVertex;

typedef struct {
    u8 v[4];
    Vec3 normal;
} Face;

typedef struct {
    u8 face;
    s16 z;
} FaceOrder;

static u16 frame_words[FRAME_H][FRAME_W];
static u16 prev_words[FRAME_H][FRAME_W];

static const s16 sin_lut[64] = {
       0,   25,   50,   74,   98,  121,  142,  162,
     181,  198,  213,  226,  237,  245,  251,  255,
     256,  255,  251,  245,  237,  226,  213,  198,
     181,  162,  142,  121,   98,   74,   50,   25,
       0,  -25,  -50,  -74,  -98, -121, -142, -162,
    -181, -198, -213, -226, -237, -245, -251, -255,
    -256, -255, -251, -245, -237, -226, -213, -198,
    -181, -162, -142, -121,  -98,  -74,  -50,  -25,
};

static const Vec3 cube_vertices[8] = {
    {-CUBE_RADIUS, -CUBE_RADIUS, -CUBE_RADIUS},
    { CUBE_RADIUS, -CUBE_RADIUS, -CUBE_RADIUS},
    { CUBE_RADIUS,  CUBE_RADIUS, -CUBE_RADIUS},
    {-CUBE_RADIUS,  CUBE_RADIUS, -CUBE_RADIUS},
    {-CUBE_RADIUS, -CUBE_RADIUS,  CUBE_RADIUS},
    { CUBE_RADIUS, -CUBE_RADIUS,  CUBE_RADIUS},
    { CUBE_RADIUS,  CUBE_RADIUS,  CUBE_RADIUS},
    {-CUBE_RADIUS,  CUBE_RADIUS,  CUBE_RADIUS},
};

static const Face cube_faces[6] = {
    {{0, 1, 2, 3}, {  0,   0, -ONE}},
    {{4, 5, 6, 7}, {  0,   0,  ONE}},
    {{0, 1, 5, 4}, {  0, -ONE,   0}},
    {{3, 2, 6, 7}, {  0,  ONE,   0}},
    {{0, 3, 7, 4}, {-ONE,   0,   0}},
    {{1, 2, 6, 5}, { ONE,   0,   0}},
};

static inline s16 qmul(s16 a, s16 b) {
    return (s16)(((s32)a * (s32)b) >> Q);
}

static inline s16 sine(u8 angle) {
    return sin_lut[angle & ANGLE_MASK];
}

static inline s16 cosine(u8 angle) {
    return sin_lut[(angle + 16) & ANGLE_MASK];
}

static inline u16 fix_word(u8 palette, u16 tile) {
    return (u16)(((u16)palette << 12) | tile);
}

static inline u8 word_palette(u16 word) {
    return (u8)(word >> 12);
}

static inline u16 word_tile(u16 word) {
    return (u16)(word & 0x0fff);
}

static inline void fix_poke_word(u8 col, u8 row, u16 word) {
    *REG_VRAMADDR = (u16)(ADDR_FIXMAP + ((u16)col << 5) + row);
    *REG_VRAMMOD = 1;
    *REG_VRAMRW = word;
}

#ifdef RENDERER_USES_SPRITES
static inline u16 sprite_attr(u16 word) {
    return (u16)((u16)word_palette(word) << 8);
}

static inline u16 sprite_tile(u16 word) {
    return (u16)(SPRITE_START_TILE + word_tile(word));
}

static inline void sprite_poke_cell(u8 col, u8 row, u16 word) {
    *REG_VRAMADDR = (u16)(ADDR_SCB1 + ((u16)col * 64) + ((u16)row * 2));
    *REG_VRAMMOD = 1;
    *REG_VRAMRW = sprite_tile(word);
    *REG_VRAMRW = sprite_attr(word);
}

static void init_sprite_framebuffer(void) {
    u16 blank = fix_word(PAL_PAPER, TILE_PAPER);

    *REG_VRAMMOD = 1;
    for (u8 x = 0; x < FRAME_W; x++) {
        *REG_VRAMADDR = (u16)(ADDR_SCB1 + ((u16)x * 64));
        for (u8 y = 0; y < FRAME_H; y++) {
            *REG_VRAMRW = sprite_tile(blank);
            *REG_VRAMRW = sprite_attr(blank);
        }
    }

    *REG_VRAMMOD = 0x200;
    *REG_VRAMADDR = ADDR_SCB2;
    *REG_VRAMRW = SPRITE_ZOOM;
    *REG_VRAMRW = (u16)((SPRITE_SCREEN_Y_REGISTER << 7) + FRAME_H);
    *REG_VRAMRW = (u16)(SPRITE_SCREEN_X << 7);

    for (u8 x = 1; x < FRAME_W; x++) {
        *REG_VRAMADDR = (u16)(ADDR_SCB2 + x);
        *REG_VRAMRW = SPRITE_ZOOM;
        *REG_VRAMRW = 1 << 6;
    }
}
#endif

static void init_one_palette(u8 palette, u16 paper) {
    volatile u16 *base = &MMAP_PALBANK1[(u16)palette << 4];

    base[0] = 0x8000;
    base[1] = paper;
    base[2] = 0x0fff;
    for (u8 i = 3; i < 16; i++) {
        base[i] = base[1];
    }
}

static void init_palettes(void) {
    init_one_palette(PAL_PAPER, 0x0fff);
    init_one_palette(PAL_FACE_LIGHT, 0x0ddd);
    init_one_palette(PAL_FACE_MID, 0x0999);
    init_one_palette(PAL_FACE_DARK, 0x0555);
    init_one_palette(PAL_EDGE, 0x0111);
}

static Vec3 rotate_vec(Vec3 p, u8 pitch, u8 yaw, u8 roll) {
    s16 sx = sine(pitch);
    s16 cx = cosine(pitch);
    s16 sy = sine(yaw);
    s16 cy = cosine(yaw);
    s16 sz = sine(roll);
    s16 cz = cosine(roll);

    s16 x1 = p.x;
    s16 y1 = (s16)(qmul(p.y, cx) - qmul(p.z, sx));
    s16 z1 = (s16)(qmul(p.y, sx) + qmul(p.z, cx));

    s16 x2 = (s16)(qmul(x1, cy) + qmul(z1, sy));
    s16 y2 = y1;
    s16 z2 = (s16)(-qmul(x1, sy) + qmul(z1, cy));

    Vec3 out;
    out.x = (s16)(qmul(x2, cz) - qmul(y2, sz));
    out.y = (s16)(qmul(x2, sz) + qmul(y2, cz));
    out.z = z2;
    return out;
}

static ProjectedVertex project_vertex(Vec3 p) {
    ProjectedVertex out;
    s16 denom = (s16)(p.z + CAMERA_DISTANCE);
    if (denom < 160) denom = 160;

    out.x = p.x;
    out.y = p.y;
    out.z = p.z;
    out.sx = (s16)(FRAME_W / 2 + ((s32)p.x * PROJECT_SCALE_X) / denom);
    out.sy = (s16)(FRAME_H / 2 - ((s32)p.y * PROJECT_SCALE_Y) / denom);
    return out;
}

static void clear_frame(void) {
    u16 blank = fix_word(PAL_PAPER, TILE_PAPER);

    for (u8 y = 0; y < FRAME_H; y++) {
        for (u8 x = 0; x < FRAME_W; x++) {
            frame_words[y][x] = blank;
        }
    }
}

static s16 light_for_normal(Vec3 normal) {
    Vec3 light = {-104, -150, -186};

    return (s16)(((s32)normal.x * light.x +
                  (s32)normal.y * light.y +
                  (s32)normal.z * light.z) >> Q);
}

static u16 face_word_for_normal(Vec3 normal) {
    s16 intensity = light_for_normal(normal);

#ifdef RENDERER_SPRITE40
    if (intensity > 85) return fix_word(PAL_FACE_LIGHT, TILE_SHADE_9);
    if (intensity > -55) return fix_word(PAL_FACE_MID, TILE_SHADE_9);
    return fix_word(PAL_FACE_DARK, TILE_SHADE_9);
#else
    if (intensity > 145) return fix_word(PAL_FACE_LIGHT, TILE_SHADE_1);
    if (intensity > 105) return fix_word(PAL_FACE_LIGHT, TILE_SHADE_2);
    if (intensity > 70) return fix_word(PAL_FACE_MID, TILE_SHADE_3);
    if (intensity > 35) return fix_word(PAL_FACE_MID, TILE_SHADE_4);
    if (intensity > 0) return fix_word(PAL_FACE_MID, TILE_SHADE_5);
    if (intensity > -45) return fix_word(PAL_FACE_DARK, TILE_SHADE_5);
    if (intensity > -90) return fix_word(PAL_FACE_DARK, TILE_SHADE_6);
    if (intensity > -135) return fix_word(PAL_FACE_DARK, TILE_SHADE_7);
    return fix_word(PAL_FACE_DARK, TILE_SHADE_8);
#endif
}

static s16 face_depth(const ProjectedVertex verts[8], const Face *face) {
    s32 sum = 0;
    for (u8 i = 0; i < 4; i++) {
        sum += verts[face->v[i]].z;
    }
    return (s16)(sum >> 2);
}

static void sort_faces(FaceOrder order[6]) {
    for (u8 i = 1; i < 6; i++) {
        FaceOrder cur = order[i];
        s8 j = (s8)i - 1;
        while (j >= 0 && order[j].z < cur.z) {
            order[j + 1] = order[j];
            j--;
        }
        order[j + 1] = cur;
    }
}

static s16 edge_cross(s16 ax, s16 ay, s16 bx, s16 by, s16 px, s16 py) {
    return (s16)((s32)(bx - ax) * (py - ay) - (s32)(by - ay) * (px - ax));
}

static u8 point_in_quad(const ProjectedVertex verts[8], const Face *face, s16 x, s16 y) {
    s16 first = 0;

    for (u8 i = 0; i < 4; i++) {
        const ProjectedVertex *a = &verts[face->v[i]];
        const ProjectedVertex *b = &verts[face->v[(i + 1) & 3]];
        s16 c = edge_cross(a->sx, a->sy, b->sx, b->sy, x, y);
        if (c == 0) continue;
        if (first == 0) first = c;
        else if ((first < 0 && c > 0) || (first > 0 && c < 0)) return 0;
    }

    return 1;
}

static void fill_face(const ProjectedVertex verts[8], const Face *face, u16 word) {
    s16 min_x = FRAME_W - 1;
    s16 max_x = 0;
    s16 min_y = FRAME_H - 1;
    s16 max_y = 0;

    for (u8 i = 0; i < 4; i++) {
        const ProjectedVertex *v = &verts[face->v[i]];
        if (v->sx < min_x) min_x = v->sx;
        if (v->sx > max_x) max_x = v->sx;
        if (v->sy < min_y) min_y = v->sy;
        if (v->sy > max_y) max_y = v->sy;
    }

    if (min_x < 0) min_x = 0;
    if (max_x >= FRAME_W) max_x = FRAME_W - 1;
    if (min_y < 0) min_y = 0;
    if (max_y >= FRAME_H) max_y = FRAME_H - 1;

    for (s16 y = min_y; y <= max_y; y++) {
        for (s16 x = min_x; x <= max_x; x++) {
            if (point_in_quad(verts, face, x, y)) {
                frame_words[y][x] = word;
            }
        }
    }
}

static u16 edge_word_for_segment(s16 x0, s16 y0, s16 x1, s16 y1) {
    s16 dx = (s16)(x1 - x0);
    s16 dy = (s16)(y1 - y0);
    s16 ax = (dx < 0) ? (s16)-dx : dx;
    s16 ay = (dy < 0) ? (s16)-dy : dy;
    u16 tile;

    if (ay * 4 <= ax) {
        tile = TILE_EDGE_H;
    } else if (ax * 4 <= ay) {
        tile = TILE_EDGE_V;
    } else if (ax * 3 >= ay * 2 && ay * 3 >= ax * 2) {
        tile = ((dx < 0) ^ (dy < 0)) ? TILE_EDGE_DIAG_UP : TILE_EDGE_DIAG_DOWN;
    } else if (ax > ay) {
        tile = ((dx < 0) ^ (dy < 0)) ? TILE_EDGE_SHALLOW_UP : TILE_EDGE_SHALLOW_DOWN;
    } else {
        tile = ((dx < 0) ^ (dy < 0)) ? TILE_EDGE_STEEP_UP : TILE_EDGE_STEEP_DOWN;
    }

    return fix_word(PAL_EDGE, tile);
}

static void line(s16 x0, s16 y0, s16 x1, s16 y1) {
    u16 word = edge_word_for_segment(x0, y0, x1, y1);
    s16 dx = (x0 < x1) ? (x1 - x0) : (x0 - x1);
    s16 sx = (x0 < x1) ? 1 : -1;
    s16 dy = (y0 < y1) ? (y0 - y1) : (y1 - y0);
    s16 sy = (y0 < y1) ? 1 : -1;
    s16 err = (s16)(dx + dy);

    for (;;) {
        if (x0 >= 0 && x0 < FRAME_W && y0 >= 0 && y0 < FRAME_H) {
            frame_words[y0][x0] = word;
        }
        if (x0 == x1 && y0 == y1) break;
        s16 e2 = (s16)(err << 1);
        if (e2 >= dy) {
            err = (s16)(err + dy);
            x0 = (s16)(x0 + sx);
        }
        if (e2 <= dx) {
            err = (s16)(err + dx);
            y0 = (s16)(y0 + sy);
        }
    }
}

static void draw_face_edges(const ProjectedVertex verts[8], const Face *face) {
    for (u8 i = 0; i < 4; i++) {
        const ProjectedVertex *a = &verts[face->v[i]];
        const ProjectedVertex *b = &verts[face->v[(i + 1) & 3]];
        line(a->sx, a->sy, b->sx, b->sy);
    }
}

static void render_cube(u8 pitch, u8 yaw, u8 roll) {
    ProjectedVertex verts[8];
    Vec3 normals[6];
    FaceOrder order[6];

    for (u8 i = 0; i < 8; i++) {
        verts[i] = project_vertex(rotate_vec(cube_vertices[i], pitch, yaw, roll));
    }

    for (u8 i = 0; i < 6; i++) {
        normals[i] = rotate_vec(cube_faces[i].normal, pitch, yaw, roll);
        order[i].face = i;
        order[i].z = face_depth(verts, &cube_faces[i]);
    }
    sort_faces(order);

    clear_frame();
    for (u8 i = 0; i < 6; i++) {
        const Face *face = &cube_faces[order[i].face];
        fill_face(verts, face, face_word_for_normal(normals[order[i].face]));
    }
    for (u8 i = 0; i < 6; i++) {
        u8 face_id = order[i].face;
        if (normals[face_id].z < -16) {
            draw_face_edges(verts, &cube_faces[face_id]);
        }
    }
}

static void blit_frame(void) {
    for (u8 y = 0; y < FRAME_H; y++) {
        for (u8 x = 0; x < FRAME_W; x++) {
            u16 word = frame_words[y][x];
            if (word != prev_words[y][x]) {
#ifdef RENDERER_USES_SPRITES
                sprite_poke_cell(x, y, word);
#else
                fix_poke_word(x, (u8)(y + FIX_VISIBLE_ROW_OFFSET), word);
#endif
                prev_words[y][x] = word;
            }
        }
    }
}

static void clear_fix_map(void) {
#ifdef RENDERER_USES_SPRITES
    u16 blank = fix_word(PAL_PAPER, TILE_CLEAR);
#else
    u16 blank = fix_word(PAL_PAPER, TILE_PAPER);
#endif

    for (u8 x = 0; x < FIX_COLS; x++) {
        for (u8 y = 0; y < FIX_MAP_ROWS; y++) {
            fix_poke_word(x, y, blank);
        }
    }

    for (u8 y = 0; y < FRAME_H; y++) {
        for (u8 x = 0; x < FRAME_W; x++) {
            prev_words[y][x] = 0xffff;
        }
    }
}

static u8 read_p1_input(void) {
    return (u8)~(*REG_P1CNT);
}

static u8 update_angles(u8 input, u8 *pitch, u8 *yaw, u8 *roll) {
    u8 changed = 0;

    if (input & CNT_LEFT) {
        *yaw = (u8)((*yaw - 1) & ANGLE_MASK);
        changed = 1;
    } else if (input & CNT_RIGHT) {
        *yaw = (u8)((*yaw + 1) & ANGLE_MASK);
        changed = 1;
    }
    if (input & CNT_UP) {
        *pitch = (u8)((*pitch - 1) & ANGLE_MASK);
        changed = 1;
    } else if (input & CNT_DOWN) {
        *pitch = (u8)((*pitch + 1) & ANGLE_MASK);
        changed = 1;
    }
    if (input & CNT_A) {
        *roll = (u8)((*roll - 1) & ANGLE_MASK);
        changed = 1;
    } else if (input & CNT_B) {
        *roll = (u8)((*roll + 1) & ANGLE_MASK);
        changed = 1;
    }

    return changed;
}

int main(void) {
    u8 pitch = 4;
    u8 yaw = 5;
    u8 roll = 0;

    init_palettes();
    clear_fix_map();
#ifdef RENDERER_USES_SPRITES
    init_sprite_framebuffer();
#endif
    render_cube(pitch, yaw, roll);
    ng_wait_vblank();
    blit_frame();

    for (;;) {
        if (update_angles(read_p1_input(), &pitch, &yaw, &roll)) {
            render_cube(pitch, yaw, roll);
        }
        ng_wait_vblank();
        blit_frame();
    }

    return 0;
}
