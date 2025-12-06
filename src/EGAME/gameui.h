#ifndef EGAME_GAMEUI_H
#define EGAME_GAMEUI_H
#include <SDL.h>

void gameuiRender(SDL_Renderer *r);
void gameuiTick(double dt);
int gameuiClickConsumed(void);
int gameuiMouseOverUI(void);

#endif
