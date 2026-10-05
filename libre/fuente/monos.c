/*
 * MONOS.COM - two monkeys throw bananas across a night city (DOS, VGA 13h, 386+).
 * Original code and art for Órbita. MIT License, Copyright (c) 2026 Órbita.
 *
 * Each turn the player types an angle (0-90) and a speed (1-200), Enter after
 * each. Gravity and wind bend the throw, explosions dig holes in the
 * buildings, and the first monkey to score 3 hits wins. ESC returns to DOS.
 */
#include "dosrt.h"

/* palette */
#define C_BLD 16   /* 16..19 building bodies */
#define C_BLDL 20  /* 20..23 roof lines */
#define C_WIN 24
#define C_WIN2 25
#define C_WOFF 26
#define C_FUR 40
#define C_FURD 41
#define C_FACE 42
#define C_BAN 44
#define C_BAND 45
#define C_EXP 48   /* 48..51 */
#define C_BOX 52
#define C_BOXE 53
#define C_SKY 200  /* 200..239, one shade per 5 rows */
#define C_MOON 240
#define C_MOON2 241
#define C_STAR 242
#define SOLID(c) ((c) >= 16 && (c) < 200)

static const short sintab[91] = {
	0, 18, 36, 54, 71, 89, 107, 125, 143, 160, 178, 195, 213, 230, 248, 265, 282, 299, 316,
	333, 350, 367, 384, 400, 416, 433, 449, 465, 481, 496, 512, 527, 543, 558, 573, 587, 602,
	616, 630, 644, 658, 672, 685, 698, 711, 724, 737, 749, 761, 773, 784, 796, 807, 818, 828,
	839, 849, 859, 868, 878, 887, 896, 904, 912, 920, 928, 935, 943, 949, 956, 962, 968, 974,
	979, 984, 989, 994, 998, 1002, 1005, 1008, 1011, 1014, 1016, 1018, 1020, 1022, 1023, 1023,
	1024, 1024};

/* monkey, 16x16, facing right. B fur, D dark fur, F face, W eye white, E pupil */
static const char *const monkey[16] = {
	"......DBBD......",
	"....BBBBBBBB....",
	"...BBBBBBBBBB...",
	"..FFBFFFFFFBFF..",
	"..FFBWEFFWEBFF..",
	"...BBFFFFFFBB...",
	"....BFFDDFFB....",
	"....BFDFFDFB....",
	".....BFDDFB.....",
	"......BBBB......",
	"....BBBBBBBB....",
	"....BBFFFFBB....",
	"....BBFFFFBB....",
	".....BBBBBB.....",
	".....BB..BB.....",
	"....DDD..DDD....",
};
/* arms as (col,row) lists, last point is the hand; mirrored for the other arm */
static const signed char arm_down[][2] = {{3, 10}, {3, 11}, {3, 12}, {3, 13}};
static const signed char arm_up[][2] = {{3, 10}, {3, 9}, {2, 8}, {2, 7}, {2, 6}};
static const signed char tail[][2] = {{4, 14}, {3, 15}, {2, 15}, {1, 14}, {1, 13}, {1, 12}, {2, 11}};

static const char *const banana[7] = {
	".....D.",
	"....Y..",
	"...YY..",
	"...YY..",
	"...YY..",
	"....Y..",
	".....D.",
};

/* city */
static int nb, bx[24], bw[24], btop[24];
/* monkeys */
static int mx[2], my[2], alive[2], pose[2];
static int score[2], wind, vs_cpu, cpu_err;
/* HUD prompt lines */
static char hl1[2][24], hl2[2][24];
static char center_msg[40];

static int skyc(int y) { return C_SKY + (y < 0 ? 0 : y / 5); }

