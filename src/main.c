#include <ngdevkit/neogeo.h>
#include <ngdevkit/ng-video.h>

#define FIX_COLS 40
#define FIX_MAP_ROWS 32
#define FIX_VISIBLE_ROWS 28
#define FIX_VISIBLE_ROW_OFFSET 2

#ifdef RENDERER_MICROFB
#define FRAME_W 80
#define FRAME_H 56
#define PROJECT_SCALE_X 92
#define PROJECT_SCALE_Y 76
#define SPRITE_ZOOM 0x033f
#define SPRITE_COLUMN_CHUNKS 2
#define SPRITE_CHUNK_CELLS (FRAME_H / SPRITE_COLUMN_CHUNKS)
#define SPRITE_BASE_INDEX 1
#elif defined(RENDERER_SPRITE40)
#define FRAME_W FIX_COLS
#define FRAME_H FIX_VISIBLE_ROWS
#define PROJECT_SCALE_X 46
#define PROJECT_SCALE_Y 38
#define SPRITE_ZOOM 0x077f
#define SPRITE_COLUMN_CHUNKS 1
#define SPRITE_CHUNK_CELLS FRAME_H
#define SPRITE_BASE_INDEX 0
#elif defined(RENDERER_SPRITE)
#define FRAME_W 20
#define FRAME_H 14
#define PROJECT_SCALE_X 23
#define PROJECT_SCALE_Y 19
#define SPRITE_ZOOM 0x0fff
#define SPRITE_COLUMN_CHUNKS 1
#define SPRITE_CHUNK_CELLS FRAME_H
#define SPRITE_BASE_INDEX 0
#else
#define FRAME_W FIX_COLS
#define FRAME_H FIX_VISIBLE_ROWS
#define PROJECT_SCALE_X 46
#define PROJECT_SCALE_Y 38
#endif

#define Q 8
#define ONE (1 << Q)
#define CUBE_RADIUS 120
#if defined(RENDERER_SPRITE40) || defined(RENDERER_MICROFB)
#define CAMERA_DISTANCE_DEFAULT 760
#define CAMERA_DISTANCE_MIN 420
#define CAMERA_DISTANCE_MAX 1300
#define CAMERA_DISTANCE_STEP 18
#else
#define CAMERA_DISTANCE_DEFAULT 700
#define CAMERA_DISTANCE_MIN 360
#define CAMERA_DISTANCE_MAX 1200
#define CAMERA_DISTANCE_STEP 24
#endif
#define ANGLE_MASK 63

#define FACE_LEVELS 12

#define ADDR_SCB1 0
#define ADDR_SCB2 0x8000
#define ADDR_SCB3 0x8200
#define SPRITE_START_TILE 256
#define SPRITE_SCREEN_X 0
#define SPRITE_SCREEN_Y_REGISTER 496
#define SPRITE_MAX_DISPLAYABLE 381

#if defined(RENDERER_SPRITE) || defined(RENDERER_SPRITE40) || defined(RENDERER_MICROFB)
#define RENDERER_USES_SPRITES 1
#endif

#if defined(RENDERER_SPRITE40) || defined(RENDERER_MICROFB)
#define RENDERER_MASKED_CUBE 1
#endif

#ifdef RENDERER_SPRITE40
#define RENDERER_DETAIL_MASKED_CUBE 1
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
#ifdef RENDERER_MASKED_CUBE
    TILE_MASK_1,
    TILE_MASK_2,
    TILE_MASK_3,
    TILE_MASK_4,
    TILE_MASK_5,
    TILE_MASK_6,
    TILE_MASK_7,
    TILE_MASK_8,
    TILE_MASK_9,
    TILE_MASK_10,
    TILE_MASK_11,
    TILE_MASK_12,
    TILE_MASK_13,
    TILE_MASK_14,
    TILE_MASK_15,
#endif
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

#ifdef RENDERER_DETAIL_MASKED_CUBE
typedef struct {
    s16 base;
    s16 origin_x;
    s16 origin_y;
    s16 ux;
    s16 uy;
    s16 vx;
    s16 vy;
    s16 inv_u_len;
    s16 inv_v_len;
    s16 g0;
    s16 g1;
    s16 g2;
    s16 g3;
    s16 g_center;
} FaceShade;

typedef struct {
    s32 a;
    s32 b;
    s32 c;
} EdgeEquation;

