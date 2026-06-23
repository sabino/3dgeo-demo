#include <ngdevkit/neogeo.h>
#include <ngdevkit/ng-video.h>

#define FIX_COLS 40
#define FIX_MAP_ROWS 32

#define FRAME_W 80
#define FRAME_H 56
#define PROJECT_SCALE_X 92
#define PROJECT_SCALE_Y 76

#define Q 8
#define ONE (1 << Q)
#define CUBE_RADIUS 120
#define CAMERA_DISTANCE_DEFAULT 760
#define CAMERA_DISTANCE_MIN 420
#define CAMERA_DISTANCE_MAX 1300
#define CAMERA_DISTANCE_STEP 18
#define ANGLE_MASK 63
#define FACE_LEVELS 12

#define ADDR_SCB1 0
#define ADDR_SCB2 0x8000
#define SPRITE_START_TILE 256
#define SPRITE_SCREEN_Y_REGISTER 496
#define SPRITE_MAX_DISPLAYABLE 381
#define SPRITE_ZOOM_4PX_CELL 0x033f
#define SPRITE_COLUMN_CHUNKS 2
#define SPRITE_CHUNK_CELLS (FRAME_H / SPRITE_COLUMN_CHUNKS)
#define SPRITE_BASE_INDEX 1

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
    TILE_MASK_1,
};

