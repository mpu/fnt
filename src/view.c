#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <dirent.h>
#include <sys/types.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

void panic(char *);
void fload(void);
void sdraw(void);
void xinit(void);
void xexpose(XEvent *);
void xconfigure(XEvent *);
void xbutton(XEvent *);
void xkey(XEvent *);
void tread(void);
void sighup(int);

void (*xhandler[LASTEvent])(XEvent *) = {
	[Expose] = xexpose,
	[ConfigureNotify] = xconfigure,
	[ButtonPress] = xbutton,
	[KeyPress] = xkey,
};
struct {
	Display *dsp;
	Window win;
	GC gc;
	int scr;
	int w;
	int h;
} xc;
struct {
	unsigned *r;
	int nr;
} txt;
struct {
	char *path;
	int w;
	int h;
	char **g;
	int ng;
} fnt;

void
panic(char *s)
{
	fprintf(stderr, "panic: %s\n", s);
	exit(1);
}

void
fload()
{
	DIR *fd;
	FILE *f;
	int x, y;
	char *p, path[512];
	struct dirent *de;
	unsigned r;

	p = strrchr(fnt.path, '/');
	if (!p)
		p = fnt.path;
	else
		p++;
	if (sscanf(p, "%d x %d", &fnt.w, &fnt.h) != 2)
		panic("invalid font path");
	fd = opendir(fnt.path);
	if (!fd)
		panic("cannot open font path");
	while ((de = readdir(fd))) {
		if (sscanf(de->d_name, "U+%x", &r) != 1)
			continue;
		snprintf(path, sizeof path, "%s/%s", fnt.path, de->d_name);
		f = fopen(path, "r");
		if (!f)
			continue;
		if (r >= fnt.ng) {
			fnt.g = realloc(fnt.g, (r+1) * sizeof (char *));
			if (!fnt.g)
				panic("out of memory");
			while (fnt.ng <= r)
				fnt.g[fnt.ng++] = 0;
		}
		fnt.g[r] = malloc(fnt.w * fnt.h);
		if (!fnt.g[r])
			panic("out of memory");
		for (y=0; y<fnt.h; y++) {
			for (x=0; x<fnt.w; x++) {
				switch (fgetc(f)) {
				case '.':
					fnt.g[r][y*fnt.w + x] = 0;
					break;
				case 'x':
					fnt.g[r][y*fnt.w + x] = 1;
					break;
				default:
					goto invalid;
				}
			}
			if (fgetc(f) != '\n') {
			invalid:
				fprintf(stderr, "warning: invalid glyph file %s\n", de->d_name);
				free(fnt.g[r]);
				fnt.g[r] = 0;
				break;
			}
		}
		fclose(f);
	}
	closedir(fd);
}

void
sdraw()
{
	char *g;
	unsigned *p;
	int cx, cy, x, y;

	XClearWindow(xc.dsp, xc.win);
	cx = 10;
	cy = 10;
	for (p=txt.r; p < &txt.r[txt.nr]; p++) {
		if (*p == '\n') {
			cx = 10;
			cy += fnt.h;
			continue;
		}
		if (*p < fnt.ng)
			g = fnt.g[*p];
		else
			g = 0;
		if (g)
			for (x=0; x<fnt.w; x++)
				for (y=0; y<fnt.h; y++)
					if (g[y*fnt.w + x])
						XDrawPoint(xc.dsp, xc.win, xc.gc, cx+x, cy+y);
		cx += fnt.w;
	}
	XFlush(xc.dsp);
}

void
xexpose(XEvent *ev)
{
	sdraw();
}

void
xbutton(XEvent *ev)
{
	/* maybe do something... */
}

void
xkey(XEvent *ev)
{
	char str[8];
	int n;

	if (ev->type != KeyPress)
		return;
	n = XLookupString(&ev->xkey, str, sizeof str, 0, 0);
	if (n) {
		switch (str[0]) {
		case 'r':
			fload();
			sdraw();
			break;
		case 'q':
		case 'Q':
			XFreeGC(xc.dsp, xc.gc);
			XDestroyWindow(xc.dsp, xc.win);
			XCloseDisplay(xc.dsp);
			exit(0);
		}
	}
}

void
xconfigure(XEvent *ev)
{
	xc.w = ev->xconfigure.width;
	xc.h = ev->xconfigure.height;
}

void
xinit(void)
{
	XGCValues gcv;

	xc.dsp = XOpenDisplay("");
	if (!xc.dsp)
		panic("cannot open display");
	xc.scr = DefaultScreen(xc.dsp);
	xc.w = 100;
	xc.h = 100;
	xc.win = XCreateSimpleWindow(
		xc.dsp, RootWindow(xc.dsp, xc.scr), 0, 0, xc.w, xc.h, 0,
		BlackPixel(xc.dsp, xc.scr), WhitePixel(xc.dsp, xc.scr)
	);
	XSelectInput(xc.dsp, xc.win, StructureNotifyMask|ExposureMask|ButtonPressMask|KeyPressMask);
	xc.gc = XCreateGC(xc.dsp, (Drawable)xc.win, 0, &gcv);
	XMapWindow(xc.dsp, xc.win);
}

void
tread()
{
	unsigned r;
	int c, rd, n;

	txt.nr = 1000;
	txt.r = malloc(txt.nr * sizeof (unsigned));
	if (!txt.r)
		panic("out of memory");
	n = 0;
	for (;;) {
		c = getchar();
		if (c == EOF)
			break;
		if (c < 0x80) {
			rd = 0;
			r = c;
		}
		else if (c < 0xc0) {
			rd = 0;
			r = 0;
		}
		else if (c < 0xe0) {
			rd = 1;
			r = c & 0x1f;
		}
		else if (c < 0xf0) {
			rd = 2;
			r = c & 0xf;
		}
		else if (c < 0xf8) {
			rd = 3;
			r = c & 0x7;
		}
		else {
			rd = 0;
			r = 0;
		}
		while (rd--) {
			c = getchar();
			r <<= 6;
			r |= c & 0x3f;
		}
		if (n == txt.nr) {
			txt.nr += 1000;
			txt.r = realloc(txt.r, txt.nr * sizeof (unsigned));
			if (!txt.r)
				panic("out of memory");
		}
		txt.r[n++] = r;
	}
	txt.nr = n;
}

void
sighup(int sig)
{
	XEvent ev;

	fload();
	memset(&ev, 0, sizeof ev);
	ev.type = Expose;
	ev.xexpose.window = xc.win;
	XSendEvent(xc.dsp, xc.win, False, ExposureMask, &ev);
	XFlush(xc.dsp);
}

int
main(int ac, char *av[])
{
	XEvent ev;

	if (ac < 2) {
		fprintf(stderr, "usage: view FONTDIR\n");
		exit(1);
	}
	fnt.path = av[1];
	signal(SIGHUP, sighup);
	fload();
	tread();
	xinit();
	for (;;) {
		XNextEvent(xc.dsp, &ev);
		if (xhandler[ev.type])
			xhandler[ev.type](&ev);
	}
}
