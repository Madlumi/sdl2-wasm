#include "player.h"
#include "enemy.h"
#include "trees.h"
#include "world.h"
#include "../MENGINE/keys.h"
#include "../MENGINE/renderer.h"
#include "../MENGINE/res.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>

typedef struct {
    Entity area;
    float timer;
    float lifetime;
    int active;
    int id;
} Slash;

typedef struct {
    int active;
    float timer;
    float duration;
    float riseSpeed;
    float baseOffset;
    char text[64];
} PlayerNotification;

static Player player;
static Slash slash;
static PlayerNotification notification;
static int slashIdCounter = 0;

static float clampf(float v, float min, float max) {
    return v < min ? min : (v > max ? max : v);
}

const Player *playerGet(void) { return &player; }

static void playerNotification(const char *fmt, ...) {
    if (!fmt) return;
    notification.active = 1;
    notification.timer = 0.0f;
    notification.duration = 0.9f;
    notification.riseSpeed = worldTileHeight() * 0.4f;
    notification.baseOffset = worldTileHeight() * 0.2f;

    va_list args;
    va_start(args, fmt);
    vsnprintf(notification.text, sizeof(notification.text), fmt, args);
    va_end(args);
}

static void updateCamera(void) {
    float viewW = WINW / ZOOM;
    float viewH = WINH / ZOOM;
    XOFF = player.body.x - viewW * 0.5f;
    YOFF = player.body.y - viewH * 0.5f;
    float maxXOff = fmaxf(0.0f, worldPixelWidth() - viewW);
    float maxYOff = fmaxf(0.0f, worldPixelHeight() - viewH);
    XOFF = clampf(XOFF, 0, maxXOff);
    YOFF = clampf(YOFF, 0, maxYOff);
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
        player.facingX = dirX;
        player.facingY = dirY;
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
                float left = player.body.x - player.body.halfW;
                float right = player.body.x + player.body.halfW;
                float top = player.body.y - player.body.halfH;
                float bottom = player.body.y + player.body.halfH;
                if (!treesCollidesAt(left, right, top, bottom)) return;
            }
        }
    }
}

void playerInit(void) {
    player.speed = 120.0f;
    player.body.halfW = worldTileWidth() * 0.3f;
    player.body.halfH = worldTileHeight() * 0.3f;
    player.wallBlocked = 0;
    player.facingX = 0.0f;
    player.facingY = 1.0f;
    player.exp = 0;
    player.wood = 0;
    slash.active = 0;
    notification.active = 0;
    findSpawn();
    updateCamera();
}

void playerTick(double dt) {
    handleMovement(dt);
    if (slash.active) {
        slash.timer += (float)dt;
        enemyApplySlash(&slash.area, 6, slash.id);
        treesApplySlash(&slash.area, 6, slash.id);
        if (slash.timer >= slash.lifetime) slash.active = 0;
    }

    if (Pressed(INP_LCLICK)) {
        float dirX = player.facingX;
        float dirY = player.facingY;
        if (dirX == 0.0f && dirY == 0.0f) dirY = 1.0f;

        float range = worldTileWidth() * 0.8f;
        slash.area.halfW = (fabsf(dirX) > fabsf(dirY)) ? range * 0.55f
                                                      : worldTileWidth() * 0.35f;
        slash.area.halfH = (fabsf(dirX) > fabsf(dirY)) ? worldTileHeight() * 0.25f
                                                      : range * 0.55f;

        slash.area.x = player.body.x + dirX * (player.body.halfW + slash.area.halfW);
        slash.area.y = player.body.y + dirY * (player.body.halfH + slash.area.halfH);
        slash.timer = 0.0f;
        slash.lifetime = 0.18f;
        slash.active = 1;
        slash.id = ++slashIdCounter;
        enemyApplySlash(&slash.area, 6, slash.id);
        treesApplySlash(&slash.area, 6, slash.id);
    }

    if (notification.active) {
        notification.timer += (float)dt;
        if (notification.timer >= notification.duration) notification.active = 0;
    }
    updateCamera();
}

void playerRender(SDL_Renderer *r) {
    (void)r;
    SDL_Color c = {255, 240, 120, 255};
    float px = player.body.x - player.body.halfW;
    float py = player.body.y - player.body.halfH;
    float pw = player.body.halfW * 2;
    float ph = player.body.halfH * 2;
    drawWorldRect(px, py, pw, ph, c);

    if (slash.active) {
        SDL_Color slashColor = {220, 220, 255, 180};
        float sx = slash.area.x - slash.area.halfW;
        float sy = slash.area.y - slash.area.halfH;
        float sw = slash.area.halfW * 2;
        float sh = slash.area.halfH * 2;
        drawWorldRect(sx, sy, sw, sh, slashColor);
    }

    if (notification.active) {
        float alphaRatio = 1.0f - (notification.timer / notification.duration);
        if (alphaRatio < 0.0f) alphaRatio = 0.0f;
        SDL_Color textColor = {255, 255, 255, (Uint8)(255 * alphaRatio)};
        float rise = notification.baseOffset + notification.riseSpeed * notification.timer;
        float tx = player.body.x;
        float ty = player.body.y - player.body.halfH - rise;
        drawWorldTextScaled("default_font", tx, ty, ANCHOR_NONE, textColor, 0.25f,
                            "%s", notification.text);
    }
}

void playerAddExp(int amount) {
    if (amount <= 0) return;
    player.exp += amount;
    playerNotification("+%d xp", amount);
}

void playerAddWood(int amount) {
    if (amount <= 0) return;
    player.wood += amount;
    playerNotification("+%d wood", amount);
}

int playerCollidesAt(float left, float right, float top, float bottom,
                     const Entity *ignore) {
    if (&player.body == ignore) return 0;

    float pLeft = player.body.x - player.body.halfW;
    float pRight = player.body.x + player.body.halfW;
    float pTop = player.body.y - player.body.halfH;
    float pBottom = player.body.y + player.body.halfH;

    return left < pRight && right > pLeft && top < pBottom && bottom > pTop;
}

int playerGetWood(void) { return player.wood; }