typedef struct {
    EdgeEquation edge[4];
    u8 positive_inside;
} FaceCoverage;

static const u8 face_adjacency[6][4] = {
    {2, 5, 3, 4},
    {2, 5, 3, 4},
    {0, 5, 1, 4},
    {0, 5, 1, 4},
    {0, 3, 1, 2},
    {0, 3, 1, 2},
};

#endif

static u16 frame_words[FRAME_H][FRAME_W];
static u16 prev_words[FRAME_H][FRAME_W];
static s16 camera_distance = CAMERA_DISTANCE_DEFAULT;
#ifdef RENDERER_MICROFB
static u8 frame_dirty = 1;
#endif

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

#ifdef RENDERER_MICROFB
static inline u16 sprite_y_word(u16 y, u16 height_tiles) {
    return (u16)(((((u16)(SPRITE_SCREEN_Y_REGISTER - y)) & 0x01ff) << 7) |
                 (height_tiles & 0x003f));
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
#endif

static inline void sprite_poke_cell(u8 col, u8 row, u16 word) {
#ifdef RENDERER_MICROFB
    u8 chunk = row / SPRITE_CHUNK_CELLS;
    u8 local_row = row - (chunk * SPRITE_CHUNK_CELLS);
    u16 sprite = (u16)(SPRITE_BASE_INDEX + chunk * FRAME_W + col);

    *REG_VRAMADDR = (u16)(ADDR_SCB1 + (sprite * 64) + ((u16)local_row * 2));
#else
    *REG_VRAMADDR = (u16)(ADDR_SCB1 + ((u16)col * 64) + ((u16)row * 2));
#endif
    *REG_VRAMMOD = 1;
    *REG_VRAMRW = sprite_tile(word);
    *REG_VRAMRW = sprite_attr(word);
}

static void init_sprite_framebuffer(void) {
    u16 blank = fix_word(PAL_PAPER, TILE_PAPER);

#ifdef RENDERER_MICROFB
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
            *REG_VRAMRW = SPRITE_ZOOM;
            *REG_VRAMRW = y_word;
            *REG_VRAMRW = (u16)((x * 4) << 7);
        }
    }
#else
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
#endif
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

#ifdef RENDERER_MASKED_CUBE
#ifndef RENDERER_MICROFB
static s16 light_for_normal(Vec3 normal) {
    Vec3 light = {-150, 70, -150};

    return (s16)(((s32)normal.x * light.x +
                  (s32)normal.y * light.y +
                  (s32)normal.z * light.z) >> Q);
}
#endif
#else
static s16 light_for_normal(Vec3 normal) {
    Vec3 light = {-104, -150, -186};

    return (s16)(((s32)normal.x * light.x +
                  (s32)normal.y * light.y +
                  (s32)normal.z * light.z) >> Q);
}
#endif

#ifdef RENDERER_MASKED_CUBE
static u8 face_level_for_intensity(s16 intensity) {
    s16 scaled = (s16)(((s32)(intensity + 130) * 16) / 36);

    if (scaled < 0) scaled = 0;
    if (scaled > (FACE_LEVELS - 1) * 16) scaled = (FACE_LEVELS - 1) * 16;

    scaled = (s16)(scaled + 8);
    if (scaled > (FACE_LEVELS - 1) * 16) scaled = (FACE_LEVELS - 1) * 16;

    return (u8)(scaled >> 4);
}

static u16 face_word_for_intensity_mask(s16 intensity, u8 mask) {
    u8 level = face_level_for_intensity(intensity);
    u16 tile = TILE_SHADE_9;

    if (mask != 0x0f) {
        tile = (u16)(TILE_MASK_1 + mask - 1);
    }

    return fix_word((u8)(PAL_FACE_0 + level), tile);
}
#else
static u16 face_word_for_intensity(s16 intensity) {
    if (intensity > 145) return fix_word(PAL_FACE_11, TILE_SHADE_1);
    if (intensity > 105) return fix_word(PAL_FACE_10, TILE_SHADE_2);
    if (intensity > 70) return fix_word(PAL_FACE_8, TILE_SHADE_3);
    if (intensity > 35) return fix_word(PAL_FACE_7, TILE_SHADE_4);
    if (intensity > 0) return fix_word(PAL_FACE_6, TILE_SHADE_5);
    if (intensity > -45) return fix_word(PAL_FACE_4, TILE_SHADE_5);
    if (intensity > -90) return fix_word(PAL_FACE_3, TILE_SHADE_6);
    if (intensity > -135) return fix_word(PAL_FACE_2, TILE_SHADE_7);
    return fix_word(PAL_FACE_1, TILE_SHADE_8);
}
#endif

