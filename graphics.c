#include <SDL2/SDL.h>
#include "mdp.h"
#include "font.h"

SDL_Window *window;
SDL_Renderer *renderer;
SDL_Texture *texture;
SDL_Surface *screen;
SDL_Surface *menu_screen;
SDL_Surface *black_screen;	/* for fade in/fade out */

#define NUM_COLORS 21

static int palette[NUM_COLORS * 3] = {
    0x00, 0x00, 0x00,	/*  0 */	0x3f, 0x3f, 0x25,	/*  1 */
    0x3f, 0x3b, 0x12,	/*  2 */	0x3f, 0x34, 0x00,	/*  3 */
    0x3f, 0x29, 0x00,	/*  4 */	0x3f, 0x1f, 0x00,	/*  5 */
    0x3f, 0x14, 0x00,	/*  6 */	0x3f, 0x00, 0x00,	/*  7 */
    0x0d, 0x0d, 0x0d,	/*  8 */	0x0b, 0x10, 0x15,	/*  9 */
    0x0b, 0x15, 0x1a,	/* 10 */	0x05, 0x08, 0x10,	/* 11 */
    0x20, 0x34, 0x34,	/* 12 */	0x10, 0x14, 0x32,	/* 13 */
    0x15, 0x1e, 0x27,	/* 14 */	0x3f, 0x3f, 0x3f,	/* 15 */
    0x3a, 0x3a, 0x3f,	/* 16 */	0x27, 0x3e, 0x3f,	/* 17 */
    0x20, 0x20, 0x20,	/* 18 */	0x0d, 0x13, 0x0f,	/* 19 */
    0x07, 0x0a, 0x08	/* 20 */
};

static Uint32 mapped_color[NUM_COLORS];
static int __color;
static int __white_color;

void setcolor(int c)
{
	__color = c;
}

void setwhitecolor(int c)
{
	__white_color = c;
}

static inline void put_pixel(SDL_Surface *surf, int x, int y, int c)
{
	Uint8 *bits;

	if (x < 0 || x >= surf->w || y < 0 || y >= surf->h) {
		return;
	}

	bits = ((Uint8 *) surf->pixels) + y * surf->pitch + x * 4;
	*((Uint32 *)(bits)) = mapped_color[c];
}

static inline void drawpixel(SDL_Surface *surf, int x, int y)
{
	put_pixel(surf, x, y, __color);
}

void drawhline(SDL_Surface *surf, int x, int y, int w)
{
	int i;

	for (i = 0; i < w; i++) {
		drawpixel(surf, x + i, y);
	}
}

void drawvline(SDL_Surface *surf, int x, int y, int h)
{
	int i;

	for (i = 0; i < h; i++) {
		drawpixel(surf, x, y + i);
	}
}

int msglen(struct font_header *f, char *s)
{
	unsigned int *p;
	int len;

	for (len = 0; *s; s++) {
		if (*s == '@') {
			continue;
		}
		for (p = &f->map[f->index[(int)*s]]; *p != 0xffffffff; p++) {
			len++;
		}
		len++;
	}

	return len;
}

int writechar(SDL_Surface *surf, struct font_header *f, int x, int y, char s, int c, int b)
{
	unsigned int p, *ptr;
	int x1 = 0, y1;

	ptr = &f->map[f->index[(int)s]];

	/* each character */
	while (1) {
		p = *ptr;
		if (p & (1 << 31))	/* end of character */
			break;

		/* each column */
		for (y1 = 0; y1 < f->h; y1++) {
			if (c >= 0) {
				if (p & 1) {
					setcolor(c);
					drawpixel(surf, x + x1, y - y1);
				} else if (b != -1) {
					setcolor(b);
					drawpixel(surf, x + x1, y - y1);
				}
			}
			p >>= 1;
		}

		/* fill rest of the column */
		if (b != -1 && c != -1) {
			setcolor(b);
			for (; y1 < f->h; y1++)
				drawpixel(surf, x + x1, y - y1);
		}

		x1++;
		ptr++;
	}

	return x1;
}

static int numspaces(char *s)
{
	int n;

	for (n = 0; *s; s++) {
		if (*s == ' ') {
			n++;
		}
	}

	return n;
}

