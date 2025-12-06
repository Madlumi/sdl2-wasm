#ifndef EGAME_TERMINAL_H
#define EGAME_TERMINAL_H

#include <SDL.h>
#include "../MENGINE/mutil.h"

void terminalInit(void);
void terminalTick(D dt);
void terminalRender(SDL_Renderer *r);

#endif
