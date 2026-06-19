#include <ngdevkit/neogeo.h>
#include <ngdevkit/ng-video.h>

#define FIX_COLS 40
#define FIX_MAP_ROWS 32
#define FIX_VISIBLE_ROWS 28
#define FIX_VISIBLE_ROW_OFFSET 2

#define FRAME_W FIX_COLS
#define FRAME_H FIX_VISIBLE_ROWS
#define Q 8
#define ONE (1 << Q)
#define CUBE_RADIUS 120
#define CAMERA_DISTANCE 700
#define PROJECT_SCALE_X 46
#define PROJECT_SCALE_Y 38

enum {
    TILE_BLANK = 0,
    TILE_LIGHT,
    TILE_MEDIUM,
    TILE_DARK,
    TILE_FULL,
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

static const s16 sin_lut[32] = {
    0, 50, 98, 142, 181, 213, 237, 251,
    256, 251, 237, 213, 181, 142, 98, 50,
    0, -50, -98, -142, -181, -213, -237, -251,
    -256, -251, -237, -213, -181, -142, -98, -50,
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

static inline s16 cosine(u8 angle) {
    return sin_lut[(angle + 8) & 31];
}

static inline s16 sine(u8 angle) {
    return sin_lut[angle & 31];
}

static inline void fix_poke(u8 col, u8 row, u8 palette, u16 tile) {
    *REG_VRAMADDR = (u16)(ADDR_FIXMAP + ((u16)col << 5) + row);
    *REG_VRAMMOD = 1;
    *REG_VRAMRW = (u16)(((u16)palette << 12) | tile);
}

static inline u16 fix_word(u8 palette, u16 tile) {
    return (u16)(((u16)palette << 12) | tile);
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

static Vec3 rotate_vec(Vec3 p, u8 angle_x, u8 angle_y) {
    s16 sx = sine(angle_x);
    s16 cx = cosine(angle_x);
    s16 sy = sine(angle_y);
    s16 cy = cosine(angle_y);

    s16 x1 = (s16)(qmul(p.x, cy) + qmul(p.z, sy));
    s16 z1 = (s16)(-qmul(p.x, sy) + qmul(p.z, cy));
    s16 y1 = p.y;

    Vec3 out;
    out.x = x1;
    out.y = (s16)(qmul(y1, cx) - qmul(z1, sx));
    out.z = (s16)(qmul(y1, sx) + qmul(z1, cx));
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
    u16 blank = fix_word(PAL_PAPER, TILE_BLANK);
    for (u8 y = 0; y < FRAME_H; y++) {
        for (u8 x = 0; x < FRAME_W; x++) {
            frame_words[y][x] = blank;
        }
    }
}

static u8 palette_for_normal(Vec3 normal) {
    Vec3 light = {-104, -150, -186};
    s16 intensity = (s16)(((s32)normal.x * light.x +
                           (s32)normal.y * light.y +
                           (s32)normal.z * light.z) >> Q);

    if (intensity > 105) return PAL_FACE_LIGHT;
    if (intensity > 25) return PAL_FACE_MID;
    return PAL_FACE_DARK;
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

static void line(s16 x0, s16 y0, s16 x1, s16 y1, u16 word) {
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
    u16 edge = fix_word(PAL_EDGE, TILE_LIGHT);

    for (u8 i = 0; i < 4; i++) {
        const ProjectedVertex *a = &verts[face->v[i]];
        const ProjectedVertex *b = &verts[face->v[(i + 1) & 3]];
        line(a->sx, a->sy, b->sx, b->sy, edge);
    }
}

static void render_cube(u8 angle_x, u8 angle_y) {
    ProjectedVertex verts[8];
    FaceOrder order[6];

    for (u8 i = 0; i < 8; i++) {
        verts[i] = project_vertex(rotate_vec(cube_vertices[i], angle_x, angle_y));
    }

    for (u8 i = 0; i < 6; i++) {
        order[i].face = i;
        order[i].z = face_depth(verts, &cube_faces[i]);
    }
    sort_faces(order);

    clear_frame();
    for (u8 i = 0; i < 6; i++) {
        const Face *face = &cube_faces[order[i].face];
        Vec3 normal = rotate_vec(face->normal, angle_x, angle_y);
        fill_face(verts, face, fix_word(palette_for_normal(normal), TILE_MEDIUM));
    }
    for (u8 i = 0; i < 6; i++) {
        const Face *face = &cube_faces[i];
        Vec3 normal = rotate_vec(face->normal, angle_x, angle_y);
        if (normal.z < 64) {
            draw_face_edges(verts, face);
        }
    }
}

static void blit_frame(void) {
    for (u8 y = 0; y < FRAME_H; y++) {
        for (u8 x = 0; x < FRAME_W; x++) {
            u16 word = frame_words[y][x];
            if (word != prev_words[y][x]) {
                fix_poke(x, (u8)(y + FIX_VISIBLE_ROW_OFFSET), (u8)(word >> 12), word & 0x0fff);
                prev_words[y][x] = word;
            }
        }
    }
}

static void clear_fix_map(void) {
    for (u8 x = 0; x < FIX_COLS; x++) {
        for (u8 y = 0; y < FIX_MAP_ROWS; y++) {
            fix_poke(x, y, 0, TILE_BLANK);
        }
    }

    for (u8 y = 0; y < FRAME_H; y++) {
        for (u8 x = 0; x < FRAME_W; x++) {
            prev_words[y][x] = 0xffff;
        }
    }
}

int main(void) {
    u8 angle_x = 3;
    u8 angle_y = 0;
    u8 frame = 0;

    init_palettes();
    clear_fix_map();

    for (;;) {
        render_cube(angle_x, angle_y);
        ng_wait_vblank();
        blit_frame();

        frame++;
        if ((frame & 3) == 0) {
            angle_x = (u8)((angle_x + 1) & 31);
            angle_y = (u8)((angle_y + 1) & 31);
        }
    }

    return 0;
}
