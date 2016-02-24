#ifndef MDP_H_
#define MDP_H_

#include <SDL2/SDL.h>
#include "font.h"

#define MAX_ENTRIES 30
#define MAX_TITLES  10
#define MAX_COMMENT 25

#define FILENAME_SIZE 80

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
	int num_entries;
};

extern SDL_Renderer *renderer;
extern SDL_Texture *texture;
extern SDL_Surface *screen;
extern SDL_Surface *menu_screen;
extern SDL_Surface *black_screen;
extern struct menu menu;

void setcolor(int);
void setwhitecolor(int);
void drawhline(SDL_Surface *, int, int, int);
void drawvline(SDL_Surface *, int, int, int);
void set_alpha(SDL_Surface *, int);
int init_video(void);
int msg(SDL_Surface *, struct font_header *, int, int, char *, int, int, int, int);
int msglen(struct font_header *, char *);
int parse_mdi(void);
int match_filename(char *, char *, size_t);

#endif
