/*% cc -Wall -lncurses % -o #
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curses.h>
#include <locale.h>

struct {
	int x;
	int y;
	int cux;
	int cuy;
	int cx;
	int cy;
	WINDOW *mw;
} scr;

struct {
	int w;
	int h;
	char **bits;
	char path[128];
} gly;


void panic(char *);
void gshift(int, int);
void gmirror(void);
void sdraw(void);
void treset(void);
void tinit(void);
void fparse(FILE *);
void fdump(FILE *);

void
panic(char *s)
{
	treset();
	fprintf(stderr, "panic: %s\n", s);
	exit(1);
}

void
gshift(int dx, int dy)
{
	char **b;
	int y;
	char *row, **col;

	row = calloc(gly.w, 1);
	col = calloc(gly.h, sizeof (char *));
	dx = (dx + gly.w) % gly.w;
	dy = (dy + gly.h) % gly.h;
	if (row && col) {
		b = gly.bits;
		memcpy(col, &b[dy], (gly.h - dy) * sizeof (char *));
		memcpy(&col[gly.h - dy], b, dy * sizeof (char *));
		memcpy(b, col, gly.h * sizeof (char *));
		for (y=0; y<gly.h; y++) {
			memcpy(row, &b[y][dx], (gly.w - dx));
			memcpy(&row[gly.w - dx], b[y], dx);
			memcpy(b[y], row, gly.w);
		}
	}
	free(row);
	free(col);
}

void
gmirror()
{
	char **b;
	int x2, x, y, t;

	x2 = gly.w / 2;
	b = gly.bits;
	for (x=0; x<=x2; x++)
		for (y=0; y<gly.h; y++) {
			t = b[y][x];
			b[y][x] = b[y][gly.w-1-x];
			b[y][gly.w-1-x] = t;
		}
}

void
sdraw()
{
	char *s, buf[4];
	int x, y;

	wclear(scr.mw);
	for (y=0; y<gly.h; y++)
		for (x=0; x<gly.w; x++) {
			s = "  ";
			if (x == scr.cux && y == scr.cuy)
				s = "--";
			if (gly.bits[y][x])
				wattron(scr.mw, COLOR_PAIR(1));
			else
				wattroff(scr.mw, COLOR_PAIR(1));
			mvwaddstr(scr.mw, y, 3 + x*2, s);
			sprintf(buf, "%02d", y+1);
			mvwaddstr(scr.mw, y, 0, buf);
		}
}

void
tinit()
{
	setlocale(LC_ALL, "");
	/* signal(SIGWINCH, sigwinch); */
	initscr();
	raw();
	noecho();
	getmaxyx(stdscr, scr.y, scr.x);
	scr.cx = (scr.x - 2*gly.w - 3) / 2;
	scr.cy = (scr.y - gly.h) / 2;
	if (scr.cx < 0 || scr.cy < 0)
		panic("window too small");
	if (!(scr.mw = newwin(gly.h, gly.w*2+3, scr.cy, scr.cx)))
		panic("cannot create curses window");
	keypad(scr.mw, 1);
	curs_set(0);
	if (has_colors() != TRUE)
		panic("colors not supported");
	start_color();
	init_pair(1, COLOR_WHITE, COLOR_BLUE);
	init_pair(2, COLOR_BLACK, COLOR_WHITE);
	wbkgd(scr.mw, COLOR_PAIR(2));
}

void
treset()
{
	if (scr.mw)
		delwin(scr.mw);
	endwin();
}

void
fparse(FILE *f)
{
	int x, y;

	for (y=0; y<gly.h; y++) {
		for (x=0; x<gly.w; x++) {
			switch (fgetc(f)) {
			case '.':
				gly.bits[y][x] = 0;
				break;
			case 'x':
				gly.bits[y][x] = 1;
				break;
			default:
				panic("invalid input file");
				break;
			}
		}
		if (fgetc(f) != '\n')
			panic("invalid input file");
	}
}

void
fdump(FILE *f)
{
	int x, y;

	for (y=0; y<gly.h; y++) {
		for (x=0; x<gly.w; x++) {
			if (gly.bits[y][x])
				fputc('x', f);
			else
				fputc('.', f);
		}
		fputc('\n', f);
	}
}

int
main(int ac, char *av[])
{
	char *bits, *pb;
	FILE *f;
	int y, ch;

	if (ac < 3) {
	usage:
		fprintf(stderr, "usage: edit WxH U+NNNN\n");
		exit(1);
	}

	snprintf(gly.path, sizeof gly.path, "%s/%s", av[1], av[2]);
	if (sscanf(av[1], "%d x %d", &gly.w, &gly.h) != 2)
		goto usage;
	if (!(bits = calloc(gly.w * gly.h, 1))
	||  !(gly.bits = calloc(gly.h, sizeof (char *))))
		panic("out of memory");
	for (y=0; y<gly.h; y++) {
		gly.bits[y] = bits;
		bits += gly.w;
	}
	if ((f = fopen(gly.path, "r"))) {
		fparse(f);
		fclose(f);
	}

	tinit();
	for (;;) {
		sdraw();
		ch = wgetch(scr.mw);
		switch (ch) {
		case 'q':
		case 'w':
			if (!(f = fopen(gly.path, "w")))
				panic("cannot open output file");
			fdump(f);
			fclose(f);
		case 'x':
			if (ch != 'w') {
				treset();
				exit(0);
			}
			break;
		case 'h':
			scr.cux += gly.w - 1;
			break;
		case 'j':
			scr.cuy += 1;
			break;
		case 'k':
			scr.cuy += gly.h - 1;
			break;
		case 'l':
			scr.cux += 1;
			break;
		case ' ':
			pb = &gly.bits[scr.cuy][scr.cux];
			*pb = !*pb;
			break;
		case '>':
			gshift(-1, 0);
			break;
		case '<':
			gshift(+1, 0);
			break;
		case '+':
			gshift(0, +1);
			break;
		case '-':
			gshift(0, -1);
			break;
		case '|':
			gmirror();
			break;
		}
		scr.cux %= gly.w;
		scr.cuy %= gly.h;
	}
}
