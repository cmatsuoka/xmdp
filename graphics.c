#include <SDL/SDL.h>
#include "mdp.h"
#include "font.h"

SDL_Surface *screen;
SDL_Surface *menu_screen;

static int palette[] = {
    0x00, 0x00, 0x00,	/*  0 */	0x3f, 0x3f, 0x25,	/*  1 */
    0x3f, 0x3b, 0x12,	/*  2 */	0x3f, 0x34, 0x00,	/*  3 */
    0x3f, 0x29, 0x00,	/*  4 */	0x3f, 0x1f, 0x00,	/*  5 */
    0x3f, 0x14, 0x00,	/*  6 */	0x3f, 0x00, 0x00,	/*  7 */
    0x0d, 0x0d, 0x0d,	/*  8 */	0x0b, 0x10, 0x15,	/*  9 */
    0x0b, 0x15, 0x1a,	/* 10 */	0x05, 0x08, 0x10,	/* 11 */
    0x0b, 0x10, 0x15,	/* 12 */	0x10, 0x14, 0x32,	/* 13 */
    0x20, 0x34, 0x34,	/* 14 */	0xff, 0xff, 0xff	/* 15 */
};

static SDL_Color color[16];
static Uint32 mapped_color[16];
static int __color;

void setcolor(int c)
{
	__color = c;
}

static inline void put_pixel(SDL_Surface *surf, int x, int y, int c)
{
	Uint32 pixel;
	Uint8 *bits, bpp;

	pixel = mapped_color[c];

	bpp = surf->format->BytesPerPixel;
	bits = ((Uint8 *) surf->pixels) + y * surf->pitch + x * bpp;

	/* Set the pixel */
	switch (bpp) {
	case 1:
		*(Uint8 *)(bits) = pixel;
		break;
	case 2:
		*((Uint16 *) (bits)) = (Uint16) pixel;
		break;
	case 3:{
		Uint8 r, g, b;
		r = (pixel >> surf->format->Rshift) & 0xff;
		g = (pixel >> surf->format->Gshift) & 0xff;
		b = (pixel >> surf->format->Bshift) & 0xff;
		*((bits) + surf->format->Rshift / 8) = r;
		*((bits) + surf->format->Gshift / 8) = g;
		*((bits) + surf->format->Bshift / 8) = b;
		}
		break;
	case 4:
		*((Uint32 *)(bits)) = (Uint32)pixel;
		break;
	}
}

static inline void drawpixel(SDL_Surface *surf, int x, int y)
{
	put_pixel(surf, x, y, __color);
}

void fill(SDL_Surface *surf)
{
	int i, j;

	for (i = 0; i < surf->h; i++) {
		for (j = 0; j < surf->w; j++) {
			drawpixel(surf, i, j);
		}
	}
}

void drawhline(int x, int y, int w)
{
	int i;

	for (i = 0; i < w; i++) {
		drawpixel(screen, x + i, y);
	}
}

void drawvline(int x, int y, int h)
{
	int i;

	for (i = 0; i < h; i++) {
		drawpixel(screen, x, y + i);
	}
}

int writemsg(SDL_Surface *surf, struct font_header *f, int x, int y, char *s, int c, int b)
{
	int x1 = 0, y1 = 0;
	unsigned int p, *ptr;
	int color = c;

	if (!*s)
		return 0;

	for (; *s; s++) {
		if (*s == '@') {	/* @word@ is printed in white */
			if (c > 0) {
				if (c == color)
					c = 15;
				else
					c = color;
			}
			continue;
		}

		ptr = &f->map[f->index[(int)*s]];

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

		/* inter-character spacing */
		if (b != -1 && c != -1) {
			for (y1 = 0; y1 < f->h; y1++)
				drawpixel(surf, x + x1, y - y1);
		}
		x1++;
	}

	if (c != -1) {
		SDL_UpdateRect(surf, x, y - f->h + 1, x1, f->h);
	}

	return x1;
}

int init_video()
{
	int i, mode;
    
	if (SDL_Init (SDL_INIT_VIDEO /*| SDL_INIT_AUDIO*/) < 0) {
		fprintf(stderr, "sdl: can't initialize: %s\n",
			SDL_GetError());
		return -1;
	}

	mode = SDL_SWSURFACE | SDL_ANYFORMAT;

#if 0
	if (opt.fullscreen)
		mode |= SDL_FULLSCREEN;
#endif

	if ((screen = SDL_SetVideoMode (640, 480, 8, mode)) == NULL) {
		fprintf(stderr, "sdl: can't set video mode: %s\n",
			SDL_GetError());
		return -1;
	}
	atexit(SDL_Quit);

	menu_screen = SDL_CreateRGBSurface(SDL_SWSURFACE, 512, 960, 
			screen->format->BytesPerPixel * 8,
			screen->format->Rmask, screen->format->Gmask,
			screen->format->Bmask, screen->format->Amask);

	if (menu_screen == NULL) {
		fprintf(stderr, "sdl: can't create menu surface: %s\n",
			SDL_GetError());
		return -1;
	}

	SDL_WM_SetCaption("xmdp", "xmdp");

	for (i = 0; i < 16; i++) {
		color[i].r = palette[i * 3] << 2;
		color[i].g = palette[i * 3 + 1] << 2;
		color[i].b = palette[i * 3 + 2] << 2;
		mapped_color[i] = SDL_MapRGB(screen->format,
			color[i].r, color[i].g, color[i].b);
	}
	SDL_SetColors(screen, color, 0, 16);

	return 0;
}


