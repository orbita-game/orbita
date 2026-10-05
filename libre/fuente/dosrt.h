/*
 * dosrt.h - tiny runtime for real-mode DOS .COM programs built with gcc -m16.
 * Part of the Órbita DOS games. MIT License, Copyright (c) 2026 Órbita.
 *
 * Model: tiny (CS=DS=ES=SS). 386+ required (32-bit registers, FS/GS).
 *   GS -> drawing target (normally the VGA framebuffer at A000h)
 *   FS -> background buffer (a 64 KB segment right after the program)
 * ES must always equal DS when C code runs (gcc assumes a flat model),
 * so every inline asm that touches ES saves and restores it.
 */
#ifndef DOSRT_H
#define DOSRT_H

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;
typedef __seg_gs u8 gs8;
typedef __seg_fs u8 fs8;

#define SCR ((gs8 *)0) /* drawing target */
#define BGM ((fs8 *)0) /* background buffer */
#define VGA_SEG 0xA000

int main(void);
extern char __bss_start[], __bss_end[];

/* Entry point: must be the first bytes of the file (section .text.entry). */
__attribute__((naked, used, section(".text.entry"))) void _start(void)
{
	__asm__ volatile(
		"cli\n"
		"movzwl %sp, %esp\n" /* gcc uses 32-bit stack addressing */
		"sti\n"
		"cld\n"
		"mov $__bss_start, %di\n"
		"mov $__bss_end, %cx\n"
		"sub %di, %cx\n"
		"xor %al, %al\n"
		"rep stosb\n"
		"call main\n"
		"mov $0x4c00, %ax\n"
		"int $0x21\n");
}

/* gcc may emit calls to these */
void *memset(void *d, int c, unsigned long n)
{
	void *r = d;
	__asm__ volatile("rep stosb" : "+D"(d), "+c"(n) : "a"(c) : "memory");
	return r;
}
void *memcpy(void *d, const void *s, unsigned long n)
{
	void *r = d;
	__asm__ volatile("rep movsb" : "+D"(d), "+S"(s), "+c"(n) : : "memory");
	return r;
}

static inline void outb(u16 p, u8 v) { __asm__ volatile("outb %0, %1" : : "a"(v), "Nd"(p)); }
static inline u8 inb(u16 p)
{
	u8 v;
	__asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(p));
	return v;
}
static inline void set_gs(u16 s) { __asm__ volatile("mov %0, %%gs" : : "r"(s) : "memory"); }
static inline void set_fs(u16 s) { __asm__ volatile("mov %0, %%fs" : : "r"(s) : "memory"); }
static inline u16 get_cs(void)
{
	u16 s;
	__asm__ volatile("mov %%cs, %0" : "=r"(s));
	return s;
}

/* ---------- BIOS timer ---------- */
/* BIOS tick counter at 0040:006C, ~18.2 Hz, same on every machine */
static inline u32 ticks(void)
{
	u32 t;
	__asm__ volatile(
		"pushw %%fs\n"
		"mov $0x40, %%ax\n"
		"mov %%ax, %%fs\n"
		"movl %%fs:0x6c, %%eax\n"
		"popw %%fs\n"
		: "=a"(t) : : "memory");
	return t;
}
/* ticks elapsed since t0 (copes with the midnight wrap) */
static u32 since(u32 t0)
{
	u32 t = ticks();
	if (t < t0) return 1; /* midnight rollover: just count it as one tick */
	return t - t0;
}
static void wait_ticks(u32 n)
{
	u32 t0 = ticks();
	while (since(t0) < n) __asm__ volatile("hlt");
}

/* ---------- video ---------- */
static void set_mode(u16 m) { __asm__ volatile("int $0x10" : "+a"(m) : : "ebx", "ecx", "edx", "esi", "edi", "memory"); }
static void vsync(void)
{
	while (inb(0x3da) & 8) ;
	while (!(inb(0x3da) & 8)) ;
}
static void pal(u8 i, u8 r, u8 g, u8 b)
{
	outb(0x3c8, i);
	outb(0x3c9, r);
	outb(0x3c9, g);
	outb(0x3c9, b);
}