#ifndef RENDERER_MASKED_CUBE
static u16 face_word_for_normal(Vec3 normal) {
    return face_word_for_intensity(light_for_normal(normal));
}
#endif
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

#ifdef RENDERER_DETAIL_MASKED_CUBE
static s32 edge_cross_q4(const ProjectedVertex *a, const ProjectedVertex *b, s16 px4, s16 py4) {
    s16 ax4 = (s16)(a->sx << 2);
    s16 ay4 = (s16)(a->sy << 2);
    s16 bx4 = (s16)(b->sx << 2);
    s16 by4 = (s16)(b->sy << 2);

    return (s32)(bx4 - ax4) * (py4 - ay4) - (s32)(by4 - ay4) * (px4 - ax4);
}

static void prepare_face_coverage(FaceCoverage *coverage, const ProjectedVertex verts[8],
                                  const Face *face) {
    s32 center_x4 = 0;
    s32 center_y4 = 0;
    s32 sign = 0;

    for (u8 i = 0; i < 4; i++) {
        const ProjectedVertex *a = &verts[face->v[i]];
        const ProjectedVertex *b = &verts[face->v[(i + 1) & 3]];
        s16 ax4 = (s16)(a->sx << 2);
        s16 ay4 = (s16)(a->sy << 2);
        s16 bx4 = (s16)(b->sx << 2);
        s16 by4 = (s16)(b->sy << 2);

        coverage->edge[i].a = (s32)(ay4 - by4);
        coverage->edge[i].b = (s32)(bx4 - ax4);
        coverage->edge[i].c = (s32)(by4 - ay4) * ax4 - (s32)(bx4 - ax4) * ay4;
        center_x4 += ax4;
        center_y4 += ay4;
    }

    center_x4 >>= 2;
    center_y4 >>= 2;

    for (u8 i = 0; i < 4; i++) {
        sign = coverage->edge[i].a * center_x4 +
               coverage->edge[i].b * center_y4 +
               coverage->edge[i].c;
        if (sign != 0) break;
    }

    coverage->positive_inside = (sign >= 0) ? 1 : 0;
}

static inline u8 edge_value_inside(const FaceCoverage *coverage, s32 value) {
    if (coverage->positive_inside) return value >= 0;
    return value <= 0;
}

static u8 coverage_mask_from_edges(const FaceCoverage *coverage, const s32 edge_value[4]) {
    static const s8 offsets[4][2] = {
        {1, 1},
        {3, 1},
        {1, 3},
        {3, 3},
    };
    u8 mask = 0;

    for (u8 sample = 0; sample < 4; sample++) {
        u8 inside = 1;
        s16 ox = offsets[sample][0];
        s16 oy = offsets[sample][1];

        for (u8 edge = 0; edge < 4; edge++) {
            s32 value = edge_value[edge] +
                        coverage->edge[edge].a * ox +
                        coverage->edge[edge].b * oy;
            if (!edge_value_inside(coverage, value)) {
                inside = 0;
                break;
            }
        }

        if (inside) {
            mask |= (u8)(1 << sample);
        }
    }

    return mask;
}

static u8 near_visible_neighbor_edge(const ProjectedVertex verts[8], const Face *face,
                                    u8 visible_neighbor_edges, s16 x, s16 y) {
    s16 px4 = (s16)((x << 2) + 2);
    s16 py4 = (s16)((y << 2) + 2);

    for (u8 i = 0; i < 4; i++) {
        if ((visible_neighbor_edges & (1 << i)) == 0) continue;

        const ProjectedVertex *a = &verts[face->v[i]];
        const ProjectedVertex *b = &verts[face->v[(i + 1) & 3]];
        s32 cross = edge_cross_q4(a, b, px4, py4);
        s16 dx = (s16)(b->sx - a->sx);
        s16 dy = (s16)(b->sy - a->sy);
        s16 len = (s16)(((dx < 0) ? -dx : dx) + ((dy < 0) ? -dy : dy));
        s32 threshold = (s32)(len + 1) * 7;

        if (cross < 0) cross = -cross;
        if (cross <= threshold) return 1;
    }

    return 0;
}

