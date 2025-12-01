#ifndef WOLF3D_H
#define WOLF3D_H

#include <SDL.h>
#include "../MENGINE/mutil.h"

void wolf3dInit();
void wolf3dTick(double dt);
void wolf3dRender(SDL_Renderer *renderer);

#endif
