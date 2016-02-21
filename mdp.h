#ifndef MDP_H_
#define MDP_H_

#include <SDL/SDL.h>
#include "font.h"

#define MAX_ENTRIES 30
#define MAX_TITLES  10
#define MAX_COMMENT 25

struct menu_entry {
	char *filename;
	char *title;
	char *year;
	char *comment[MAX_COMMENT];
	int ystart, yend;
};

struct menu {
	char *titles[MAX_TITLES];
	struct menu_entry entry[MAX_ENTRIES];
	char *lastline;
};

extern SDL_Surface *screen;
extern SDL_Surface *menu_screen;
extern struct menu menu;

void setcolor(int);
void drawhline(int, int, int);
void drawvline(int, int, int);
void fill(SDL_Surface *);
int init_video(void);
int msg(SDL_Surface *, struct font_header *, int, int, char *, int, int, int);
int parse_mdi(void);

#endif
