/* xmdp written by Claudio Matsuoka and Hipolito Carraro Jr
 * Original MDP.EXE for DOS by Future Crew
 *
 * 1.5.0: Added MDI parser and song selector
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
#include <SDL2/SDL.h>
#include <xmp.h>
#include "mdp.h"

#define VERSION "1.5.0"
#define MAX_TIMER 1600		/* Expire time */
#define SRATE 44100		/* Sampling rate */

#define MODE_MENU   0		/* Song menu screen */
#define MODE_PLAYER 1		/* Player screen */

#define MENU_HEIGHT 480
#define MENU_OFFSET 200
#define MENU_START 200

struct channel_info {
    int timer;
    int vol;
    int note;
    int bend;
    int y2, y3;
};


#define writemsg(surf,f,x,y,s,c,b)					\
    msg(surf,f,x,y,s,c,b,-1,-1)

#define shadowmsg(surf,f,x,y,s,c,sc,b)					\
    msg(surf,f,x,y,s,c,b,sc,-1)

#define centermsg(surf,f,x,y,s,c,b)					\
    shadowmsg(surf, f, (x) - msglen(f,s) / 2, y, s, c, 0, b)

#define justifymsg(surf,f,x,y,s,c,sc,b) 				\
    msg(surf,f,x,y,s,c,b,sc,512-6);					\

#define rightmsg(surf,f,x,y,s,c,sc,b)					\
    msg(surf, f, (x) - msglen(f,s), y, s, c, b, sc, -1)

#define update_counter(a,b,y) {						\
    if (mode_changed || (a) != (b)) {					\
	sprintf(buf, "%d  ", (int)(a));					\
	writemsg(screen, &font2, 590, y, buf, 14, 9);			\
	b = a;								\
    }									\
}


extern struct font_header font1;
extern struct font_header font2;

static struct channel_info channel_info[40];

static xmp_context ctx;
static int paused;
static int end_of_player = 0;
static int volume;
static int mode = MODE_MENU;
static int mode_changed = 1;
static int current_mod;
static int ofs_y, blit_y;
static int menu_fade_in;
static int menu_fade_out;
static int standalone = 0;

#define ACTION_PLAY	0
#define ACTION_SWITCH	1
#define ACTION_QUIT	2
static int action;

static struct xmp_module_info mi;


static float interpolate(float in)
{
	return in >= 0.0f ? in * in * in : -in * in * in;
}

void draw_lines(int i, int a, int b, int c)
{
	setcolor(c);
	i = i * 16 + 1;
	for (; a < b; a++) {
		drawhline(screen, i, a, 13);
	}
}

