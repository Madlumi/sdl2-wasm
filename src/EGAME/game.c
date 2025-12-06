#include "game.h"

#include "terminal.h"
#include "../MENGINE/renderer.h"
#include "../MENGINE/tick.h"

void gameInit() {
    ZOOM = 1.0;
    terminalInit();
    tickF_add(terminalTick);
    renderF_add(terminalRender);
}
