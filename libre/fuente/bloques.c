/*
 * TETRIS.COM - falling blocks for DOS (VGA mode 13h, 386+).
 * Original code for Órbita. MIT License, Copyright (c) 2026 Órbita.
 *
 * Controls: Left/Right move, Up rotate, Down soft drop, Space hard drop,
 * P pause, ESC back to DOS.
 */
#include "dosrt.h"

#define BW 10
#define BH 20
#define CSZ 9
#define BX0 115
#define BY0 10

/* palette indices */
#define C_WELL 16
#define C_GRID 17
#define C_PANEL 18
#define C_EDGE 19
#define C_EDGE2 20
#define C_SKY 64   /* 64..95 gradient */
#define C_STAR 96  /* 96..99 */
#define C_PIECE 32 /* 32 + 4*p + {light, mid, dark, ghost} */

/* pieces: I O T S Z J L, 4x4 grids; n = rotation box size (0 = no rotation) */
static const char *const pdef[7] = {
	"....XXXX........",
	".XX..XX.........",
	".X..XXX.........",
	".XX.XX..........",
	"XX...XX.........",
	"X...XXX.........",
	"..X.XXX.........",
};
static const u8 pbox[7] = {4, 0, 3, 3, 3, 3, 3};
static const u8 pcol[7][3] = {
	{0, 50, 58}, {60, 54, 0}, {44, 12, 54}, {8, 50, 14}, {58, 10, 12}, {12, 24, 62}, {62, 34, 0},
};
static u16 shp[7][4];

static u8 board[BH][BW];
static u8 shown[BH][BW];
static u8 want[BH][BW];

static int cur, crot, cx, cy, nxt;
static u32 score, lines, level;
static u8 bag[7];
static int bagn;
static u8 clearing[BH];
static int nclear;

static void make_shapes(void)
{
	for (int p = 0; p < 7; p++) {
		u8 g[4][4], t[4][4];
		for (int i = 0; i < 16; i++) g[i / 4][i % 4] = pdef[p][i] == 'X';
		int n = pbox[p];
		for (int r = 0; r < 4; r++) {
			u16 m = 0;
			for (int i = 0; i < 16; i++)
				if (g[i / 4][i % 4]) m |= 1 << i;
			shp[p][r] = m;
			if (n) {
				for (int y = 0; y < 4; y++)
					for (int x = 0; x < 4; x++) t[y][x] = 0;
				for (int y = 0; y < n; y++)
					for (int x = 0; x < n; x++) t[y][x] = g[n - 1 - x][y];
				for (int i = 0; i < 16; i++) g[i / 4][i % 4] = t[i / 4][i % 4];
			}
		}
	}
}

static int fits(int p, int r, int x, int y)
{
	u16 m = shp[p][r & 3];
	for (int i = 0; i < 16; i++) {
		if (!(m & (1 << i))) continue;
		int bx = x + i % 4, by = y + i / 4;
		if (bx < 0 || bx >= BW || by >= BH) return 0;
		if (by >= 0 && board[by][bx]) return 0;
	}
	return 1;
}

static int next_from_bag(void)
{
#ifdef TEST_ONLY_O
	return 1; /* test build: only O pieces */
#endif
	if (!bagn) {
		for (int i = 0; i < 7; i++) bag[i] = i;
		for (int i = 6; i > 0; i--) {
			int j = rnd(i + 1);
			u8 t = bag[i];
			bag[i] = bag[j];
			bag[j] = t;
		}
		bagn = 7;
	}
	return bag[--bagn];
}

/* ---------- drawing ---------- */
static void setup_palette(void)
{
	pal(C_WELL, 3, 3, 8);
	pal(C_GRID, 7, 7, 14);
	pal(C_PANEL, 5, 5, 12);
	pal(C_EDGE, 20, 22, 40);
	pal(C_EDGE2, 40, 44, 60);
	for (int i = 0; i < 32; i++) pal(C_SKY + i, 2 + i / 4, 1 + i / 6, 10 + i / 2);
	pal(C_STAR, 63, 63, 63);
	pal(C_STAR + 1, 40, 40, 50);
	pal(C_STAR + 2, 25, 25, 35);
	pal(C_STAR + 3, 63, 50, 30);
	for (int p = 0; p < 7; p++) {
		const u8 *c = pcol[p];
		u8 b = C_PIECE + p * 4;
		pal(b, c[0] + (63 - c[0]) / 2, c[1] + (63 - c[1]) / 2, c[2] + (63 - c[2]) / 2);
		pal(b + 1, c[0], c[1], c[2]);
		pal(b + 2, c[0] / 2, c[1] / 2, c[2] / 2);
		pal(b + 3, c[0] / 3 + 3, c[1] / 3 + 3, c[2] / 3 + 6);
	}
}

