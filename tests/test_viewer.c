/* Host regressions for the primitive viewer; include the demo for static APIs. */
#define main viewer_demo_main
#include "../src/main.c"
#undef main
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void assert_frame_palettes(void) {
    for (u8 y=0;y<FRAME_H;y++) for (u8 x=0;x<FRAME_W;x++)
        assert(word_palette(frame_words[y][x]) < 16);
}

static void capture_viewer(const char *name) {
    const char *dir = getenv("VIEWER_CAPTURE_DIR");
    if (!dir) dir = "build/viewer/captures";
    char path[256]; snprintf(path,sizeof path,"%s/%s.ppm",dir,name);
    FILE *f=fopen(path,"wb"); assert(f);
    fprintf(f,"P6\n%u %u\n255\n",FRAME_W*4,FRAME_H*4);
    for (u8 y=0;y<FRAME_H;y++) for (u8 sy=0;sy<4;sy++) for (u8 x=0;x<FRAME_W;x++) {
        u8 palette = word_palette(frame_words[y][x]);
        u16 rgb12 = MMAP_PALBANK1[((u16)palette << 4) + 1];
        u8 value = (u8)((rgb12 & 15) * 17);
        for (u8 sx=0;sx<4;sx++) { fputc(value,f); fputc(value,f); fputc(value,f); }
    }
    fclose(f);
}

static void test_sine(void) {
    assert(SPRITE_CHUNK_CELLS == 16);
    assert(SPRITE_COLUMN_CHUNKS == 4);
    assert(FRAME_W * SPRITE_COLUMN_CHUNKS + SPRITE_BASE_INDEX <= SPRITE_MAX_DISPLAYABLE);
    assert(FRAME_W <= 96);
    unsigned total_rows = 0;
    for (u8 chunk = 0; chunk < SPRITE_COLUMN_CHUNKS; ++chunk) {
        u8 rows = sprite_chunk_rows(chunk);
        assert(rows <= 16 && rows % 4 == 0);
        total_rows += rows;
    }
    assert(total_rows == FRAME_H);
    for (u16 a=0;a<256;a++) {
        assert(sine((u8)a)>=-256 && sine((u8)a)<=256);
        assert(cosine((u8)a)>=-256 && cosine((u8)a)<=256);
        assert(sine((u8)(a+128))==-sine((u8)a));
        assert(cosine((u8)(a+128))==-cosine((u8)a));
    }
    assert(sine(0)==0 && sine(64)==256 && sine(128)==0 && sine(192)==-256);
    assert(cosine(0)==256 && cosine(64)==0);
}

static void test_coplanar_light(void) {
    const ModelData *cube=&models[0]; render_light=(Vec3){-260,260,-340};
    for (u16 yaw=0;yaw<256;yaw++) for (u8 i=0;i<cube->tri_count;i++)
        for (u8 j=(u8)(i+1);j<cube->tri_count;j++) {
            const ModelTri *a=&cube->tris[i],*b=&cube->tris[j];
            Vec3 na=a->normal,nb=b->normal;
            Vec3 va={cube->vertices[a->a].x,cube->vertices[a->a].y,cube->vertices[a->a].z};
            Vec3 vb={cube->vertices[b->a].x,cube->vertices[b->a].y,cube->vertices[b->a].z};
            s32 da=(s32)na.x*va.x+(s32)na.y*va.y+(s32)na.z*va.z;
            s32 db=(s32)nb.x*vb.x+(s32)nb.y*vb.y+(s32)nb.z*vb.z;
            if (na.x!=nb.x||na.y!=nb.y||na.z!=nb.z||da!=db) continue;
            assert(light_for_tri_position(cube,a,rotate_vec(na,0,(u8)yaw,0))==
                   light_for_tri_position(cube,b,rotate_vec(nb,0,(u8)yaw,0)));
        }
}

static void test_constant_fill(void) {
    ProjectedVertex v[3]={{8,8,1,8,8},{70,10,1,70,10},{38,48,1,38,48}};
    ModelTri t={0,1,2,{0,0,-256}}; clear_frame(); fill_tri_spans(v,&t,10);
    u8 palette=255;
    for (u8 y=0;y<FRAME_H;y++) for (u8 x=0;x<FRAME_W;x++)
        if (word_tile(frame_words[y][x])==TILE_SHADE_9) {
            u8 p=word_palette(frame_words[y][x]); if(palette==255)palette=p; else assert(p==palette);
        }
    assert(palette!=255);
}

static void test_controls(void) {
    u8 p1=216,y1=224,r1=0,p2=p1,y2=y1,r2=r1;
    for(u8 i=0;i<10;i++) update_controls(CNT_RIGHT,&p1,&y1,&r1,1);
    update_controls(CNT_RIGHT,&p2,&y2,&r2,10);
    assert(p1==p2&&y1==y2&&r1==r2);
    assert(viewer_elapsed_ticks(2,65534)==4);
    assert(viewer_elapsed_ticks(2,65535)==3);
    assert(viewer_elapsed_ticks(20,0)==8);
    camera_distance=CAMERA_DISTANCE_MIN;
    update_controls(CNT_C,&p1,&y1,&r1,1000); assert(camera_distance==CAMERA_DISTANCE_MIN);
    camera_distance=CAMERA_DISTANCE_MAX;
    update_controls(CNT_D,&p1,&y1,&r1,1000); assert(camera_distance==CAMERA_DISTANCE_MAX);
}

static void test_models(void) {
    init_palettes();
    for(u8 model=0;model<MODEL_COUNT;model++)
        for(u16 pitch=0;pitch<256;pitch+=64) for(u16 yaw=0;yaw<256;yaw+=64) {
            render_regular_model(model,(u8)pitch,(u8)yaw,0); assert_frame_palettes();
        }
    /* The control clamp test leaves the camera at MAX; native captures use
     * the demo's default distance and the angles observed in the probe. */
    camera_distance = CAMERA_DISTANCE_DEFAULT;
    render_regular_model(0,216,243,0); capture_viewer("cube-243");
    render_regular_model(1,216,175,0); capture_viewer("pyramid-175");
}

int main(void) {
    test_sine(); test_coplanar_light(); test_constant_fill(); test_controls(); test_models();
    puts("PASS: viewer sine, lighting, fills, elapsed controls, model angles and captures");
    return 0;
}