static s16 screen_light_bias(s16 x, s16 y) {
    return (s16)(((FRAME_H / 2 - y) * 5) - ((x - FRAME_W / 2) * 2));
}

static s16 clamp_q8(s16 value) {
    if (value < 0) return 0;
    if (value > ONE) return ONE;
    return value;
}

static void prepare_face_shade(FaceShade *shade, const ProjectedVertex verts[8], const Face *face, Vec3 normal) {
    const ProjectedVertex *v0 = &verts[face->v[0]];
    const ProjectedVertex *v1 = &verts[face->v[1]];
    const ProjectedVertex *v2 = &verts[face->v[2]];
    const ProjectedVertex *v3 = &verts[face->v[3]];
    s32 u_len;
    s32 v_len;

    shade->base = light_for_normal(normal);
    shade->origin_x = v0->sx;
    shade->origin_y = v0->sy;
    shade->ux = (s16)(v1->sx - v0->sx);
    shade->uy = (s16)(v1->sy - v0->sy);
    shade->vx = (s16)(v3->sx - v0->sx);
    shade->vy = (s16)(v3->sy - v0->sy);

    u_len = (s32)shade->ux * shade->ux + (s32)shade->uy * shade->uy;
    v_len = (s32)shade->vx * shade->vx + (s32)shade->vy * shade->vy;
    shade->inv_u_len = (u_len > 0) ? (s16)(4096 / u_len) : 0;
    shade->inv_v_len = (v_len > 0) ? (s16)(4096 / v_len) : 0;

    shade->g0 = screen_light_bias(v0->sx, v0->sy);
    shade->g1 = screen_light_bias(v1->sx, v1->sy);
    shade->g2 = screen_light_bias(v2->sx, v2->sy);
    shade->g3 = screen_light_bias(v3->sx, v3->sy);
    shade->g_center = (s16)((shade->g0 + shade->g1 + shade->g2 + shade->g3) >> 2);
}

static s16 face_local_gradient(const FaceShade *shade, s16 x, s16 y) {
    s16 px = (s16)(x - shade->origin_x);
    s16 py = (s16)(y - shade->origin_y);
    s16 u = 0;
    s16 v = 0;
    s16 gu0;
    s16 gu1;
    s16 g;

    if (shade->inv_u_len) {
        s32 dot_u = (s32)px * shade->ux + (s32)py * shade->uy;
        u = clamp_q8((s16)((dot_u * shade->inv_u_len) >> 4));
    }
    if (shade->inv_v_len) {
        s32 dot_v = (s32)px * shade->vx + (s32)py * shade->vy;
        v = clamp_q8((s16)((dot_v * shade->inv_v_len) >> 4));
    }

    gu0 = (s16)(shade->g0 + (((s32)(shade->g1 - shade->g0) * u) >> Q));
    gu1 = (s16)(shade->g3 + (((s32)(shade->g2 - shade->g3) * u) >> Q));
    g = (s16)(gu0 + (((s32)(gu1 - gu0) * v) >> Q));

    return (s16)(g - shade->g_center);
}

static void draw_contact_shadow(const ProjectedVertex verts[8]) {
    s16 min_x = FRAME_W - 1;
    s16 max_x = 0;
    s16 min_y = FRAME_H - 1;
    s16 max_y = 0;

    for (u8 i = 0; i < 8; i++) {
        if (verts[i].sx < min_x) min_x = verts[i].sx;
        if (verts[i].sx > max_x) max_x = verts[i].sx;
        if (verts[i].sy < min_y) min_y = verts[i].sy;
        if (verts[i].sy > max_y) max_y = verts[i].sy;
    }

    s16 cx = (s16)((min_x + max_x) >> 1);
    s16 cy = (s16)(max_y + 2);
    s16 rx = (s16)(((max_x - min_x) >> 1) + 2);
    s16 ry = (s16)(((max_y - min_y) / 9) + 1);

    if (rx < 5) rx = 5;
    if (rx > 14) rx = 14;
    if (ry < 2) ry = 2;
    if (ry > 4) ry = 4;

    s16 x0 = (s16)(cx - rx);
    s16 x1 = (s16)(cx + rx);
    s16 y0 = (s16)(cy - ry);
    s16 y1 = (s16)(cy + ry);

    if (x0 < 0) x0 = 0;
    if (x1 >= FRAME_W) x1 = FRAME_W - 1;
    if (y0 < 0) y0 = 0;
    if (y1 >= FRAME_H) y1 = FRAME_H - 1;

    for (s16 y = y0; y <= y1; y++) {
        for (s16 x = x0; x <= x1; x++) {
            s16 dx = (s16)(x - cx);
            s16 dy = (s16)(y - cy);
            s16 d = (s16)(((s32)dx * dx * 16) / ((s32)rx * rx) +
                          ((s32)dy * dy * 16) / ((s32)ry * ry));

            if (d <= 16) {
                if (d < 7) {
                    frame_words[y][x] = fix_word(PAL_SHADOW_DARK, TILE_SHADE_2);
                } else {
                    frame_words[y][x] = fix_word(PAL_SHADOW_LIGHT, TILE_SHADE_1);
                }
            }
        }
    }
}
#endif