/* fill n bytes at GS:off with c */
static void fill_row(u16 off, u16 n, u8 c)
{
	__asm__ volatile(
		"pushw %%es\n"
		"mov %%gs, %%dx\n"
		"mov %%dx, %%es\n"
		"rep stosb\n"
		"popw %%es\n"
		: "+D"(off), "+c"(n) : "a"(c) : "edx", "memory");
}
/* copy n bytes FS:off -> GS:off (background to screen) */
static void copy_row(u16 off, u16 n)
{
	__asm__ volatile(
		"pushw %%ds\n"
		"pushw %%es\n"
		"mov %%gs, %%dx\n"
		"mov %%dx, %%es\n"
		"mov %%fs, %%dx\n"
		"mov %%dx, %%ds\n"
		"mov %%di, %%si\n"
		"rep movsb\n"
		"popw %%es\n"
		"popw %%ds\n"
		: "+D"(off), "+c"(n) : : "edx", "esi", "memory");
}
/* copy the whole 64000-byte background (FS) to the target (GS) */
static void copy_all(void)
{
	__asm__ volatile(
		"pushw %%ds\n"
		"pushw %%es\n"
		"mov %%gs, %%dx\n"
		"mov %%dx, %%es\n"
		"mov %%fs, %%dx\n"
		"mov %%dx, %%ds\n"
		"xor %%si, %%si\n"
		"xor %%di, %%di\n"
		"mov $16000, %%cx\n"
		"rep movsl\n"
		"popw %%es\n"
		"popw %%ds\n"
		: : : "ecx", "edx", "esi", "edi", "memory");
}

static void pset(int x, int y, u8 c)
{
	if ((unsigned)x < 320 && (unsigned)y < 200) SCR[y * 320 + x] = c;
}
static int clip(int *x, int *y, int *w, int *h)
{
	if (*x < 0) { *w += *x; *x = 0; }
	if (*y < 0) { *h += *y; *y = 0; }
	if (*x + *w > 320) *w = 320 - *x;
	if (*y + *h > 200) *h = 200 - *y;
	return *w > 0 && *h > 0;
}
static void rect(int x, int y, int w, int h, u8 c)
{
	if (!clip(&x, &y, &w, &h)) return;
	u16 o = y * 320 + x;
	while (h--) { fill_row(o, w, c); o += 320; }
}
static void frame(int x, int y, int w, int h, u8 c)
{
	rect(x, y, w, 1, c);
	rect(x, y + h - 1, w, 1, c);
	rect(x, y, 1, h, c);
	rect(x + w - 1, y, 1, h, c);
}
/* restore a rectangle of the screen from the background buffer */
static void restore(int x, int y, int w, int h)
{
	if (!clip(&x, &y, &w, &h)) return;
	u16 o = y * 320 + x;
	while (h--) { copy_row(o, w); o += 320; }
}

/* Flicker-free updates: draw a rectangle in an off-screen work buffer
   (starting from the background), then copy it to the screen in one go. */
static u16 bg_seg, work_seg;
static void band_begin(int x, int y, int w, int h)
{
	set_gs(work_seg);
	restore(x, y, w, h);
}
static void band_end(int x, int y, int w, int h)
{
	set_fs(work_seg);
	set_gs(VGA_SEG);
	restore(x, y, w, h);
	set_fs(bg_seg);
}

/* ---------- text (uses the 8x8 font from the machine's own BIOS ROM) ---------- */
static u8 font[128 * 8];
static void init_font(void)
{
	u16 seg, off;
	__asm__ volatile(
		"pushw %%bp\n"
		"pushw %%es\n"
		"int $0x10\n"
		"mov %%es, %%ax\n"
		"mov %%bp, %%dx\n"
		"popw %%es\n"
		"popw %%bp\n"
		: "=a"(seg), "=d"(off) : "a"(0x1130), "b"(0x0300) : "ecx", "memory");
	/* read seg:off with FS (FS is not set up yet at this point) */
	set_fs(seg);
	for (int i = 0; i < 128 * 8; i++) font[i] = BGM[(u16)(off + i)];
}
static int slen(const char *s)
{
	int n = 0;
	while (s[n]) n++;
	return n;
}
static void putch(int x, int y, u8 ch, u8 col, int sc)
{
	const u8 *g = font + (ch & 127) * 8;
	if (sc == 1 && x >= 0 && y >= 0 && x <= 312 && y <= 192) { /* fast path */
		u16 o = y * 320 + x;
		for (int r = 0; r < 8; r++, o += 320) {
			u8 b = g[r];
			for (int c = 0; b; c++, b <<= 1)
				if (b & 0x80) SCR[(u16)(o + c)] = col;
		}
		return;
	}
	for (int r = 0; r < 8; r++) {
		u8 b = g[r];
		for (int c = 0; c < 8; c++)
			if (b & (0x80 >> c)) {
				if (sc == 1) pset(x + c, y + r, col);
				else rect(x + c * sc, y + r * sc, sc, sc, col);
			}
	}
}
static void text(int x, int y, const char *s, u8 col, int sc)
{
	while (*s) { putch(x, y, *s++, col, sc); x += 8 * sc; }
}
/* text with a dark drop shadow */
static void textsh(int x, int y, const char *s, u8 col, int sc)
{
	text(x + sc, y + sc, s, 0, sc);
	text(x, y, s, col, sc);
}
static void ctext(int y, const char *s, u8 col, int sc)
{
	textsh(160 - slen(s) * 4 * sc, y, s, col, sc);
}
/* unsigned to decimal */
static char *utoa_(u32 v, char *buf)
{
	char t[12];
	int n = 0;
	do { t[n++] = '0' + v % 10; v /= 10; } while (v);
	for (int i = 0; i < n; i++) buf[i] = t[n - 1 - i];
	buf[n] = 0;
	return buf;
}