void draw_bars()
{
	int p, v, i, y0, y1, y2, y3, k;

	for (i = 0; i < 40; i++) {
		struct channel_info *ci = &channel_info[i];

		/* previous bar limits */
		y2 = ci->y2;
		y3 = ci->y3;

		if ((v = ci->vol)) {
			ci->vol--;
		}

		if (ci->timer && !--ci->timer) {
			draw_lines(i, y2, y3, 0);
			ci->y2 = ci->y3 = ci->note = 0;
			continue;
		}

		if (ci->note == 0) {
			continue;
		}

		/* find pitch */
		p = 538 - ci->note * 4 - ci->bend / 25;

		if (p < 86)
			p = 86;
		if (p > 470)
			p = 470;
		v++;

		if (mode_changed) {
			/* set old bar limits to lowest value */
			y2 = y3 = p;	
		}

		/* current bar limits */
		ci->y2 = y0 = p - v;
		ci->y3 = y1 = p + v;
		k = abs(((i + 10) % 12) - 6) + 1;

		if (y0 == y2 && y1 == y3) {
			/* nothing changed */
			continue;
		}

		if (y1 < y2 || y3 < y0) {
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

	if (mode_changed) {
		oldpos = 0;
	}

	if (pos == oldpos)
		return;
	if (pos > oldpos) {
		setcolor(13);
		for (i = oldpos + 1; i <= pos; i++)
			drawvline(screen, 11 + i, 58, 7);
	}
	if (pos < oldpos) {
		setcolor(9);
		for (i = pos + 1; i <= oldpos; i++)
			drawvline(screen, 11 + i, 58, 7);
	}
	oldpos = pos;
}

int start_player(char *filename)
{
	char name[FILENAME_SIZE];

	match_filename(filename, name, FILENAME_SIZE);

	if (xmp_load_module(ctx, filename) < 0) {
		return -1;
	}

	xmp_start_player(ctx, SRATE, 0);
	xmp_get_module_info(ctx, &mi);

	volume = 64;
	SDL_PauseAudio(paused = 0);

	return 0;
}

void stop_player()
{
	if (xmp_get_player(ctx, XMP_PLAYER_STATE) != XMP_STATE_LOADED) {
		return;
	}

	xmp_end_player(ctx);
	xmp_release_module(ctx);
}

void update_menu_screen()
{
	int offset = ofs_y ? ofs_y * interpolate(1.0f * blit_y / ofs_y) : 0;
	SDL_Rect r0 = { 0, MENU_OFFSET - offset, 512, MENU_HEIGHT };
	SDL_Rect r = { 64, 0, 512, MENU_HEIGHT };

	SDL_BlitSurface(menu_screen, &r0, screen, &r);
}

static void draw_menu_borders()
{
	int i;

	setcolor(9);
	for (i = 0; i < MENU_HEIGHT; i++) {
		drawhline(screen, 0, i, 64);
		drawhline(screen, 576, i, 64);
	}
}

void prepare_menu_screen()
{
	struct menu_entry *e;
	int i, j, ypos, ystart, yend;

	setwhitecolor(16);
	draw_menu_borders();

	/* fill background */
	for (i = 0; i < 960; i++) {
		drawhline(menu_screen, 0, i, 512);
	}

	ystart = menu.entry[current_mod].ystart;
	yend = menu.entry[current_mod].yend;
	ypos = MENU_START - ystart + MENU_OFFSET;

	/* draw selection box */
	setcolor(14);
	for (i = 0; i < yend - ystart; i++) {
		int y = MENU_START + i - 4 + MENU_OFFSET;
		drawhline(menu_screen, 0, y, 512);
	}

	/* write titles */
	for (i = 0; menu.titles[i] && i < MAX_TITLES; i++) {
		centermsg(menu_screen, &font1, 256, ypos, menu.titles[i], 12, -1);
		ypos += 20;
	}

	ypos += 5;

	/* write entries */
	for (j = 0; menu.entry[j].filename && j < MAX_ENTRIES; j++) {
		int c, sc;

		ypos += 40;
		e = &menu.entry[j];

		if (j == current_mod) {
			setwhitecolor(15);
			c = 15;
			sc = 19;
		} else {
			setwhitecolor(16);
			c = 16;
			sc = 20;
		}

		/* title */
		shadowmsg(menu_screen, &font1, 2, ypos, e->title, c, sc, -1);

		/* year */
		rightmsg(menu_screen, &font2, 510, ypos, e->year, c, sc, -1);

		ypos += 4;

		/* comment lines */
		for (i = 0; e->comment[i] && i < MAX_COMMENT; i++) {
			int c = 12;

			if (j == current_mod) {
				c = 17;
			}

			ypos += 18;
			if (e->comment[i][0] == '$') {
				justifymsg(menu_screen, &font2, 2, ypos, e->comment[i] + 1, c, sc, -1);
			} else {
				shadowmsg(menu_screen, &font2, 2, ypos, e->comment[i], c, sc, -1);
			}
		}
	}

	update_menu_screen();
}

void prepare_player_screen()
{
	int i, j, x, y;

	memset(screen->pixels, 0, screen->w * screen->h * screen->format->BytesPerPixel);

	setwhitecolor(15);
	setcolor(10);

	drawhline(screen, 0, 0, 640);
	drawhline(screen, 0, 1, 640);
	setcolor(9);
	for (i = 2; i < 72; i++) {
		drawhline(screen, 0, i, 640);
	}
	setcolor(11);
	drawhline(screen, 0, 72, 640);
	drawhline(screen, 0, 73, 640);
	setcolor(8);
	for (i = 0; i < 39; i++) {
		x = i * 16 + 15;
		drawvline(screen, x, 85, 386);
		for (j = 0; j < 17; j++) {
			y = 470 - j * 24;
			drawhline(screen, x - 1, y, 3);
			if (!(j % 2))
				drawhline(screen, x - 1, y - 1, 3);
		}
	}
	setcolor(10);
	drawhline(screen, 10, 65, 129);
	drawvline(screen, 138, 57, 9);
	setcolor(11);
	drawhline(screen, 10, 57, 129);
	drawvline(screen, 10, 57, 9);

	rightmsg(screen, &font2, 500, 36, "Spacebar to pause", 14, -1, -1);
	rightmsg(screen, &font2, 500, 52, "+/- to change volume", 14, -1, -1);
	rightmsg(screen, &font2, 500, 20, "F10 to quit", 14, -1, -1);
	rightmsg(screen, &font2, 500, 68, "Arrows to change pattern", 14, -1, -1);
	writemsg(screen, &font2, 540, 20, "Ord", 14, -1);
	writemsg(screen, &font2, 540, 36, "Pat", 14, -1);
	writemsg(screen, &font2, 540, 52, "Row", 14, -1);
	writemsg(screen, &font2, 540, 68, "Vol", 14, -1);
	writemsg(screen, &font2, 580, 20, ":", 14, -1);
	writemsg(screen, &font2, 580, 36, ":", 14, -1);
	writemsg(screen, &font2, 580, 52, ":", 14, -1);
	writemsg(screen, &font2, 580, 68, ":", 14, -1);
}

static void switch_to_player()
{
	if (mode == MODE_MENU) {
		mode = MODE_PLAYER;
		prepare_player_screen();
		mode_changed = 1;
	}
}

static void switch_to_menu()
{
	if (mode == MODE_PLAYER) {
		mode = MODE_MENU;
		prepare_menu_screen();
		mode_changed = 1;
		menu_fade_in = 255;
	}
}

static void process_menu_events(int key)
{
	int ystart, yend;

	if (blit_y != 0) {
		return;
	}

	switch (key) {
	case SDLK_UP:
		if (current_mod > 0) {
			current_mod--;
			ystart = menu.entry[current_mod].ystart;
			yend = menu.entry[current_mod].yend;
			ofs_y = -(yend - ystart + 14);
			blit_y = ofs_y;
			prepare_menu_screen();
		}
		break;
	case SDLK_DOWN:
		if (current_mod < menu.num_entries - 1) {
			ystart = menu.entry[current_mod].ystart;
			yend = menu.entry[current_mod].yend;
			current_mod++;
			ofs_y = yend - ystart + 14;
			blit_y = ofs_y;
			prepare_menu_screen();
		}
		break;
	case SDLK_RETURN:
		action = ACTION_PLAY;
		menu_fade_out = 255;
		break;
	}
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

		if (event.type != SDL_KEYDOWN) {
			continue;
		}

		key = (int)event.key.keysym.sym;

		switch (key) {
		case SDLK_F10:
			if (mode == MODE_MENU) {
				action = ACTION_QUIT;
				menu_fade_out = 255;
			} else {
				xmp_stop_module(ctx);
				SDL_PauseAudio(paused = 0);
				end_of_player = 1;
			}
			break;
		case SDLK_ESCAPE:
			if (standalone) {
				xmp_stop_module(ctx);
				SDL_PauseAudio(paused = 0);
				end_of_player = 1;
			}
			if (mode == MODE_MENU) {
				action = ACTION_SWITCH;
				menu_fade_out = 255;
			} else {
				switch_to_menu();
				menu_fade_in = 255;
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

static void fade(SDL_Surface *surf, int val)
{
	draw_menu_borders();
	update_menu_screen();
	set_alpha(surf, val);
	SDL_BlitSurface(black_screen, 0, screen, 0);
}

#define STEP 3
#define FADE_STEP 24

static void draw_menu_screen()
{
	int flip = 0;

	mode_changed = 0;

	if (blit_y > STEP) {
		blit_y -= STEP;
		update_menu_screen();
		flip = 1;
	} else if (blit_y < -STEP) {
		blit_y += STEP;
		update_menu_screen();
		flip = 1;
	} else {
		blit_y = 0;
	}

	if (menu_fade_in > FADE_STEP) {
		menu_fade_in -= FADE_STEP;
		fade(black_screen, menu_fade_in);
		flip = 1;
	} else if (menu_fade_in > 0) {
		menu_fade_in = 0;
	}

	if (menu_fade_out > FADE_STEP) {
		menu_fade_out -= FADE_STEP;
		fade(black_screen, 255 - menu_fade_out);
		flip = 1;
	} else if (menu_fade_out > 0) {
		char *filename;

		menu_fade_out = 0;

		switch (action) {
		case ACTION_PLAY:
			stop_player();
			filename = menu.entry[current_mod].filename;
			if (start_player(filename) < 0) {
				perror(filename);
			}
			switch_to_player();
			break;
		case ACTION_SWITCH:
			switch_to_player();
			break;
		case ACTION_QUIT:
			xmp_stop_module(ctx);
			SDL_PauseAudio(paused = 0);
			end_of_player = 1;
		}

		flip = 1;
	}

	if (flip) {
		SDL_UpdateTexture(texture, NULL, screen->pixels, 640 * sizeof (Uint32));
		SDL_RenderClear(renderer);
		SDL_RenderCopy(renderer, texture, NULL, NULL);
		SDL_RenderPresent(renderer);
	}
}

static void draw_player_screen(struct xmp_module_info *mi, struct xmp_frame_info *fi)
{
	static int pos = -1, pat = -1, row = -1, vol = -1;
	char buf[80];
	int i;

	if (xmp_get_player(ctx, XMP_PLAYER_STATE) != XMP_STATE_PLAYING) {
		return;
	}

	if (mode_changed) {
		shadowmsg(screen, &font1, 10, 26, mi->mod->name, 15, 0, -1);
	}

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

	SDL_UpdateTexture(texture, NULL, screen->pixels, 640 * sizeof (Uint32));
	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, texture, NULL, NULL);
	SDL_RenderPresent(renderer);

	mode_changed = 0;
}

/*
 * Sound
 */

static void fill_audio(void *udata, unsigned char *stream, int len)
{
	xmp_play_buffer(ctx, stream, len, 0);
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

	if (sound_init(SRATE, 2) < 0) {
		fprintf(stderr, "%s: can't initialize sound\n", argv[0]);
		exit(1);
	}

	ofs_y = 0;
	action = 0;
	current_mod = 0;
	menu_fade_in = 255;

	ctx = xmp_create_context();

	if (argv[optind] != NULL) {
		standalone = 1;
		if (start_player(argv[optind]) < 0) {
			fprintf(stderr, "%s: can't load %s\n", argv[0], argv[optind]);
			goto err1;
		}
	}

	init_video();
	SDL_PollEvent(0);		/* otherwise screen starts blank */

	if (standalone) {
		switch_to_player();
	} else {
		if (parse_mdi() < 0) {
			if (!argv[optind]) {
				usage();
				exit(1);
			}
		}

		prepare_menu_screen();
	}
	
	while (!end_of_player) {
		process_events();

		SDL_LockAudio();
		xmp_get_frame_info(ctx, &fi);
		SDL_UnlockAudio();

		if (mode == MODE_MENU) {
			draw_menu_screen();
			usleep(10000);
		} else {
			draw_player_screen(&mi, &fi);
			usleep(100000);
		}
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
