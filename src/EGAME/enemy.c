#include "enemy.h"
#include "entity.h"
#include "player.h"
#include "world.h"
#include "../MENGINE/renderer.h"
#include <math.h>

static Entity enemy;
static float chaseSpeed = 80.0f;

static void positionEnemy(void) {
    enemy.halfW = worldTileWidth() * 0.25f;
    enemy.halfH = worldTileHeight() * 0.25f;
    enemy.x = worldPixelWidth() * 0.8f;
    enemy.y = worldPixelHeight() * 0.8f;
}

void enemyInit(void) { positionEnemy(); }

void enemyTick(double dt) {
    const Player *p = playerGet();
    float dirX = p->body.x - enemy.x;
    float dirY = p->body.y - enemy.y;
    float len = sqrtf(dirX * dirX + dirY * dirY);
    if (len > 1.0f) {
        dirX /= len;
        dirY /= len;
        entityMove(&enemy, dirX * chaseSpeed * (float)dt,
                   dirY * chaseSpeed * (float)dt);
    }
}

void enemyRender(SDL_Renderer *r) {
    (void)r;
    SDL_Color c = {220, 80, 80, 255};
    int px = (int)(enemy.x - enemy.halfW);
    int py = (int)(enemy.y - enemy.halfH);
    int pw = (int)(enemy.halfW * 2);
    int ph = (int)(enemy.halfH * 2);
    drawRect(px, py, pw, ph, ANCHOR_NONE, c);
}
