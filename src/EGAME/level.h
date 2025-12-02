#ifndef LEVEL_H
#define LEVEL_H

#include <SDL.h>
#include "render3d.h"

#define MAP_W 50
#define MAP_H 50
#define TILE_SIZE 1.0f
#define WALL_HEIGHT 0.1f

extern MeshInstance faces[MAP_W * MAP_H * 6];
extern int faceCount;

extern MeshInstance floorFaces[MAP_W * MAP_H];
extern int floorCount;

void levelInit(void);
int tileHeight(int x, int z);
float sampleHeightAt(float wx, float wz);
SDL_Texture *levelGetGrassTexture(void);

#endif
