#include "game.h"
#include "wolf3d.h"
#include "../MENGINE/tick.h"
#include "../MENGINE/renderer.h"

void gameInit() {
    wolf3dInit();
    tickF_add(wolf3dTick);
    renderF_add(wolf3dRender);
}