static void setup_palette(void)
{
	static const u8 bc[4][3] = {{18, 26, 30}, {34, 16, 16}, {20, 20, 36}, {32, 28, 20}};
	for (int i = 0; i < 4; i++) {
		pal(C_BLD + i, bc[i][0], bc[i][1], bc[i][2]);
		pal(C_BLDL + i, bc[i][0] + 12, bc[i][1] + 12, bc[i][2] + 12);
	}
	pal(C_WIN, 63, 56, 26);
	pal(C_WIN2, 46, 38, 14);
	pal(C_WOFF, 8, 8, 12);
	pal(C_FUR, 34, 20, 8);
	pal(C_FURD, 20, 11, 4);
	pal(C_FACE, 56, 42, 28);
	pal(C_BAN, 63, 58, 10);
	pal(C_BAND, 36, 28, 4);
	pal(C_EXP, 63, 20, 0);
	pal(C_EXP + 1, 63, 40, 0);
	pal(C_EXP + 2, 63, 60, 20);
	pal(C_EXP + 3, 63, 63, 56);
	pal(C_BOX, 6, 6, 16);
	pal(C_BOXE, 40, 44, 63);
	for (int i = 0; i < 40; i++) /* deep blue night to violet dusk */
		pal(C_SKY + i, 2 + i * 26 / 40, 2 + i * 6 / 40, 12 + i * 18 / 40);
	pal(C_MOON, 60, 60, 52);
	pal(C_MOON2, 46, 46, 40);
	pal(C_STAR, 50, 50, 60);
}

/* filled circle on the current target; sky = 1 paints the sky color per row */
static void disc(int x, int y, int r, u8 c, int sky)
{
	for (int dy = -r; dy <= r; dy++) {
		int dx = 0;
		while ((dx + 1) * (dx + 1) + dy * dy <= r * r) dx++;
		rect(x - dx, y + dy, 2 * dx + 1, 1, sky ? skyc(y + dy) : c);
	}
}

static void gen_city(void)
{
	int x = 0;
	nb = 0;
	while (x < 320 && nb < 24) {
		int w = 24 + rnd(18);
		if (320 - (x + w) < 24) w = 320 - x;
		bx[nb] = x;
		bw[nb] = w;
		btop[nb] = 200 - (40 + rnd(100));
		nb++;
		x += w + 1;
	}
	set_gs(bg_seg);
	for (int y = 0; y < 200; y++) rect(0, y, 320, 1, skyc(y));
	for (int i = 0; i < 70; i++) pset(rnd(320), rnd(130), C_STAR);
	int moonx = 110 + rnd(100);
	disc(moonx, 56, 9, C_MOON, 0);
	disc(moonx + 4, 53, 8, skyc(53), 0); /* crescent */
	pset(moonx - 4, 58, C_MOON2);
	for (int i = 0; i < nb; i++) {
		int k = rnd(4), t = btop[i];
		rect(bx[i], t, bw[i], 200 - t, C_BLD + k);
		rect(bx[i], t, bw[i], 1, C_BLDL + k);
		for (int wy = t + 5; wy + 5 < 200; wy += 9)
			for (int wx = bx[i] + 3; wx + 6 <= bx[i] + bw[i]; wx += 6) {
				u32 r = rnd(10);
				rect(wx, wy, 3, 4, r < 5 ? C_WIN : r < 7 ? C_WIN2 : C_WOFF);
			}
	}
	set_gs(VGA_SEG);
	/* monkeys on the 2nd/3rd building from each side */
	int a = 1 + rnd(2), b = nb - 2 - rnd(2);
	mx[0] = bx[a] + bw[a] / 2 - 8;
	my[0] = btop[a] - 16;
	mx[1] = bx[b] + bw[b] / 2 - 8;
	my[1] = btop[b] - 16;
	alive[0] = alive[1] = 1;
	pose[0] = pose[1] = 0;
	wind = (int)rnd(11) - 5;
	if (rnd(3) == 0) wind += wind < 0 ? -(int)rnd(6) : (int)rnd(6);
}

