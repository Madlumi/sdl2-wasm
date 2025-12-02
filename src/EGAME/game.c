#include "game.h"
#include "player.h"
#include "enemy.h"
#include "npc.h"
#include "trees.h"
#include "world.h"
#include "gameui.h"
#include "../MENGINE/keys.h"
#include "../MENGINE/renderer.h"
#include "../MENGINE/tick.h"
#include <math.h>

static void handleZoomInput(void) {
    if (mouseWheelMoved == 0) return;

    const float step = 0.25f;
    const float minZoom = 1.0f;
    const float maxZoom = 24.0f;

    float newZoom = (float)ZOOM + (float)mouseWheelMoved * step;
    newZoom = fmaxf(minZoom, fminf(maxZoom, newZoom));
    ZOOM = newZoom;
    mouseWheelMoved = 0;
}

static void gameTick(double dt) {
    handleZoomInput();
    worldTick(dt);
    treesTick(dt);
    playerTick(dt);
    npcTick(dt);
    enemyTick(dt);
}

static void gameRender(SDL_Renderer *r) {
    worldRender(r);
    treesRender(r);
    playerRender(r);
    npcRender(r);
    enemyRender(r);
    gameuiRender(r);
}

void gameInit() {
    worldInit();
    treesInit();
    ZOOM = 4.0f;
    playerInit();
    npcInit();
    enemyInit();
    tickF_add(gameTick);
    renderF_add(gameRender);
}
