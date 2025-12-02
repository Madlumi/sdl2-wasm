#include "player.h"
#include "world.h"
#include "../MENGINE/keys.h"
#include "../MENGINE/renderer.h"
#include "../MENGINE/res.h"
#include <math.h>

static Player player;

static float clampf(float v, float min, float max) {
    return v < min ? min : (v > max ? max : v);
}

const Player *playerGet(void) { return &player; }

static void updateCamera(void) {
    float viewW = WINW / ZOOM;
    float viewH = WINH / ZOOM;
    XOFF = player.body.x - viewW * 0.5f;
    YOFF = player.body.y - viewH * 0.5f;
    XOFF = clampf(XOFF, 0, worldPixelWidth() - viewW);
    YOFF = clampf(YOFF, 0, worldPixelHeight() - viewH);
}

static void handleMovement(double dt) {
    float dirX = 0.0f, dirY = 0.0f;
    if (Held(INP_W)) dirY -= 1.0f;
    if (Held(INP_S)) dirY += 1.0f;
    if (Held(INP_A)) dirX -= 1.0f;
    if (Held(INP_D)) dirX += 1.0f;

    float len = sqrtf(dirX * dirX + dirY * dirY);
    if (len > 0.0f) {
        dirX /= len;
        dirY /= len;
        float stepX = dirX * player.speed * (float)dt;
        float stepY = dirY * player.speed * (float)dt;
        int blocked = entityMove(&player.body, stepX, stepY);
        if (blocked && !player.wallBlocked) {
            Mix_PlayChannel(-1, resGetSound("bump"), 0);
            player.wallBlocked = 1;
        } else if (!blocked) {
            player.wallBlocked = 0;
        }
    } else {
        player.wallBlocked = 0;
    }
}

static void findSpawn(void) {
    for (int y = 1; y < worldMapHeight() - 1; y++) {
        for (int x = 1; x < worldMapWidth() - 1; x++) {
            if (!worldTileSolid(x, y)) {
                player.body.x = x * worldTileWidth() + worldTileWidth() * 0.5f;
                player.body.y = y * worldTileHeight() + worldTileHeight() * 0.5f;
                return;
            }
        }
    }
}

void playerInit(void) {
    player.speed = 140.0f;
    player.body.halfW = worldTileWidth() * 0.3f;
    player.body.halfH = worldTileHeight() * 0.3f;
    player.wallBlocked = 0;
    findSpawn();
    updateCamera();
}

void playerTick(double dt) {
    handleMovement(dt);
    updateCamera();
}

void playerRender(SDL_Renderer *r) {
    (void)r;
    SDL_Color c = {255, 240, 120, 255};
    int px = (int)(player.body.x - player.body.halfW);
    int py = (int)(player.body.y - player.body.halfH);
    int pw = (int)(player.body.halfW * 2);
    int ph = (int)(player.body.halfH * 2);
    drawRect(px, py, pw, ph, ANCHOR_NONE, c);
}
