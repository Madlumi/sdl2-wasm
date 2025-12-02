#ifndef EGAME_PLAYER_H
#define EGAME_PLAYER_H
#include <SDL.h>
#include "entity.h"

typedef struct {
    Entity body;
    float speed;
    int wallBlocked;
    float facingX;
    float facingY;
    int exp;
} Player;

const Player *playerGet(void);
void playerInit(void);
void playerTick(double dt);
void playerRender(SDL_Renderer *r);
void playerAddExp(int amount);

#endif