static void block(int x, int y, int p)
{
	u8 b = C_PIECE + p * 4;
	rect(x, y, CSZ, CSZ, b + 1);
	rect(x, y, CSZ, 1, b);
	rect(x, y, 1, CSZ, b);
	rect(x, y + CSZ - 1, CSZ, 1, b + 2);
	rect(x + CSZ - 1, y + 1, 1, CSZ - 1, b + 2);
	rect(x + 2, y + 2, 2, 2, b);
}

static void panel(int x, int y, int w, int h)
{
	rect(x, y, w, h, C_PANEL);
	frame(x, y, w, h, C_EDGE);
	frame(x - 1, y - 1, w + 2, h + 2, 0);
}

/* static background into the FS buffer, then shown */
static void draw_background(void)
{
	set_gs(bg_seg);
	for (int y = 0; y < 200; y++) rect(0, y, 320, 1, C_SKY + y * 32 / 200);
	for (int i = 0; i < 90; i++) pset(rnd(320), rnd(200), C_STAR + rnd(4));
	/* planet in the corner, an orbit nod */
	for (int y = -40; y <= 40; y++)
		for (int x = -40; x <= 40; x++)
			if (x * x + y * y <= 1600) pset(290 + x, 200 + y, (x + y < -20) ? C_EDGE2 : C_EDGE);
	/* well */
	frame(BX0 - 3, BY0 - 3, BW * CSZ + 6, BH * CSZ + 6, 0);
	frame(BX0 - 2, BY0 - 2, BW * CSZ + 4, BH * CSZ + 4, C_EDGE2);
	frame(BX0 - 1, BY0 - 1, BW * CSZ + 2, BH * CSZ + 2, C_EDGE);
	/* left panel: score */
	panel(8, 10, 98, 112);
	textsh(16, 18, "PUNTOS", 14, 1);
	textsh(16, 52, "LINEAS", 14, 1);
	textsh(16, 86, "NIVEL", 14, 1);
	panel(8, 132, 98, 58);
	textsh(17, 140, "BLOQUES", 11, 1);
	text(17, 156, "para", 7, 1);
	text(17, 168, "ORBITA", 13, 1);
	/* right panel: next + help */
	panel(214, 10, 98, 62);
	textsh(222, 18, "SIGUIENTE", 14, 1);
	panel(214, 82, 98, 108);
	text(220, 88, "<- -> mover", 7, 1);
	text(220, 100, "ARRIBA gira", 7, 1);
	text(220, 112, "ABAJO baja", 7, 1);
	text(220, 124, "ESPACIO", 7, 1);
	text(220, 134, "  cae", 7, 1);
	text(220, 148, "P pausa", 7, 1);
	text(220, 160, "ESC salir", 7, 1);
	set_gs(VGA_SEG);
	vsync();
	copy_all();
}

static void draw_number(int y, u32 v)
{
	char b[12];
	band_begin(16, y, 84, 16);
	textsh(16, y, utoa_(v, b), 15, 2);
	band_end(16, y, 84, 16);
}
static void draw_stats(void)
{
	draw_number(30, score);
	draw_number(64, lines);
	draw_number(98, level);
}
static void draw_next(void)
{
	band_begin(222, 30, 84, 38);
	u16 m = shp[nxt][0];
	int minr = 4, maxr = 0, minc = 4, maxc = 0;
	for (int i = 0; i < 16; i++)
		if (m & (1 << i)) {
			int r = i / 4, c = i % 4;
			if (r < minr) minr = r;
			if (r > maxr) maxr = r;
			if (c < minc) minc = c;
			if (c > maxc) maxc = c;
		}
	int w = (maxc - minc + 1) * CSZ, h = (maxr - minr + 1) * CSZ;
	int ox = 263 - w / 2, oy = 49 - h / 2;
	for (int i = 0; i < 16; i++)
		if (m & (1 << i)) block(ox + (i % 4 - minc) * CSZ, oy + (i / 4 - minr) * CSZ, nxt);
	band_end(222, 30, 84, 38);
}

static int ghost_y(void)
{
	int y = cy;
	while (fits(cur, crot, cx, y + 1)) y++;
	return y;
}

static int playing; /* a piece is on the board */
static int flash_on;