/* pose bits: 1 = front arm up, 2 = back arm up */
static void draw_monkey(int i)
{
	if (!alive[i]) return;
	int flip = i == 1, x0 = mx[i], y0 = my[i];
#define MP(c, r, col) pset(x0 + (flip ? 15 - (c) : (c)), y0 + (r), col)
	for (int r = 0; r < 16; r++)
		for (int c = 0; c < 16; c++) {
			u8 col;
			switch (monkey[r][c]) {
			case 'B': col = C_FUR; break;
			case 'D': col = C_FURD; break;
			case 'F': col = C_FACE; break;
			case 'W': col = 15; break;
			case 'E': col = 0; break;
			default: continue;
			}
			MP(c, r, col);
		}
	for (int k = 0; k < 7; k++) MP(tail[k][0], tail[k][1], C_FURD);
	/* back arm (left side when facing right) and front arm (mirrored) */
	for (int side = 0; side < 2; side++) {
		int up = side ? (pose[i] & 1) : (pose[i] & 2);
		int n = up ? 5 : 4;
		const signed char(*a)[2] = up ? arm_up : arm_down;
		for (int k = 0; k < n; k++) {
			int c = side ? 15 - a[k][0] : a[k][0];
			MP(c, a[k][1], k == n - 1 ? C_FACE : C_FUR);
		}
	}
#undef MP
}
static void redraw_monkey(int i)
{
	restore(mx[i] - 1, my[i], 18, 16);
	draw_monkey(i);
}

static const char *pname(int i) { return i == 0 ? "MONO 1" : vs_cpu ? "CPU" : "MONO 2"; }

static void draw_hud(void)
{
	char b[24];
	band_begin(0, 0, 320, 36);
	textsh(4, 2, pname(0), 11, 1);
	textsh(316 - slen(pname(1)) * 8, 2, pname(1), 12, 1);
	b[0] = '0' + score[0];
	memcpy(b + 1, " PUNTOS ", 8);
	b[9] = '0' + score[1];
	b[10] = 0;
	ctext(2, b, 15, 1);
	ctext(14, "VIENTO", 7, 1);
	if (wind) {
		int L = wind * 7 / 2, T = 160 + L, d = wind > 0 ? 1 : -1;
		rect(L > 0 ? 160 : T, 26, L > 0 ? L : -L, 2, 14);
		for (int i = 0; i < 5; i++) rect(T - i * d, 27 - i, 1, 2 * i, 14);
	} else rect(158, 25, 4, 4, 14);
	for (int i = 0; i < 2; i++) {
		if (hl1[i][0]) textsh(i ? 316 - slen(hl1[i]) * 8 : 4, 14, hl1[i], 15, 1);
		if (hl2[i][0]) textsh(i ? 316 - slen(hl2[i]) * 8 : 4, 24, hl2[i], 15, 1);
	}
	band_end(0, 0, 320, 36);
	if (center_msg[0]) ctext(80, center_msg, 14, 1);
}

static void scene(void)
{
	vsync();
	copy_all();
	draw_monkey(0);
	draw_monkey(1);
	draw_hud();
}

/* idle wait that keeps the sound going; ESC quits */
static void pause_ticks(u32 n)
{
	u32 t0 = ticks();
	while (since(t0) < n) {
		sound_update();
		u16 k = getkey();
		if (k && SCAN(k) == K_ESC) dos_exit();
		vsync();
	}
}

static void sset(char *d, const char *s)
{
	while ((*d++ = *s++)) ;
}

