#ifndef EGAME_NPC_H
#define EGAME_NPC_H
#include <SDL.h>

void npcInit(void);
void npcTick(double dt);
void npcRender(SDL_Renderer *r);

#endif