/* compose what each cell should look like and redraw only what changed */
static void draw_board(void)
{
	for (int r = 0; r < BH; r++)
		for (int c = 0; c < BW; c++) want[r][c] = board[r][c];
	if (nclear && flash_on)
		for (int r = 0; r < BH; r++)
			if (clearing[r])
				for (int c = 0; c < BW; c++) want[r][c] = 16;
	if (playing) {
		u16 m = shp[cur][crot];
		int gy = ghost_y();
		for (int i = 0; i < 16; i++)
			if (m & (1 << i)) {
				int x = cx + i % 4, y = gy + i / 4;
				if (y >= 0 && !want[y][x]) want[y][x] = 8 + cur + 1;
			}
		for (int i = 0; i < 16; i++)
			if (m & (1 << i)) {
				int x = cx + i % 4, y = cy + i / 4;
				if (y >= 0) want[y][x] = cur + 1;
			}
	}
	for (int r = 0; r < BH; r++)
		for (int c = 0; c < BW; c++) {
			u8 v = want[r][c];
			if (v == shown[r][c]) continue;
			shown[r][c] = v;
			int x = BX0 + c * CSZ, y = BY0 + r * CSZ;
			if (v == 0) {
				rect(x, y, CSZ, CSZ, C_WELL);
				pset(x + CSZ - 1, y + CSZ - 1, C_GRID);
			} else if (v <= 7) {
				block(x, y, v - 1);
			} else if (v == 16) {
				rect(x, y, CSZ, CSZ, 15);
			} else {
				rect(x, y, CSZ, CSZ, C_WELL);
				frame(x, y, CSZ, CSZ, C_PIECE + (v - 9) * 4 + 3);
			}
		}
}
static void invalidate_board(void)
{
	for (int r = 0; r < BH; r++)
		for (int c = 0; c < BW; c++) shown[r][c] = 0xff;
}

/* centered message box over the well */
static void msgbox(const char *big, const char *l1, const char *l2, const char *l3)
{
	rect(46, 66, 228, 70, 0);
	rect(48, 68, 224, 66, C_PANEL);
	frame(48, 68, 224, 66, C_EDGE2);
	ctext(76, big, 14, 2);
	ctext(98, l1, 15, 1);
	ctext(112, l2, 7, 1);
	ctext(122, l3, 7, 1);
}
static void close_box(void)
{
	restore(46, 66, 228, 70);
	invalidate_board();
	draw_board();
}

/* ---------- game ---------- */
static const u8 speed_tab[] = {16, 14, 12, 11, 10, 9, 8, 7, 6, 5, 4, 4, 3, 3, 3, 2, 2, 2, 2, 1};
static u32 fall_delay(void)
{
	u32 l = level - 1;
	if (l >= sizeof speed_tab) l = sizeof speed_tab - 1;
	return speed_tab[l];
}

static int spawn(void)
{
	cur = nxt;
	nxt = next_from_bag();
	crot = 0;
	cx = 3;
	cy = cur == 0 ? -1 : 0;
	playing = 1;
	draw_next();
	return fits(cur, crot, cx, cy);
}

static int try_rotate(int dir)
{
	static const signed char kx[] = {0, -1, 1, -2, 2, 0};
	static const signed char ky[] = {0, 0, 0, 0, 0, -1};
	int nr = (crot + dir) & 3;
	for (int i = 0; i < 6; i++)
		if (fits(cur, nr, cx + kx[i], cy + ky[i])) {
			crot = nr;
			cx += kx[i];
			cy += ky[i];
			return 1;
		}
	return 0;
}

/* returns 0 = keep going, 1 = game over */
static int lock_piece(void)
{
	u16 m = shp[cur][crot];
	int over = 0;
	for (int i = 0; i < 16; i++)
		if (m & (1 << i)) {
			int x = cx + i % 4, y = cy + i / 4;
			if (y < 0) over = 1;
			else board[y][x] = cur + 1;
		}
	playing = 0;
	if (over) return 1;
	nclear = 0;
	for (int r = 0; r < BH; r++) {
		int full = 1;
		for (int c = 0; c < BW; c++)
			if (!board[r][c]) full = 0;
		clearing[r] = full;
		nclear += full;
	}
	if (!nclear) note(140, 1);
	return 0;
}