/* read a number on player p's HUD line */
static int input_number(int p, char *line, const char *label, int maxv)
{
	char num[4];
	int n = 0, blink = 1;
	u32 tb = ticks();
	for (;;) {
		int l = slen(label);
		memcpy(line, label, l);
		memcpy(line + l, num, n);
		line[l + n] = blink ? '_' : ' ';
		line[l + n + 1] = 0;
		vsync();
		draw_hud();
		for (;;) {
			sound_update();
			if (since(tb) >= 5) { tb = ticks(); blink = !blink; break; }
			u16 k = getkey();
			if (!k) { vsync(); continue; }
			u8 a = ASC(k), s = SCAN(k);
			if (s == K_ESC) dos_exit();
			if (a >= '0' && a <= '9' && n < 3) { num[n++] = a; note(1400, 1); }
			else if ((s == K_BKSP || a == 8) && n) n--;
			else if ((s == K_ENTER || a == 13) && n) {
				int v = 0;
				for (int i = 0; i < n; i++) v = v * 10 + num[i] - '0';
				if (v > maxv) v = maxv;
				line[0] = 0;
				memcpy(line, label, l);
				char vb[8];
				utoa_(v, vb);
				sset(line + l, vb);
				draw_hud();
				return v;
			} else continue;
			blink = 1;
			tb = ticks();
			break;
		}
	}
}

/* ---------- banana physics (fixed point, 1/4096 px; 8 substeps per BIOS tick) ---------- */
#define SUB 8
#define GRAV 19
typedef struct { long x, y, vx, vy; int left_self, p; } Ban;
enum { B_FLY, B_OUT, B_BUILDING, B_HIT, B_SELF };

static void ban_init(Ban *b, int p, int ang, int spd)
{
	int d = p == 0 ? 1 : -1;
	long c = sintab[90 - ang], s = sintab[ang];
	b->p = p;
	b->x = (long)(mx[p] + 8 + d * 5) << 12;
	b->y = (long)(my[p] + 2) << 12;
	b->vx = d * (spd * 5626L * c / 102400L);
	b->vy = -(spd * 5626L * s / 102400L);
	b->left_self = 0;
}
static int in_monkey(int i, int x, int y)
{
	return alive[i] && x >= mx[i] && x < mx[i] + 16 && y >= my[i] + 1 && y < my[i] + 16;
}
static int ban_step(Ban *b)
{
	b->vx += wind;
	b->vy += GRAV;
	b->x += b->vx;
	b->y += b->vy;
	int x = b->x >> 12, y = b->y >> 12;
	if (x < -40 || x > 360 || y >= 200) return B_OUT;
	if (y < 0 || x < 0 || x >= 320) return B_FLY;
	int o = 1 - b->p;
	if (in_monkey(o, x, y)) return B_HIT;
	if (in_monkey(b->p, x, y)) {
		if (b->left_self) return B_SELF;
	} else b->left_self = 1;
	if (SOLID(BGM[y * 320 + x])) return B_BUILDING;
	return B_FLY;
}

static void draw_banana(int x, int y, int f)
{
	for (int r = 0; r < 7; r++)
		for (int c = 0; c < 7; c++) {
			int sr = r, sc = c; /* rotate the sprite by f quarter turns */
			for (int k = 0; k < f; k++) { int t = sr; sr = 6 - sc; sc = t; }
			char ch = banana[sr][sc];
			if (ch != '.') pset(x - 3 + c, y - 3 + r, ch == 'Y' ? C_BAN : C_BAND);
		}
}

static void explode(int x, int y, int R)
{
	note(1, R > 12 ? 10 : 4);
	if (R > 12) { note(220, 2); note(160, 2); note(110, 4); }
	for (int r = 2; r <= R; r += 2) {
		u32 t0 = ticks();
		for (int k = r; k > 0; k -= 3) disc(x, y, k, C_EXP + ((r - k) / 3 + r / 2) % 4, 0);
		while (since(t0) < 1) sound_update();
	}
	set_gs(bg_seg);
	disc(x, y, R, 0, 1);
	set_gs(VGA_SEG);
	pause_ticks(2);
	vsync();
	restore(x - R - 1, y - R - 1, 2 * R + 3, 2 * R + 3);
	draw_monkey(0);
	draw_monkey(1);
	if (y - R < 36) draw_hud();
}