int msg(SDL_Surface *surf, struct font_header *f, int x, int y, char *s, int c, int b, int sc, int len)
{
	int x1 = 0, y1;
	int color = c;
	int sp = 0, extra = 0;		/* justification spacing */

	if (s == NULL || *s == 0) {
		return 0;
	}

	/* justify message */
	if (len > 0) {
		int numsp = numspaces(s);
		if (numsp > 0) {
			len -= msglen(f, s);
			sp = len / numsp;
			extra = len % numsp;
		}
	}
	
	for (; *s; s++) {
		if (*s == '@') {	/* @word@ is printed in white */
			if (c > 0) {
				if (c == color)
					c = __white_color;
				else
					c = color;
			}
			continue;
		} else if (*s == ' ') {
			if (sp > 0) {	/* justification */
				x1 += sp;
			}
			if (extra > 0) {
				x1++;
				extra--;
			}
		}


		/* draw shadow */
		if (sc >= 0) {
			writechar(surf, f, x + x1 + 2, y + 2, *s, sc, b);
		}

		x1 += writechar(surf, f, x + x1, y, *s, c, b);

		/* inter-character spacing */
		if (b != -1 && c != -1) {
			for (y1 = 0; y1 < f->h; y1++)
				drawpixel(surf, x + x1, y - y1);
		}
		x1++;
	}

	if (c != -1) {
		//SDL_UpdateRect(surf, x, y - f->h + 1, x1, f->h);
	}

	return x1;
}

int init_video()
{
	int i;
	Uint32 rmask, gmask, bmask, amask;

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	rmask = 0xff000000;
	gmask = 0x000000ff;
	bmask = 0x0000ff00;
	amask = 0x00ff0000;
#else
	rmask = 0x00ff0000;
	gmask = 0x0000ff00;
	bmask = 0x000000ff;
	amask = 0xff000000;
#endif
    
	if (SDL_Init(SDL_INIT_VIDEO /*| SDL_INIT_AUDIO*/) < 0) {
		fprintf(stderr, "sdl: can't initialize: %s\n",
			SDL_GetError());
		return -1;
	}

#if 0
	if (opt.fullscreen)
		mode |= SDL_FULLSCREEN;
#endif

	if ((window = SDL_CreateWindow("xmdp", SDL_WINDOWPOS_UNDEFINED,
                        SDL_WINDOWPOS_UNDEFINED, 640, 480,
			SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE)) == NULL) {
		fprintf(stderr, "sdl: can't create window: %s\n", SDL_GetError());
		return -1;
	}
	atexit(SDL_Quit);

	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

	if ((renderer = SDL_CreateRenderer(window, -1, 0)) == NULL) {
		fprintf(stderr, "sdl: can't create renderer: %s\n", SDL_GetError());
		return -1;
	}

	if ((texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                               SDL_TEXTUREACCESS_STREAMING, 640, 480)) == NULL) {
		fprintf(stderr, "sdl: can't create texture: %s\n", SDL_GetError());
		return -1;
	}

	SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
	SDL_GL_SetSwapInterval(1);
	SDL_RenderClear(renderer);

	screen = SDL_CreateRGBSurface(0, 640, 480, 32,
					rmask, gmask, bmask, amask);
	menu_screen = SDL_CreateRGBSurface(0, 512, 960, 32,
					rmask, gmask, bmask, amask);

	if (menu_screen == NULL) {
		fprintf(stderr, "sdl: can't create menu surface: %s\n",
			SDL_GetError());
		return -1;
	}

	for (i = 0; i < NUM_COLORS; i++) {
		Uint8 r = palette[i * 3] << 2;
		Uint8 g = palette[i * 3 + 1] << 2;
		Uint8 b = palette[i * 3 + 2] << 2;
		mapped_color[i] = SDL_MapRGB(screen->format, r, g, b);
	}

	return 0;
}

void set_alpha(SDL_Texture *text, int alpha)
{
	SDL_SetTextureAlphaMod(text, alpha);
}

void render_screen(SDL_Texture *text, SDL_Surface *surf)
{
	SDL_UpdateTexture(text, NULL, surf->pixels, 640 * sizeof (Uint32));
	SDL_RenderCopy(renderer, text, NULL, NULL);
	SDL_RenderPresent(renderer);
}
