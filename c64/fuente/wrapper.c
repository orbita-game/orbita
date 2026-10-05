/*
    c64 wasm wrapper for "Orbita": exposes a tiny C API around floooh/chips c64.h
    (zlib license, see chips/LICENSE). No ROM data is compiled in: KERNAL, BASIC
    and CHARGEN images are copied in from JavaScript at runtime (MEGA65 Open ROMs).
*/
#define CHIPS_IMPL
#define CHIPS_ASSERT(c) ((void)0)
#include <assert.h>
#include "chips/chips_common.h"
#include "chips/m6502.h"
#include "chips/m6526.h"
#include "chips/m6569.h"
#include "chips/m6581.h"
#include "chips/kbd.h"
#include "chips/mem.h"
#include "chips/clk.h"
#include "systems/c1530.h"
#include "chips/m6522.h"
#include "systems/c1541.h"
#include "systems/c64.h"

#define EXPORT(name) __attribute__((export_name(#name)))

static c64_t sys;
static uint8_t rom_kernal[0x2000];
static uint8_t rom_basic[0x2000];
static uint8_t rom_chars[0x1000];
static uint8_t io_buf[0x10000 + 2];             /* PRG upload buffer */
static uint32_t rgba[M6569_FRAMEBUFFER_SIZE_BYTES]; /* RGBA output */

/* audio ring buffer (mono float) */
#define AUDIO_RING (16384)
static float audio_ring[AUDIO_RING];
static uint32_t audio_wr = 0;  /* total samples written (wraps) */

static void audio_cb(const float* samples, int num_samples, void* user_data) {
    (void)user_data;
    for (int i = 0; i < num_samples; i++) {
        audio_ring[audio_wr & (AUDIO_RING - 1)] = samples[i];
        audio_wr++;
    }
}

EXPORT(rom_ptr) uint8_t* w_rom_ptr(int which) {
    switch (which) { case 0: return rom_kernal; case 1: return rom_basic; default: return rom_chars; }
}
EXPORT(io_ptr) uint8_t* w_io_ptr(void) { return io_buf; }
EXPORT(io_size) int w_io_size(void) { return (int)sizeof(io_buf); }

EXPORT(init) void w_init(int sample_rate) {
    c64_init(&sys, &(c64_desc_t){
        .c1530_enabled = false,
        .c1541_enabled = false,
        .joystick_type = C64_JOYSTICKTYPE_DIGITAL_2,
        .audio = {
            .callback = { .func = audio_cb, .user_data = 0 },
            .num_samples = 128,
            .sample_rate = sample_rate > 0 ? sample_rate : 44100,
            .volume = 1.0f,
        },
        .roms = {
            .chars  = { .ptr = rom_chars,  .size = sizeof(rom_chars) },
            .basic  = { .ptr = rom_basic,  .size = sizeof(rom_basic) },
            .kernal = { .ptr = rom_kernal, .size = sizeof(rom_kernal) },
        },
    });
    /* keymap fixes on top of chips' table (done here, chips stays unmodified):
       - chips registers '0' a second time in its shift layer, so typing '0'
         sent SHIFT+0 (nothing on a real C64); re-register it unshifted
       - add the up-arrow key (matrix column 6, line 6) as ASCII '^' */
    kbd_register_key(&sys.kbd, '0', 3, 4, 0);
    kbd_register_key(&sys.kbd, '^', 6, 6, 0);
}
EXPORT(reset) void w_reset(void) { c64_reset(&sys); }
EXPORT(exec) uint32_t w_exec(uint32_t us) { return c64_exec(&sys, us); }
EXPORT(key_down) void w_key_down(int code) { kbd_key_down(&sys.kbd, code); }
EXPORT(key_up) void w_key_up(int code) { kbd_key_up(&sys.kbd, code); }
EXPORT(joystick) void w_joystick(int j1, int j2) { c64_joystick(&sys, (uint8_t)j1, (uint8_t)j2); }
EXPORT(quickload) int w_quickload(int size) {
    return c64_quickload(&sys, (chips_range_t){ .ptr = io_buf, .size = (size_t)size }) ? 1 : 0;
}
EXPORT(peek) int w_peek(int addr) { return mem_rd(&sys.mem_cpu, (uint16_t)addr); }
EXPORT(poke) void w_poke(int addr, int val) { mem_wr(&sys.mem_cpu, (uint16_t)addr, (uint8_t)val); }
EXPORT(ram_ptr) uint8_t* w_ram_ptr(void) { return sys.ram; }
EXPORT(pc) int w_pc(void) { return m6502_pc(&sys.cpu); }

EXPORT(screen_w) int w_screen_w(void) { return c64_display_info(&sys).screen.width; }
EXPORT(screen_h) int w_screen_h(void) { return c64_display_info(&sys).screen.height; }

/* convert the visible part of the indexed framebuffer to RGBA (tightly packed
   at screen_w x screen_h), cropped by (cx, cy, cw, ch); returns pointer */
EXPORT(render) uint32_t* w_render(int cx, int cy, int cw, int ch) {
    const uint32_t* pal = (const uint32_t*) m6569_palette().ptr;
    uint32_t* dst = rgba;
    for (int y = 0; y < ch; y++) {
        const uint8_t* src = sys.fb + (cy + y) * M6569_FRAMEBUFFER_WIDTH + cx;
        for (int x = 0; x < cw; x++) {
            *dst++ = pal[src[x] & 15];
        }
    }
    return rgba;
}

EXPORT(audio_ptr) float* w_audio_ptr(void) { return audio_ring; }
EXPORT(audio_ring_size) int w_audio_ring_size(void) { return AUDIO_RING; }
EXPORT(audio_written) uint32_t w_audio_written(void) { return audio_wr; }
EXPORT(sizeof_c64) int w_sizeof_c64(void) { return (int)sizeof(c64_t); }
