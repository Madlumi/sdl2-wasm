#include "game.h"
#include "player.h"
#include "enemy.h"
#include "npc.h"
#include "world.h"
#include "gameui.h"
#include "../MENGINE/renderer.h"
#include "../MENGINE/tick.h"

static void gameTick(double dt) {
    worldTick(dt);
    playerTick(dt);
    npcTick(dt);
    enemyTick(dt);
}

static void gameRender(SDL_Renderer *r) {
    worldRender(r);
    playerRender(r);
    npcRender(r);
    enemyRender(r);
    gameuiRender(r);
}

void gameInit() {
    worldInit();
    ZOOM = 1.5f;
    playerInit();
    npcInit();
    enemyInit();
    tickF_add(gameTick);
    renderF_add(gameRender);
}