/* returns the B_* result */
static int throw_banana(int p, int ang, int spd)
{
	Ban b;
	ban_init(&b, p, ang, spd);
	pose[p] = 1;
	redraw_monkey(p);
	note(500, 1);
	note(800, 1);
	int res = B_FLY, ox = -100, oy = -100, frame_n = 0;
	u32 t0 = ticks(), done = 0;
	while (res == B_FLY) {
		sound_update();
		u16 k = getkey();
		if (k && SCAN(k) == K_ESC) dos_exit();
		u32 e = since(t0);
		if (e <= done) { vsync(); continue; }
		if (e - done > 4) done = e - 4; /* very slow machine: don't run away */
		while (done < e && res == B_FLY) {
			for (int i = 0; i < SUB && res == B_FLY; i++) res = ban_step(&b);
			done++;
		}
		if (done >= 5 && pose[p]) { pose[p] = 0; redraw_monkey(p); }
		int x = b.x >> 12, y = b.y >> 12;
		vsync();
		if (ox > -100) {
			restore(ox - 3, oy - 3, 7, 7);
			for (int i = 0; i < 2; i++)
				if (ox + 4 >= mx[i] - 1 && ox - 4 <= mx[i] + 16 && oy + 4 >= my[i] && oy - 4 <= my[i] + 16) draw_monkey(i);
			if (oy - 3 < 36) draw_hud();
		}
		ox = -100;
		if (res == B_FLY && y >= 3 && x >= 0 && x < 320) {
			draw_banana(x, y, frame_n++ & 3);
			ox = x;
			oy = y;
		}
	}
	if (pose[p]) { pose[p] = 0; redraw_monkey(p); }
	int x = b.x >> 12, y = b.y >> 12;
	if (res == B_BUILDING) explode(x, y, 8);
	else if (res == B_HIT || res == B_SELF) {
		int who = res == B_HIT ? 1 - p : p;
		int cx = mx[who] + 8, cy = my[who] + 8;
		alive[who] = 0;
		explode(cx, cy, 20);
	}
	return res;
}

/* ---------- CPU opponent: aims by simulating throws, then adds some error ---------- */
static int simulate(int p, int ang, int spd, int *ex)
{
	Ban b;
	ban_init(&b, p, ang, spd);
	int r = B_FLY;
	for (int i = 0; i < 4000 && r == B_FLY; i++) r = ban_step(&b);
	*ex = b.x >> 12;
	return r;
}
static void cpu_aim(int p, int *ang, int *spd)
{
	static const u8 angs[] = {45, 55, 35, 62, 50, 40, 68, 30, 75};
	int d = p == 0 ? 1 : -1, tx = mx[1 - p] + 8, best = 1 << 30;
	*ang = 45;
	*spd = 60;
	for (unsigned i = 0; i < sizeof angs; i++) {
		int lo = 10, hi = 200;
		while (lo <= hi) {
			int mid = (lo + hi) / 2, ex;
			int r = simulate(p, angs[i], mid, &ex);
			int dist = r == B_SELF ? -1000 : d * (ex - tx);
			if (r == B_HIT) { *ang = angs[i]; *spd = mid; goto found; }
			if ((dist < 0 ? -dist : dist) < best) {
				best = dist < 0 ? -dist : dist;
				*ang = angs[i];
				*spd = mid;
			}
			if (dist < 0) lo = mid + 1;
			else hi = mid - 1;
		}
	}
found:
	if (cpu_err) *spd += (int)rnd(2 * cpu_err + 1) - cpu_err;
	if (*spd < 1) *spd = 1;
	if (*spd > 200) *spd = 200;
	cpu_err = cpu_err > 5 ? cpu_err - 3 : 2;
}
/* show the CPU "typing" its numbers */
static void cpu_type(int p, char *line, const char *label, int v)
{
	char vb[8];
	int l = slen(label);
	utoa_(v, vb);
	memcpy(line, label, l + 1);
	draw_hud();
	pause_ticks(6);
	for (int i = 0; vb[i]; i++) {
		line[l + i] = vb[i];
		line[l + i + 1] = 0;
		note(1400, 1);
		draw_hud();
		pause_ticks(4);
	}
	(void)p;
	pause_ticks(4);
}