static void remove_lines(void)
{
	static const u16 pts[5] = {0, 100, 300, 500, 800};
	int dst = BH - 1;
	for (int r = BH - 1; r >= 0; r--) {
		if (clearing[r]) continue;
		if (dst != r)
			for (int c = 0; c < BW; c++) board[dst][c] = board[r][c];
		dst--;
	}
	for (; dst >= 0; dst--)
		for (int c = 0; c < BW; c++) board[dst][c] = 0;
	u32 oldlvl = level;
	score += pts[nclear] * level;
	lines += nclear;
	level = lines / 10 + 1;
	if (level > 99) level = 99;
	for (int r = 0; r < BH; r++) clearing[r] = 0;
	if (level != oldlvl) {
		note(523, 2); note(659, 2); note(784, 2); note(1047, 4);
	}
	nclear = 0;
	draw_stats();
}

static void game_over_sound(void)
{
	note(392, 3); note(330, 3); note(262, 3); note(196, 6);
}

/* wait for ENTER (returns 1) or ESC (quits) */
static int wait_enter(void)
{
	for (;;) {
		sound_update();
		u16 k = getkey();
		if (!k) { vsync(); continue; }
		if (SCAN(k) == K_ESC) dos_exit();
		if (SCAN(k) == K_ENTER) return 1;
	}
}

static void play(void)
{
	for (int r = 0; r < BH; r++)
		for (int c = 0; c < BW; c++) board[r][c] = 0;
	score = lines = 0;
	level = 1;
	bagn = 0;
	nclear = 0;
	nxt = next_from_bag();
	draw_stats();
	spawn();
	invalidate_board();
	draw_board();
	u32 tfall = ticks(), tclear = 0;
	int paused = 0;

	for (;;) {
		int dirty = 0;
		sound_update();
		u16 k;
		while ((k = getkey()) != 0) {
			u8 s = SCAN(k);
			if (s == K_ESC) dos_exit();
			if (s == 0x19) { /* P */
				paused = !paused;
				if (paused) {
					spk_off();
					msgbox("PAUSA", "P para seguir", "ESC para salir", "");
				} else {
					close_box();
					tfall = ticks();
				}
				continue;
			}
			if (paused || !playing) continue;
			if (s == K_LEFT && fits(cur, crot, cx - 1, cy)) { cx--; dirty |= 1; }
			else if (s == K_RIGHT && fits(cur, crot, cx + 1, cy)) { cx++; dirty |= 1; }
			else if (s == K_UP || s == 0x2D /* X */) { if (try_rotate(1)) { dirty |= 1; note(880, 1); } }
			else if (s == 0x2C /* Z */) { if (try_rotate(3)) { dirty |= 1; note(880, 1); } }
			else if (s == K_DOWN) {
				if (fits(cur, crot, cx, cy + 1)) { cy++; score++; tfall = ticks(); dirty = 3; }
				else tfall = 0; /* lock on the next check */
			} else if (s == K_SPACE) {
				int g = ghost_y();
				score += 2 * (g - cy);
				cy = g;
				dirty = 3;
				tfall = 0;
				note(300, 1);
			}
		}
		if (paused) { vsync(); continue; }

		if (nclear) {
			/* flashing rows, then remove them */
			u32 e = since(tclear);
			int f = (e / 2) & 1 ? 0 : 1;
			if (f != flash_on) { flash_on = f; dirty |= 1; }
			if (e >= 7) {
				remove_lines();
				flash_on = 0;
				if (!spawn()) goto over;
				tfall = ticks();
				dirty |= 1;
			}
		} else if (playing && (tfall == 0 || since(tfall) >= fall_delay())) {
			tfall = ticks();
			if (fits(cur, crot, cx, cy + 1)) {
				cy++;
				dirty |= 1;
			} else {
				if (lock_piece()) goto over;
				if (nclear) {
					tclear = ticks();
					flash_on = 1;
					if (nclear == 4) { note(523, 1); note(659, 1); note(784, 1); note(1047, 1); note(1319, 3); }
					else for (int i = 0; i < nclear; i++) { note(660 + i * 120, 1); note(0, 1); }
				} else if (!spawn()) goto over;
				dirty |= 1;
			}
		}
		if (dirty) {
			vsync();
			draw_board();
			if (dirty & 2) draw_stats();
		} else vsync();
	}
over:
	playing = 1;
	draw_board();
	draw_stats();
	game_over_sound();
	wait_ticks(4);
	flush_keys();
	msgbox("FIN", "ENTER para jugar de nuevo,", "ESC para salir", "");
	wait_enter();
	close_box();
}

int main(void)
{
	dos_init();
	make_shapes();
	setup_palette();
	draw_background();
	invalidate_board();
	draw_board();
	nxt = next_from_bag();
	draw_next();
	level = 1;
	draw_stats();
	msgbox("BLOQUES", "ENTER para jugar", "ESC para salir", "");
	wait_enter();
	close_box();
	for (;;) play();
	return 0;
}
