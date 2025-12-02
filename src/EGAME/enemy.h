#ifndef EGAME_ENEMY_H
#define EGAME_ENEMY_H
#include "entity.h"
#include <SDL.h>

void enemyInit(void);
void enemyTick(double dt);
void enemyRender(SDL_Renderer *r);
void enemyApplySlash(const Entity *slash, int damage, int slashId);
int enemyCollidesAt(float left, float right, float top, float bottom,
                    const Entity *ignore);

#endif
