/* xmdp written by Claudio Matsuoka and Hipolito Carraro Jr
 * Original MDP.EXE for DOS by Future Crew
 *
 * 1.4.0: Ported to libxmp 4.1 with sdl sound
 * 1.3.0: Ported to libxmp 4.0
 * 1.2.0: Ported to xmp 3.0 and sdl
 * 1.1.0: Raw keys handled by Russel Marks' rawkey functions
 *        Links to libxmp 1.1.0, shm stuff removed
 * 1.0.2: execlp() bug fixed by Takashi Iwai
 * 1.0.1: X shared memory allocation test fixed
 * 1.0.0: public release
 * 0.0.1: X11 supported hacked into the code.
 */

/*
 * This is free and unencumbered software released into the public domain.
 *
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any
 * means.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <SDL/SDL.h>
#include "xmp.h"
#include "font.h"

#define VERSION "1.4.0"
#define MAX_TIMER 1600		/* Expire time */
#define SRATE 44100		/* Sampling rate */

#define MODE_MENU   0		/* Song menu screen */
#define MODE_PLAYER 1		/* Player screen */

struct channel_info {
    int timer;
    int vol;
    int note;
    int bend;
    int y2, y3;
};


#define rightmsg(f,x,y,s,c,b)						\
    writemsg(f, (x) - writemsg (f,0,0,s,-1,0), y, s, c, b)

#define update_counter(a,b,y) {						\
    if ((a) != (b)) {							\
	sprintf(buf, "%d  ", (int)(a));					\
	writemsg(&font2, 590, y, buf, 14, 9);				\
	b = a;								\
    }									\
}


extern struct font_header font1;
extern struct font_header font2;

static SDL_Surface *screen;
static SDL_Surface *menu;
static struct channel_info channel_info[40];

static int palette[] = {
    0x00, 0x00, 0x00,	/*  0 */	0x3f, 0x3f, 0x25,	/*  1 */
    0x3f, 0x3b, 0x12,	/*  2 */	0x3f, 0x34, 0x00,	/*  3 */
    0x3f, 0x29, 0x00,	/*  4 */	0x3f, 0x1f, 0x00,	/*  5 */
    0x3f, 0x14, 0x00,	/*  6 */	0x3f, 0x00, 0x00,	/*  7 */
    0x0d, 0x0d, 0x0d,	/*  8 */	0x0b, 0x10, 0x15,	/*  9 */
    0x0b, 0x15, 0x1a,	/* 10 */	0x05, 0x08, 0x10,	/* 11 */
    0xd0, 0x00, 0x00,	/* 12 */	0x10, 0x14, 0x32,	/* 13 */
    0x10, 0x18, 0x20,	/* 14 */	0xff, 0xff, 0xff	/* 15 */
};

static SDL_Color color[16];
static Uint32 mapped_color[16];
static int __color;
static xmp_context ctx;
static int paused;
static int end_of_song;
static int volume;
static int mode = MODE_MENU;

static inline void setcolor(int c)
{
	__color = c;
}

static inline void put_pixel(int x, int y, int c)
{
	Uint32 pixel;
	Uint8 *bits, bpp;

	pixel = mapped_color[c];

	bpp = screen->format->BytesPerPixel;
	bits = ((Uint8 *) screen->pixels) + y * screen->pitch + x * bpp;

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
		r = (pixel >> screen->format->Rshift) & 0xff;
		g = (pixel >> screen->format->Gshift) & 0xff;
		b = (pixel >> screen->format->Bshift) & 0xff;
		*((bits) + screen->format->Rshift / 8) = r;
		*((bits) + screen->format->Gshift / 8) = g;
		*((bits) + screen->format->Bshift / 8) = b;
		}
		break;
	case 4:
		*((Uint32 *)(bits)) = (Uint32)pixel;
		break;
	}
}

void drawpixel(int x, int y)
{
	put_pixel(x, y, __color);
}

void drawhline (int x, int y, int w)
{
	int i;

	for (i = 0; i < w; i++)
		drawpixel(x + i, y);
}