/* ---------- keyboard (BIOS int 16h) ---------- */
/* returns 0 if no key, else (scancode << 8) | ascii */
static u16 getkey(void)
{
	u16 k;
	__asm__ volatile(
		"mov $0x0100, %%ax\n"
		"int $0x16\n"
		"jz 1f\n"
		"xor %%ax, %%ax\n"
		"int $0x16\n"
		"or $0x8000, %%ax\n" /* never 0 for a real key (scan codes < 0x80) */
		"jmp 2f\n"
		"1: xor %%ax, %%ax\n"
		"2:\n"
		: "=a"(k) : : "cc", "memory");
	return k;
}
#define K_ESC 0x01
#define K_ENTER 0x1C
#define K_SPACE 0x39
#define K_UP 0x48
#define K_DOWN 0x50
#define K_LEFT 0x4B
#define K_RIGHT 0x4D
#define K_BKSP 0x0E
#define SCAN(k) (((k) >> 8) & 0x7f)
#define ASC(k) ((k) & 0xff)
static void flush_keys(void) { while (getkey()) ; }
static void typematic(u16 bx) { __asm__ volatile("int $0x16" : : "a"(0x0305), "b"(bx) : "memory"); }

/* ---------- PC speaker + note queue ---------- */
static void spk_on(u16 f)
{
	u16 d = 1193180UL / f;
	outb(0x43, 0xb6);
	outb(0x42, d & 255);
	outb(0x42, d >> 8);
	outb(0x61, inb(0x61) | 3);
}
static void spk_off(void) { outb(0x61, inb(0x61) & ~3); }

static u32 rseed = 1;
static u32 rnd(u32 n)
{
	rseed = rseed * 1103515245UL + 12345;
	return (rseed >> 16) % n;
}

#define SQN 32
static u16 sq_f[SQN];
static u8 sq_d[SQN];
static int sq_head, sq_tail, sq_busy;
static u32 sq_t0, sq_len;
static u16 sq_cur;
/* f = 0: silence, f = 1: noise; d = ticks */
static void note(u16 f, u8 d)
{
	int n = (sq_tail + 1) % SQN;
	if (n == sq_head) return;
	sq_f[sq_tail] = f;
	sq_d[sq_tail] = d;
	sq_tail = n;
}
static void sound_clear(void)
{
	sq_head = sq_tail = 0;
	sq_busy = 0;
	spk_off();
}
static void sound_update(void)
{
	if (sq_busy && since(sq_t0) >= sq_len) sq_busy = 0;
	if (!sq_busy) {
		if (sq_head == sq_tail) { spk_off(); return; }
		sq_cur = sq_f[sq_head];
		sq_len = sq_d[sq_head];
		sq_head = (sq_head + 1) % SQN;
		sq_t0 = ticks();
		sq_busy = 1;
		if (sq_cur == 0) spk_off();
		else if (sq_cur > 1) spk_on(sq_cur);
	}
	if (sq_cur == 1) spk_on(80 + rnd(400));
}

/* ---------- setup / shutdown ---------- */
/* mode 13h, font, background segment right after our 64 KB */
static void dos_init(void)
{
	u16 top;
	__asm__ volatile("movw 2, %0" : "=r"(top)); /* PSP: first segment beyond our memory */
	bg_seg = get_cs() + 0x1000;
	work_seg = bg_seg + 0x1000;
	if (top < work_seg + 0x1000) {
		const char *m = "Memoria insuficiente.\r\n$";
		__asm__ volatile("int $0x21" : : "a"(0x0900), "d"(m) : "memory");
		__asm__ volatile("mov $0x4c01, %ax\n int $0x21");
	}
	rseed = ticks() | 1;
	set_mode(0x13);
	init_font();
	set_fs(bg_seg);
	set_gs(VGA_SEG);
	typematic(0x0000); /* fastest key repeat: 250 ms delay, 30/s */
	flush_keys();
}
__attribute__((noreturn)) static void dos_exit(void)
{
	sound_clear();
	typematic(0x010B); /* back to the usual BIOS repeat rate */
	set_mode(0x03);
	flush_keys();
	__asm__ volatile("mov $0x4c00, %ax\n int $0x21");
	__builtin_unreachable();
}

#endif
