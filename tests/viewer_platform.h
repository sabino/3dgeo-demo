/* Host stand-in for the Neo Geo SDK registers used by the viewer demo. */
#ifndef VIEWER_PLATFORM_H
#define VIEWER_PLATFORM_H
#include <stdint.h>
typedef uint8_t u8; typedef int16_t s16; typedef uint16_t u16;
typedef int32_t s32; typedef uint32_t u32;
static volatile u8 viewer_reg_p1cnt, viewer_reg_status_b;
static volatile u16 viewer_reg_vramaddr, viewer_reg_vrammod, viewer_reg_vramrw;
static volatile u16 viewer_mmap_palbank1[4096];
#define REG_P1CNT (&viewer_reg_p1cnt)
#define REG_STATUS_B (&viewer_reg_status_b)
#define REG_VRAMADDR (&viewer_reg_vramaddr)
#define REG_VRAMMOD (&viewer_reg_vrammod)
#define REG_VRAMRW (&viewer_reg_vramrw)
#define MMAP_PALBANK1 viewer_mmap_palbank1
#define ADDR_FIXMAP 0x7000
#define VRAMADDRMODRW u16
#define CNT_UP 0x01
#define CNT_DOWN 0x02
#define CNT_LEFT 0x04
#define CNT_RIGHT 0x08
#define CNT_A 0x10
#define CNT_B 0x20
#define CNT_C 0x40
#define CNT_D 0x80
#define CNT_SELECT1 0x01
static inline void ng_wait_vblank(void) {}
#endif
