#ifndef EGAME_WORLD_H
#define EGAME_WORLD_H
#include <SDL.h>

int worldMapWidth(void);
int worldMapHeight(void);
int worldTileWidth(void);
int worldTileHeight(void);
float worldPixelWidth(void);
float worldPixelHeight(void);

int worldTileSolid(int tx, int ty);

void worldInit(void);
void worldTick(double dt);
void worldRender(SDL_Renderer *r);

#endif