#ifndef RENDERER_MASKED_CUBE
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
#endif

#ifndef RENDERER_MASKED_CUBE
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
#endif

#ifdef RENDERER_DETAIL_MASKED_CUBE
static void fill_face_lit(const ProjectedVertex verts[8], const Face *face, u8 face_id,
                          const Vec3 normals[6]) {
    s16 min_x = FRAME_W - 1;
    s16 max_x = 0;
    s16 min_y = FRAME_H - 1;
    s16 max_y = 0;
    FaceShade shade;
    FaceCoverage coverage;
    u8 visible_neighbor_edges = 0;

    prepare_face_shade(&shade, verts, face, normals[face_id]);
    prepare_face_coverage(&coverage, verts, face);

    for (u8 i = 0; i < 4; i++) {
        if (normals[face_adjacency[face_id][i]].z < 0) {
            visible_neighbor_edges |= (u8)(1 << i);
        }
    }

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
        s32 edge_value[4];

        for (u8 edge = 0; edge < 4; edge++) {
            edge_value[edge] = coverage.edge[edge].a * (s16)(min_x << 2) +
                               coverage.edge[edge].b * (s16)(y << 2) +
                               coverage.edge[edge].c;
        }

        for (s16 x = min_x; x <= max_x; x++) {
            u8 mask = coverage_mask_from_edges(&coverage, edge_value);
            if (mask) {
                s16 gradient = face_local_gradient(&shade, x, y);
                if (mask != 0x0f &&
                    near_visible_neighbor_edge(verts, face, visible_neighbor_edges, x, y)) {
                    mask = 0x0f;
                }
                frame_words[y][x] = face_word_for_intensity_mask((s16)(shade.base + gradient), mask);
            }

            for (u8 edge = 0; edge < 4; edge++) {
                edge_value[edge] += coverage.edge[edge].a << 2;
            }
        }
    }
}
#endif

#ifdef RENDERER_MICROFB
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
    s16 dy;
    s32 x;
    s32 step;
    s16 start_y;
    s16 end_y;

    if (y0 == y1) return;
    if (y0 > y1) {
        s16 tx = x0;
        s16 ty = y0;
        x0 = x1;
        y0 = y1;
        x1 = tx;
        y1 = ty;
    }

    dy = (s16)(y1 - y0);
    step = ((s32)(x1 - x0) << Q) / dy;
    start_y = y0;
    end_y = y1;

    if (start_y < 0) start_y = 0;
    if (end_y > FRAME_H) end_y = FRAME_H;
    if (start_y >= end_y) return;

    x = ((s32)x0 << Q) + step * (start_y - y0);
    for (s16 y = start_y; y < end_y; y++) {
        s16 xi = (s16)(x >> Q);

        if (xi < left[y]) left[y] = xi;
        if (xi > right[y]) right[y] = xi;
        x += step;
    }
}

static void fill_face_fast_spans(const ProjectedVertex verts[8], const Face *face, Vec3 normal) {
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

        if (x0 <= x1) {
            s16 mid;
            s16 row_bias;
            u16 word;

            if (x0 < 0) x0 = 0;
            if (x1 >= FRAME_W) x1 = FRAME_W - 1;
            if (x0 > x1) continue;

            mid = (s16)((x0 + x1) >> 1);
            row_bias = (s16)((FRAME_H / 2 - y) - ((mid - FRAME_W / 2) >> 1));
            word = face_word_for_intensity_mask((s16)(base + row_bias), 0x0f);

            for (s16 x = x0; x <= x1; x++) {
                frame_words[y][x] = word;
            }
        }
    }
}