void drawvline (int x, int y, int h)
{
	int i;

	for (i = 0; i < h; i++)
		drawpixel(x, y + i);
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

	menu = SDL_CreateRGBSurface(SDL_SWSURFACE, 512, 960, 
			screen->format->BytesPerPixel * 8,
			screen->format->Rmask, screen->format->Gmask,
			screen->format->Bmask, screen->format->Amask);

	if (menu == NULL) {
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

void prepare_screen()
{
	int i, j, x, y;

	setcolor(10);
	drawhline(0, 0, 640);
	drawhline(0, 1, 640);
	setcolor(9);
	for (i = 2; i < 72; i++)
		drawhline(0, i, 640);
	setcolor(11);
	drawhline(0, 72, 640);
	drawhline(0, 73, 640);
	setcolor(8);
	for (i = 0; i < 39; i++) {
		x = i * 16 + 15;
		drawvline(x, 85, 386);
		for (j = 0; j < 17; j++) {
			y = 470 - j * 24;
			drawhline(x - 1, y, 3);
			if (!(j % 2))
				drawhline(x - 1, y - 1, 3);
		}
	}
	setcolor(10);
	drawhline(10, 65, 129);
	drawvline(138, 57, 9);
	setcolor(11);
	drawhline(10, 57, 129);
	drawvline(10, 57, 9);
}

void draw_lines(int i, int a, int b, int c)
{
	int y = a;

	setcolor(c);
	i = i * 16 + 1;
	for (; a < b; a++)
		drawhline(i, a, 13);
	SDL_UpdateRect(screen, i, y, 13, a - y + 1);
}

void draw_bars()
{
	int p, v, i, y0, y1, y2, y3, k;

	for (i = 0; i < 40; i++) {
		struct channel_info *ci = &channel_info[i];

		y2 = ci->y2;
		y3 = ci->y3;

		if ((v = ci->vol))
			ci->vol--;

		if (ci->timer && !--ci->timer) {
			draw_lines(i, y2, y3, 0);
			ci->y2 = ci->y3 = ci->note = 0;
			continue;
		}

		if (ci->note == 0)
			continue;

		p = 538 - ci->note * 4 - ci->bend / 25;

		if (p < 86)
			p = 86;
		if (p > 470)
			p = 470;
		v++;

		ci->y2 = y0 = p - v;
		ci->y3 = y1 = p + v;
		k = abs(((i + 10) % 12) - 6) + 1;

		if ((y0 == y2) && (y1 == y3))
			continue;

		if ((y1 < y2) || (y3 < y0)) {
			draw_lines(i, y2, y3, 0);
			draw_lines(i, y0, y1, k);
			continue;
		}

		if (y1 < y3)
			draw_lines(i, y1, y3, 0);
		if (y2 < y0)
			draw_lines(i, y2, y0, 0);
		if (y0 < y2)
			draw_lines(i, y0, y2, k);
		if (y3 < y1)
			draw_lines(i, y3, y1, k);
	}
}

void draw_progress(int pos)
{
	static int i, oldpos = 0;

	if (pos == oldpos)
		return;
	if (pos > oldpos) {
		setcolor(13);
		for (i = oldpos + 1; i <= pos; i++)
			drawvline(11 + i, 58, 7);
	}
	if (pos < oldpos) {
		setcolor(9);
		for (i = pos + 1; i <= oldpos; i++)
			drawvline(11 + i, 58, 7);
	}
	oldpos = pos;
	SDL_UpdateRect(screen, 11, 58, 127, 7);
}

int writemsg(struct font_header *f, int x, int y, char *s, int c, int b)
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
						drawpixel(x + x1, y - y1);
					} else if (b != -1) {
						setcolor(b);
						drawpixel(x + x1, y - y1);
					}
				}
				p >>= 1;
			}

			/* fill rest of the column */
			if (b != -1 && c != -1) {
				setcolor(b);
				for (; y1 < f->h; y1++)
					drawpixel(x + x1, y - y1);
			}

			x1++;
			ptr++;
		}

		/* inter-character spacing */
		if (b != -1 && c != -1) {
			for (y1 = 0; y1 < f->h; y1++)
				drawpixel(x + x1, y - y1);
		}
		x1++;
	}

	if (c != -1)
		SDL_UpdateRect(screen, x, y - f->h + 1, x1, f->h);

	return x1;
}


void shadowmsg(struct font_header *f, int x, int y, char *s, int c, int b)
{
	writemsg(f, x + 2, y + 2, s, 0, b);
	writemsg(f, x, y, s, c, b);
}

static void process_menu_events(int key)
{
}

static void process_player_events(int key)
{
	switch (key) {
	case SDLK_SPACE:
		SDL_PauseAudio(paused ^= 1);
		break;
	case SDLK_LEFT:
		xmp_prev_position(ctx);
		SDL_PauseAudio(paused = 0);
		break;
	case SDLK_RIGHT:
		xmp_next_position(ctx);
		SDL_PauseAudio(paused = 0);
		break;
	case SDLK_MINUS:
	case SDLK_KP_MINUS:
		volume--;
		if (volume < 0)
			volume = 0;
		xmp_set_player(ctx, XMP_PLAYER_VOLUME,
						100 * volume / 64);
		SDL_PauseAudio(paused = 0);
		break;
	case SDLK_PLUS:
	case SDLK_KP_PLUS:
	case SDLK_EQUALS:
		volume++;
		if (volume > 64)
			volume = 64;
		xmp_set_player(ctx, XMP_PLAYER_VOLUME,
						100 * volume / 64);
		SDL_PauseAudio(paused = 0);
		break;
	}
}