/* ---------- screens ---------- */
static void box(int y, int h)
{
	rect(38, y - 2, 244, h + 4, 0);
	rect(40, y, 240, h, C_BOX);
	frame(40, y, 240, h, C_BOXE);
}

static void dance(int w)
{
	static const u16 tune[] = {523, 659, 784, 659, 784, 1047};
	for (int i = 0; i < 6; i++) note(tune[i], 3);
	for (int i = 0; i < 12; i++) {
		pose[w] = (i & 1) ? 1 : 2;
		vsync();
		redraw_monkey(w);
		pause_ticks(3);
	}
	pose[w] = 3;
	redraw_monkey(w);
}

static int menu(void)
{
	gen_city();
	score[0] = score[1] = 0;
	hl1[0][0] = hl1[1][0] = hl2[0][0] = hl2[1][0] = 0;
	center_msg[0] = 0;
	vsync();
	copy_all();
	pose[0] = pose[1] = 3;
	draw_monkey(0);
	draw_monkey(1);
	box(40, 112);
	ctext(50, "MONOS", 14, 3);
	ctext(78, "una guerra de bananas", 7, 1);
	ctext(98, "1  DOS JUGADORES", 15, 1);
	ctext(112, "2  CONTRA LA CPU", 15, 1);
	ctext(132, "ESC  SALIR", 7, 1);
	flush_keys();
	for (;;) {
		sound_update();
		u16 k = getkey();
		if (!k) { vsync(); continue; }
		if (SCAN(k) == K_ESC) dos_exit();
		if (ASC(k) == '1') return 0;
		if (ASC(k) == '2') return 1;
	}
}

static void match(void)
{
	int starter = 0;
	score[0] = score[1] = 0;
	for (;;) {
		gen_city();
		cpu_err = 12;
		center_msg[0] = 0;
		scene();
		int p = starter, winner = -1;
		starter ^= 1;
		while (winner < 0) {
			int ang, spd;
			hl1[0][0] = hl1[1][0] = hl2[0][0] = hl2[1][0] = 0;
			draw_hud();
			if (vs_cpu && p == 1) {
				cpu_aim(p, &ang, &spd);
				cpu_type(p, hl1[p], "ANGULO: ", ang);
				cpu_type(p, hl2[p], "VELOCIDAD: ", spd);
			} else {
				flush_keys();
				ang = input_number(p, hl1[p], "ANGULO: ", 90);
				spd = input_number(p, hl2[p], "VELOCIDAD: ", 200);
				if (spd < 1) spd = 1;
			}
			int r = throw_banana(p, ang, spd);
			if (r == B_HIT) winner = p;
			else if (r == B_SELF) winner = 1 - p;
			else { pause_ticks(4); p = 1 - p; }
		}
		score[winner]++;
		sset(center_msg, "PUNTO PARA ");
		sset(center_msg + 11, pname(winner));
		hl1[0][0] = hl1[1][0] = hl2[0][0] = hl2[1][0] = 0;
		draw_hud();
		dance(winner);
		if (score[winner] >= 3) {
			char m[24];
			sset(m, "GANA ");
			sset(m + 5, pname(winner));
			pause_ticks(6);
			box(56, 84);
			ctext(64, m, 14, 2);
			ctext(86, "FIN", 15, 1);
			ctext(102, "ENTER para jugar de nuevo,", 15, 1);
			ctext(118, "ESC para salir", 7, 1);
			flush_keys();
			for (;;) {
				sound_update();
				u16 k = getkey();
				if (!k) { vsync(); continue; }
				if (SCAN(k) == K_ESC) dos_exit();
				if (SCAN(k) == K_ENTER) break;
			}
			score[0] = score[1] = 0;
			starter = 0;
		} else pause_ticks(10);
	}
}

int main(void)
{
	dos_init();
	setup_palette();
	vs_cpu = menu();
	match();
	return 0;
}