static void draw_contact_shadow_fast(const ProjectedVertex verts[8]) {
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

static void microfb_put_edge(s16 x, s16 y, u16 word) {
    if (x >= 0 && x < FRAME_W && y >= 0 && y < FRAME_H) {
        frame_words[y][x] = word;
    }
}

static void microfb_line(s16 x0, s16 y0, s16 x1, s16 y1, u16 word) {
    s16 dx = (x0 < x1) ? (x1 - x0) : (x0 - x1);
    s16 sx = (x0 < x1) ? 1 : -1;
    s16 dy = (y0 < y1) ? (y0 - y1) : (y1 - y0);
    s16 sy = (y0 < y1) ? 1 : -1;
    s16 err = (s16)(dx + dy);

    for (;;) {
        microfb_put_edge(x0, y0, word);
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

static void draw_face_edges_fast(const ProjectedVertex verts[8], const Face *face) {
    u16 edge = fix_word(PAL_EDGE, TILE_SHADE_8);

    for (u8 i = 0; i < 4; i++) {
        const ProjectedVertex *a = &verts[face->v[i]];
        const ProjectedVertex *b = &verts[face->v[(i + 1) & 3]];
        microfb_line(a->sx, a->sy, b->sx, b->sy, edge);
    }
}
#endif

#ifndef RENDERER_MASKED_CUBE
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
#endif

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
#ifdef RENDERER_MICROFB
    draw_contact_shadow_fast(verts);
#elif defined(RENDERER_MASKED_CUBE)
    draw_contact_shadow(verts);
#endif
    for (u8 i = 0; i < 6; i++) {
        const Face *face = &cube_faces[order[i].face];
#ifdef RENDERER_MICROFB
        u8 face_id = order[i].face;
        if (normals[face_id].z < 0) {
            fill_face_fast_spans(verts, face, normals[face_id]);
        }
#elif defined(RENDERER_MASKED_CUBE)
        u8 face_id = order[i].face;
        if (normals[face_id].z < 0) {
            fill_face_lit(verts, face, face_id, normals);
        }
#else
        fill_face(verts, face, face_word_for_normal(normals[order[i].face]));
#endif
    }
#ifdef RENDERER_MICROFB
    for (u8 i = 0; i < 6; i++) {
        u8 face_id = order[i].face;
        if (normals[face_id].z < -16) {
            draw_face_edges_fast(verts, &cube_faces[face_id]);
        }
    }
#endif
#ifndef RENDERER_MASKED_CUBE
    for (u8 i = 0; i < 6; i++) {
        u8 face_id = order[i].face;
        if (normals[face_id].z < -16) {
            draw_face_edges(verts, &cube_faces[face_id]);
        }
    }
#endif
#ifdef RENDERER_MICROFB
    frame_dirty = 1;
#endif
}

static void blit_frame(void) {
#ifdef RENDERER_MICROFB
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
#elif defined(RENDERER_USES_SPRITES)
    for (u8 y = 0; y < FRAME_H; y++) {
        for (u8 x = 0; x < FRAME_W; x++) {
            u16 word = frame_words[y][x];
            if (word != prev_words[y][x]) {
                sprite_poke_cell(x, y, word);
                prev_words[y][x] = word;
            }
        }
    }
#else
    for (u8 y = 0; y < FRAME_H; y++) {
        u8 row = (u8)(y + FIX_VISIBLE_ROW_OFFSET);
        u8 x = 0;

        while (x < FRAME_W) {
            while (x < FRAME_W && frame_words[y][x] == prev_words[y][x]) {
                x++;
            }
            if (x >= FRAME_W) break;

            *REG_VRAMMOD = 32;
            *REG_VRAMADDR = (u16)(ADDR_FIXMAP + ((u16)x << 5) + row);

            while (x < FRAME_W) {
                u16 word = frame_words[y][x];
                if (word == prev_words[y][x]) break;

                *REG_VRAMRW = word;
                prev_words[y][x] = word;
                x++;
            }
        }
    }
#endif
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
#ifdef RENDERER_MASKED_CUBE
    u8 pitch = 54;
    u8 yaw = 56;
#else
    u8 pitch = 4;
    u8 yaw = 5;
#endif
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
        if (update_controls(read_p1_input(), &pitch, &yaw, &roll)) {
            render_cube(pitch, yaw, roll);
        }
        ng_wait_vblank();
        blit_frame();
    }

    return 0;
}
