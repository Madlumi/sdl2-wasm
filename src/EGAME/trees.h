#ifndef EGAME_TREES_H
#define EGAME_TREES_H

#include <SDL.h>
#include "entity.h"

void treesInit(void);
void treesTick(double dt);
void treesRender(SDL_Renderer *r);
void treesApplySlash(const Entity *slash, int damage, int slashId);
int treesCollidesAt(float left, float right, float top, float bottom);

#endif
