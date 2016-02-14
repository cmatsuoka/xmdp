#ifndef MDP_H_
#define MDP_H_

#include <SDL/SDL.h>
#include "font.h"

extern SDL_Surface *screen;
extern SDL_Surface *menu_screen;

#define MAX_ENTRIES 30
#define MAX_TITLES  10
#define MAX_COMMENT 25

struct menu_entry {
	char *filename;
	char *title;
	char *year;
	char *channels;
	char *time;
	char *comment[MAX_COMMENT];
};

struct menu {
	char *titles[MAX_TITLES];
	struct menu_entry entry[MAX_ENTRIES];
	char *lastline;
};

void setcolor(int);
void drawhline(int, int, int);
void drawvline(int, int, int);
int init_video(void);
int writemsg(SDL_Surface *surf, struct font_header *, int, int, char *, int, int);
int parse_mdi(void);

#endif