static void process_events()
{
	SDL_Event event;

	while (SDL_PollEvent(&event) > 0) {
		int key;

		if (event.type != SDL_KEYDOWN)
			continue;

		key = (int)event.key.keysym.sym;

		switch (key) {
		case SDLK_F10:
			xmp_stop_module(ctx);
			SDL_PauseAudio(paused = 0);
			break;
		case SDLK_ESCAPE:
			if (mode == MODE_MENU) {
				mode = MODE_PLAYER;
			} else {
				mode = MODE_MENU;
			}
			break;
		default:
			if (mode == MODE_MENU) {
				process_menu_events(key);
			} else {
				process_player_events(key);
			}
		}
	}
}


static void draw_menu()
{
}

static void draw_screen(struct xmp_module_info *mi, struct xmp_frame_info *fi)
{
	static int pos = -1, pat = -1, row = -1, vol = -1;
	char buf[80];
	int i;

	update_counter(fi->pos, pos, 20);
	update_counter(fi->pattern, pat, 36);
	update_counter(volume, vol, 68);

	if (fi->row != row) {
		draw_progress(pos * 126 / mi->mod->len +
			      fi->row * 126 / mi->mod->len / fi->num_rows);
	}
	update_counter(fi->row, row, 52);

	for (i = 0; i < mi->mod->chn; i++) {
		struct xmp_channel_info *info = &fi->channel_info[i];
		struct channel_info *ci = &channel_info[info->instrument];

		if (info->instrument < 40) {
			if (info->note < 0x80) {
				ci->note = info->note;
				ci->bend = info->pitchbend;
				ci->vol = info->volume;
				ci->timer = MAX_TIMER / 30;
				if (ci->vol > 8) {
					ci->vol = 8;
				}
			} else {
				ci->note = 0;
			}
		}
	}

	draw_bars();
}

/*
 * Sound
 */

static void fill_audio(void *udata, unsigned char *stream, int len)
{
	if (xmp_play_buffer(ctx, stream, len, 1) < 0)
		end_of_song = 1;
}

int sound_init(int sampling_rate, int channels)
{
	SDL_AudioSpec a;

	a.freq = sampling_rate;
	a.format = (AUDIO_S16);
	a.channels = channels;
	a.samples = 2048;
	a.callback = fill_audio;
	a.userdata = NULL;

	if (SDL_OpenAudio(&a, NULL) < 0) {
		fprintf(stderr, "%s\n", SDL_GetError());
		return -1;
	}

	return 0;
}

void sound_deinit()
{
	SDL_CloseAudio();
}


/*
 * Main
 */

void usage()
{
	printf("Usage: xmdp [-v] module\n");
}

int main(int argc, char **argv)
{
	int o;
	struct xmp_module_info mi;
	struct xmp_frame_info fi;

	while ((o = getopt(argc, argv, "v")) != -1) {
		switch (o) {
		case 'v':
			printf("Version %s\n", VERSION);
			exit(0);
		default:
			exit(1);
		}
	}

	if (!argv[optind]) {
		usage();
		exit(1);
	}

	if (sound_init(SRATE, 2) < 0) {
		fprintf(stderr, "%s: can't initialize sound\n", argv[0]);
		exit(1);
	}

	ctx = xmp_create_context();

	if (xmp_load_module(ctx, argv[optind]) < 0) {
		fprintf(stderr, "%s: can't load %s\n", argv[0], argv[optind]);
		goto err1;
	}

	init_video();
	prepare_screen();

	rightmsg(&font2, 500, 36, "Spacebar to pause", 14, -1);
	rightmsg(&font2, 500, 52, "+/- to change volume", 14, -1);
	rightmsg(&font2, 500, 20, "F10 to quit", 14, -1);
	rightmsg(&font2, 500, 68, "Arrows to change pattern", 14, -1);
	writemsg(&font2, 540, 20, "Ord", 14, -1);
	writemsg(&font2, 540, 36, "Pat", 14, -1);
	writemsg(&font2, 540, 52, "Row", 14, -1);
	writemsg(&font2, 540, 68, "Vol", 14, -1);
	writemsg(&font2, 580, 20, ":", 14, -1);
	writemsg(&font2, 580, 36, ":", 14, -1);
	writemsg(&font2, 580, 52, ":", 14, -1);
	writemsg(&font2, 580, 68, ":", 14, -1);

	SDL_UpdateRect(screen, 0, 0, 640, 480);

	xmp_start_player(ctx, SRATE, 0);
	xmp_get_module_info(ctx, &mi);
	shadowmsg(&font1, 10, 26, mi.mod->name, 15, -1);

	end_of_song = 0;
	volume = 64;
	SDL_PauseAudio(paused = 0);

	while (!end_of_song) {
		process_events();
		SDL_LockAudio();
		xmp_get_frame_info(ctx, &fi);
		SDL_UnlockAudio();

		if (mode == MODE_MENU) {
			draw_menu();
		} else {
			draw_screen(&mi, &fi);
		}
		usleep(100000);
	}

	xmp_end_player(ctx);

	sound_deinit();
	xmp_free_context(ctx);

	exit(0);

      err1:
	sound_deinit();
	xmp_free_context(ctx);

	exit(1);
}