enum {
    PAL_PAPER = 0,
    PAL_FACE_0,
    PAL_FACE_1,
    PAL_FACE_2,
    PAL_FACE_3,
    PAL_FACE_4,
    PAL_FACE_5,
    PAL_FACE_6,
    PAL_FACE_7,
    PAL_FACE_8,
    PAL_FACE_9,
    PAL_FACE_10,
    PAL_FACE_11,
    PAL_EDGE,
    PAL_SHADOW_LIGHT,
    PAL_SHADOW_DARK,
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
static s16 camera_distance = CAMERA_DISTANCE_DEFAULT;
static u8 frame_dirty = 1;

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

static inline u16 sprite_attr(u16 word) {
    return (u16)((u16)word_palette(word) << 8);
}

static inline u16 sprite_tile(u16 word) {
    return (u16)(SPRITE_START_TILE + word_tile(word));
}

static inline u16 sprite_y_word(u16 y, u16 height_tiles) {
    return (u16)(((((u16)(SPRITE_SCREEN_Y_REGISTER - y)) & 0x01ff) << 7) |
                 (height_tiles & 0x003f));
}

static inline void fix_poke_word(u8 col, u8 row, u16 word) {
    *REG_VRAMADDR = (u16)(ADDR_FIXMAP + ((u16)col << 5) + row);
    *REG_VRAMMOD = 1;
    *REG_VRAMRW = word;
}

static void clear_fix_map(void) {
    u16 blank = fix_word(PAL_PAPER, TILE_CLEAR);

    for (u8 x = 0; x < FIX_COLS; x++) {
        for (u8 y = 0; y < FIX_MAP_ROWS; y++) {
            fix_poke_word(x, y, blank);
        }
    }
}

static void clear_sprite_state(void) {
    *REG_VRAMMOD = 0x200;
    for (u16 sprite = 0; sprite < SPRITE_MAX_DISPLAYABLE; sprite++) {
        *REG_VRAMADDR = (u16)(ADDR_SCB2 + sprite);
        *REG_VRAMRW = 0;
        *REG_VRAMRW = 0;
        *REG_VRAMRW = 0;
    }
}

static void init_sprite_framebuffer(void) {
    u16 blank = fix_word(PAL_PAPER, TILE_PAPER);

    clear_sprite_state();

    *REG_VRAMMOD = 1;
    for (u16 s = 0; s < (FRAME_W * SPRITE_COLUMN_CHUNKS); s++) {
        u16 sprite = (u16)(SPRITE_BASE_INDEX + s);

        *REG_VRAMADDR = (u16)(ADDR_SCB1 + (sprite * 64));
        for (u8 y = 0; y < SPRITE_CHUNK_CELLS; y++) {
            *REG_VRAMRW = sprite_tile(blank);
            *REG_VRAMRW = sprite_attr(blank);
        }
    }

    *REG_VRAMMOD = 0x200;
    for (u8 chunk = 0; chunk < SPRITE_COLUMN_CHUNKS; chunk++) {
        u16 y = (u16)(chunk * SPRITE_CHUNK_CELLS * 4);
        u16 y_word = sprite_y_word(y, SPRITE_CHUNK_CELLS);

        for (u8 x = 0; x < FRAME_W; x++) {
            u16 sprite = (u16)(SPRITE_BASE_INDEX + chunk * FRAME_W + x);

            *REG_VRAMADDR = (u16)(ADDR_SCB2 + sprite);
            *REG_VRAMRW = SPRITE_ZOOM_4PX_CELL;
            *REG_VRAMRW = y_word;
            *REG_VRAMRW = (u16)((x * 4) << 7);
        }
    }
}

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
    init_one_palette(PAL_FACE_0, 0x0222);
    init_one_palette(PAL_FACE_1, 0x0333);
    init_one_palette(PAL_FACE_2, 0x0444);
    init_one_palette(PAL_FACE_3, 0x0555);
    init_one_palette(PAL_FACE_4, 0x0666);
    init_one_palette(PAL_FACE_5, 0x0777);
    init_one_palette(PAL_FACE_6, 0x0888);
    init_one_palette(PAL_FACE_7, 0x0999);
    init_one_palette(PAL_FACE_8, 0x0aaa);
    init_one_palette(PAL_FACE_9, 0x0bbb);
    init_one_palette(PAL_FACE_10, 0x0ccc);
    init_one_palette(PAL_FACE_11, 0x0eee);
    init_one_palette(PAL_EDGE, 0x0111);
    init_one_palette(PAL_SHADOW_LIGHT, 0x0eee);
    init_one_palette(PAL_SHADOW_DARK, 0x0ddd);
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
    s16 denom = (s16)(p.z + camera_distance);
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

static u8 face_level_for_intensity(s16 intensity) {
    s16 scaled = (s16)(((s32)(intensity + 130) * 16) / 36);

    if (scaled < 0) scaled = 0;
    if (scaled > (FACE_LEVELS - 1) * 16) scaled = (FACE_LEVELS - 1) * 16;

    scaled = (s16)(scaled + 8);
    if (scaled > (FACE_LEVELS - 1) * 16) scaled = (FACE_LEVELS - 1) * 16;

    return (u8)(scaled >> 4);
}

static u16 face_word_for_intensity(s16 intensity) {
    return fix_word((u8)(PAL_FACE_0 + face_level_for_intensity(intensity)), TILE_SHADE_9);
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

static s16 light_for_face_position(const ProjectedVertex verts[8], const Face *face, Vec3 normal) {
    s16 cx = 0;
    s16 cy = 0;
    s16 cz = 0;
    const s16 light_x = -260;
    const s16 light_y = 260;
    const s16 light_z = -340;

    for (u8 i = 0; i < 4; i++) {
        const ProjectedVertex *v = &verts[face->v[i]];
        cx = (s16)(cx + (v->x >> 2));
        cy = (s16)(cy + (v->y >> 2));
        cz = (s16)(cz + (v->z >> 2));
    }

    return (s16)((((s32)normal.x * (light_x - cx) +
                   (s32)normal.y * (light_y - cy) +
                   (s32)normal.z * (light_z - cz)) >> 9) + 24);
}

static void add_edge_spans(s16 left[FRAME_H], s16 right[FRAME_H],
                           const ProjectedVertex *a, const ProjectedVertex *b) {
    s16 x0 = a->sx;
    s16 y0 = a->sy;
    s16 x1 = b->sx;
    s16 y1 = b->sy;

    if (y0 == y1) return;
    if (y0 > y1) {
        s16 tx = x0;
        s16 ty = y0;
        x0 = x1;
        y0 = y1;
        x1 = tx;
        y1 = ty;
    }

    s16 dy = (s16)(y1 - y0);
    s32 step = ((s32)(x1 - x0) << Q) / dy;
    s16 start_y = y0;
    s16 end_y = y1;

    if (start_y < 0) start_y = 0;
    if (end_y > FRAME_H) end_y = FRAME_H;
    if (start_y >= end_y) return;

    s32 x = ((s32)x0 << Q) + step * (start_y - y0);
    for (s16 y = start_y; y < end_y; y++) {
        s16 xi = (s16)(x >> Q);

        if (xi < left[y]) left[y] = xi;
        if (xi > right[y]) right[y] = xi;
        x += step;
    }
}

static void fill_face_spans(const ProjectedVertex verts[8], const Face *face, Vec3 normal) {
    static s16 left[FRAME_H];
    static s16 right[FRAME_H];
    s16 min_y = FRAME_H - 1;
    s16 max_y = 0;
    s16 base = light_for_face_position(verts, face, normal);

    for (u8 i = 0; i < 4; i++) {
        const ProjectedVertex *v = &verts[face->v[i]];
        if (v->sy < min_y) min_y = v->sy;
        if (v->sy > max_y) max_y = v->sy;
    }

    if (min_y < 0) min_y = 0;
    if (max_y >= FRAME_H) max_y = FRAME_H - 1;

    for (s16 y = min_y; y <= max_y; y++) {
        left[y] = 32767;
        right[y] = -32768;
    }

    for (u8 i = 0; i < 4; i++) {
        add_edge_spans(left, right, &verts[face->v[i]], &verts[face->v[(i + 1) & 3]]);
    }

    for (s16 y = min_y; y <= max_y; y++) {
        s16 x0 = left[y];
        s16 x1 = right[y];

        if (x0 > x1) continue;
        if (x0 < 0) x0 = 0;
        if (x1 >= FRAME_W) x1 = FRAME_W - 1;
        if (x0 > x1) continue;

        s16 mid = (s16)((x0 + x1) >> 1);
        s16 row_bias = (s16)((FRAME_H / 2 - y) - ((mid - FRAME_W / 2) >> 1));
        u16 word = face_word_for_intensity((s16)(base + row_bias));

        for (s16 x = x0; x <= x1; x++) {
            frame_words[y][x] = word;
        }
    }
}

static void draw_contact_shadow(const ProjectedVertex verts[8]) {
    s16 min_x = FRAME_W - 1;
    s16 max_x = 0;
    s16 max_y = 0;
    u16 shadow = fix_word(PAL_SHADOW_DARK, TILE_SHADE_3);

    for (u8 i = 0; i < 8; i++) {
        if (verts[i].sx < min_x) min_x = verts[i].sx;
        if (verts[i].sx > max_x) max_x = verts[i].sx;
        if (verts[i].sy > max_y) max_y = verts[i].sy;
    }

    s16 cx = (s16)((min_x + max_x) >> 1);
    s16 rx = (s16)(((max_x - min_x) >> 1) + 3);
    s16 cy = (s16)(max_y + 3);
    s16 ry = 3;

    if (rx < 8) rx = 8;
    if (rx > 26) rx = 26;
    if (cy >= FRAME_H) cy = FRAME_H - 1;

    for (s16 y = (s16)(cy - ry); y <= (s16)(cy + ry); y++) {
        if (y < 0 || y >= FRAME_H) continue;

        s16 dy = (s16)((y > cy) ? (y - cy) : (cy - y));
        s16 width = (s16)(rx - ((s32)dy * rx) / (ry + 1));
        s16 x0 = (s16)(cx - width);
        s16 x1 = (s16)(cx + width);

        if (x0 < 0) x0 = 0;
        if (x1 >= FRAME_W) x1 = FRAME_W - 1;
        for (s16 x = x0; x <= x1; x++) {
            frame_words[y][x] = shadow;
        }
    }
}

static void put_edge(s16 x, s16 y, u16 word) {
    if (x >= 0 && x < FRAME_W && y >= 0 && y < FRAME_H) {
        frame_words[y][x] = word;
    }
}

static void line(s16 x0, s16 y0, s16 x1, s16 y1, u16 word) {
    s16 dx = (x0 < x1) ? (x1 - x0) : (x0 - x1);
    s16 sx = (x0 < x1) ? 1 : -1;
    s16 dy = (y0 < y1) ? (y0 - y1) : (y1 - y0);
    s16 sy = (y0 < y1) ? 1 : -1;
    s16 err = (s16)(dx + dy);

    for (;;) {
        put_edge(x0, y0, word);
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
    u16 edge = fix_word(PAL_EDGE, TILE_SHADE_8);

    for (u8 i = 0; i < 4; i++) {
        const ProjectedVertex *a = &verts[face->v[i]];
        const ProjectedVertex *b = &verts[face->v[(i + 1) & 3]];
        line(a->sx, a->sy, b->sx, b->sy, edge);
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
    draw_contact_shadow(verts);

    for (u8 i = 0; i < 6; i++) {
        u8 face_id = order[i].face;

        if (normals[face_id].z < 0) {
            fill_face_spans(verts, &cube_faces[face_id], normals[face_id]);
        }
    }

    for (u8 i = 0; i < 6; i++) {
        u8 face_id = order[i].face;

        if (normals[face_id].z < -16) {
            draw_face_edges(verts, &cube_faces[face_id]);
        }
    }

    frame_dirty = 1;
}

static void blit_frame(void) {
    if (!frame_dirty) return;

    *REG_VRAMMOD = 1;
    for (u8 chunk = 0; chunk < SPRITE_COLUMN_CHUNKS; chunk++) {
        u8 row_base = (u8)(chunk * SPRITE_CHUNK_CELLS);

        for (u8 x = 0; x < FRAME_W; x++) {
            u16 sprite = (u16)(SPRITE_BASE_INDEX + chunk * FRAME_W + x);

            *REG_VRAMADDR = (u16)(ADDR_SCB1 + (sprite * 64));
            for (u8 row = 0; row < SPRITE_CHUNK_CELLS; row++) {
                u16 word = frame_words[row_base + row][x];

                *REG_VRAMRW = sprite_tile(word);
                *REG_VRAMRW = sprite_attr(word);
            }
        }
    }

    frame_dirty = 0;
}

static u8 read_p1_input(void) {
    return (u8)~(*REG_P1CNT);
}

static u8 update_controls(u8 input, u8 *pitch, u8 *yaw, u8 *roll) {
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
    if (input & CNT_C) {
        if (camera_distance > CAMERA_DISTANCE_MIN) {
            camera_distance = (s16)(camera_distance - CAMERA_DISTANCE_STEP);
            changed = 1;
        }
    } else if (input & CNT_D) {
        if (camera_distance < CAMERA_DISTANCE_MAX) {
            camera_distance = (s16)(camera_distance + CAMERA_DISTANCE_STEP);
            changed = 1;
        }
    }

    return changed;
}

int main(void) {
    u8 pitch = 54;
    u8 yaw = 56;
    u8 roll = 0;

    init_palettes();
    clear_fix_map();
    init_sprite_framebuffer();
    render_cube(pitch, yaw, roll);
    ng_wait_vblank();
    blit_frame();

    for (;;) {
        if (update_controls(read_p1_input(), &pitch, &yaw, &roll)) {
            render_cube(pitch, yaw, roll);
        }
        ng_wait_vblank();
        blit_frame();
    }

    return 0;
}
