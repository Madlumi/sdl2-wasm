#ifndef EGAME_ENEMY_H
#define EGAME_ENEMY_H
#include <SDL.h>

void enemyInit(void);
void enemyTick(double dt);
void enemyRender(SDL_Renderer *r);

#endif
