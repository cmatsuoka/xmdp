#ifndef MDP_H_
#define MDP_H_

#include <SDL/SDL.h>
#include "font.h"

extern SDL_Surface *screen;
extern SDL_Surface *menu;

void setcolor(int);
void drawhline(int, int, int);
void drawvline(int, int, int);
int init_video(void);
int writemsg(struct font_header *, int, int, char *, int, int);

#endif
