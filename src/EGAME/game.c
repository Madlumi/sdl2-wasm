#include "game.h"
#include "../MENGINE/renderer.h"
#include "../MENGINE/tick.h"
#include "../MENGINE/res.h"
#include "../MENGINE/keys.h"
#include <SDL.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

// Simple Zelda-like tile adventure prototype.
// Explore a 2D tile map with collision and camera follow.

enum TileType {
    TILE_GRASS = 0,
    TILE_SAND,
    TILE_WATER,
    TILE_WALL,
    TILE_TOTAL
};

typedef struct {
    const char *texture;
    SDL_Color tint;
    int solid;
} TileInfo;

static const TileInfo TILE_INFO[TILE_TOTAL] = {
    [TILE_GRASS] = {"sand", {60, 200, 80, 255}, 0},
    [TILE_SAND]  = {"sand", {255, 255, 255, 255}, 0},
    [TILE_WATER] = {"water", {255, 255, 255, 255}, 1},
    [TILE_WALL]  = {"sand", {120, 120, 120, 255}, 1},
};

static int tileW = 0;
static int tileH = 0;

#define MAP_W 29
#define MAP_H 24

// Legend: "." grass, "," sand, "~" water (solid), "#" wall (solid)
static const char *WORLD_LAYOUT[MAP_H] = {
    "~~~~~~~~~~~~~~~~~~~~~~~~~~~~",
    "~.............,,,,,,,,,,,,,~",
    "~..######.....,~~~~~~~~~~~,.~",
    "~..#....#.....,~.........#,.~",
    "~..#.~~.#.....,~.#####...#,.~",
    "~..#....#.....,~.#...#...#,.~",
    "~..######..,,,~.#...#...#,.~",
    "~........,,,,~.#...#...#,.~",
    "~,,,,,,,,,,,,~.#...#...#,.~",
    "~,,,,,,,,,,,,~.#...#####,.~",
    "~,,,,,,,,,,,,~.#.........~",
    "~,,,,,,,,,,,,~.#########.~",
    "~,,,,,,,,,,,,~...........~",
    "~,,,,,,,,,,,,~~~~~~~~~~~~~",
    "~,,,,,,,,,,,,,...........~",
    "~,,,,,,,,,,,,,.,,,,,,,,,,~",
    "~,,,,,~~~~~~~~~.,,,,,,,,,~",
    "~,,,~~~~~~~~~~#.,,,,,,,,,~",
    "~,,,~......~~##.,,,,,,,,,~",
    "~,,,~.####.~~##.,,,,,,,,,~",
    "~,,,~......~~##.,,,,,,,,,~",
    "~,,,~~~~~~~~~~##,,,,,,,,,~",
    "~.......................~",
    "~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
};

static unsigned char world[MAP_W * MAP_H];

typedef struct {
    float x;
    float y;
    float speed;
    float halfW;
    float halfH;
} Player;

static Player player;

static inline float clampf(float v, float min, float max) {
    return v < min ? min : (v > max ? max : v);
}

static unsigned char tileFromChar(char c) {
    switch (c) {
        case '.': return TILE_GRASS;
        case ',': return TILE_SAND;
        case '~': return TILE_WATER;
        case '#': return TILE_WALL;
        default:  return TILE_GRASS;
    }
}

static int indexFor(int tx, int ty) { return ty * MAP_W + tx; }

static int tileSolid(int tx, int ty) {
    if (tx < 0 || tx >= MAP_W || ty < 0 || ty >= MAP_H) return 1;
    return TILE_INFO[world[indexFor(tx, ty)]].solid;
}

static int collidesAt(float cx, float cy) {
    float left = cx - player.halfW;
    float right = cx + player.halfW - 1;
    float top = cy - player.halfH;
    float bottom = cy + player.halfH - 1;

    int startX = (int)floorf(left / tileW);
    int endX = (int)floorf(right / tileW);
    int startY = (int)floorf(top / tileH);
    int endY = (int)floorf(bottom / tileH);

    for (int ty = startY; ty <= endY; ty++) {
        for (int tx = startX; tx <= endX; tx++) {
            if (tileSolid(tx, ty)) {
                return 1;
            }
        }
    }
    return 0;
}

static void movePlayer(float dx, float dy) {
    float newX = player.x + dx;
    if (!collidesAt(newX, player.y)) {
        player.x = newX;
    }

    float newY = player.y + dy;
    if (!collidesAt(player.x, newY)) {
        player.y = newY;
    }
}

static void updateCamera(void) {
    float viewW = WINW / ZOOM;
    float viewH = WINH / ZOOM;
    float worldW = MAP_W * tileW;
    float worldH = MAP_H * tileH;

    XOFF = player.x - viewW * 0.5f;
    YOFF = player.y - viewH * 0.5f;

    XOFF = clampf(XOFF, 0, worldW - viewW);
    YOFF = clampf(YOFF, 0, worldH - viewH);
}

static void handleMovement(double dt) {
    float dirX = 0.0f;
    float dirY = 0.0f;

    if (Held(INP_W)) dirY -= 1.0f;
    if (Held(INP_S)) dirY += 1.0f;
    if (Held(INP_A)) dirX -= 1.0f;
    if (Held(INP_D)) dirX += 1.0f;

    float len = sqrtf(dirX * dirX + dirY * dirY);
    if (len > 0.0f) {
        dirX /= len;
        dirY /= len;

        float moveX = dirX * player.speed * (float)dt;
        float moveY = dirY * player.speed * (float)dt;
        movePlayer(moveX, moveY);
    }
}

static SDL_Rect tileScreenRect(int tx, int ty) {
    float fx = ((float)tx * tileW - XOFF) * ZOOM;
    float fy = ((float)ty * tileH - YOFF) * ZOOM;
    float fx2 = ((float)(tx + 1) * tileW - XOFF) * ZOOM;
    float fy2 = ((float)(ty + 1) * tileH - YOFF) * ZOOM;

    int sx  = (int)floorf(fx);
    int sy  = (int)floorf(fy);
    int sx2 = (int)ceilf(fx2);
    int sy2 = (int)ceilf(fy2);

    SDL_Rect r;
    r.x = sx;
    r.y = sy;
    r.w = sx2 - sx;
    r.h = sy2 - sy;
    return r;
}

static void renderWorld(SDL_Renderer *r) {
    float viewWidth = WINW / ZOOM;
    float viewHeight = WINH / ZOOM;

    float worldLeft = XOFF;
    float worldRight = XOFF + viewWidth;
    float worldTop = YOFF;
    float worldBottom = YOFF + viewHeight;

    int startTileX = (int)fmaxf(0.0f, floorf(worldLeft / tileW));
    int endTileX = (int)fminf((float)MAP_W, ceilf(worldRight / tileW));
    int startTileY = (int)fmaxf(0.0f, floorf(worldTop / tileH));
    int endTileY = (int)fminf((float)MAP_H, ceilf(worldBottom / tileH));

    for (int ty = startTileY; ty < endTileY; ty++) {
        for (int tx = startTileX; tx < endTileX; tx++) {
            unsigned char t = world[indexFor(tx, ty)];
            const TileInfo *info = &TILE_INFO[t];

            SDL_Texture *tex = resGetTexture(info->texture);
            if (!tex) continue;

            SDL_SetTextureColorMod(tex, info->tint.r, info->tint.g, info->tint.b);
            SDL_Rect dst = tileScreenRect(tx, ty);
            SDL_RenderCopy(r, tex, NULL, &dst);
            SDL_SetTextureColorMod(tex, 255, 255, 255);
        }
    }
}

static void renderPlayer(SDL_Renderer *r) {
    (void)r;
    SDL_Color c = {255, 240, 120, 255};
    int px = (int)(player.x - player.halfW);
    int py = (int)(player.y - player.halfH);
    int pw = (int)(player.halfW * 2);
    int ph = (int)(player.halfH * 2);
    drawRect(px, py, pw, ph, ANCHOR_NONE, c);
}

static void renderUI(SDL_Renderer *r) {
    (void)r;
    SDL_Color white = {255, 255, 255, 255};
    drawText("default_font", 10, 10, ANCHOR_TOP_L, white,
             "WASD to move. Avoid water and walls.");
    drawText("default_font", WINW - 10, 10, ANCHOR_TOP_R, white,
             "Zoom %.1fx", ZOOM);
}

static void gameTick(double dt) {
    handleMovement(dt);
    updateCamera();
}

static void gameRender(SDL_Renderer *r) {
    renderWorld(r);
    renderPlayer(r);
    renderUI(r);
}

static void buildWorld(void) {
    for (int y = 0; y < MAP_H; y++) {
        size_t len = strlen(WORLD_LAYOUT[y]);
        for (int x = 0; x < MAP_W; x++) {
            char c = (x < (int)len) ? WORLD_LAYOUT[y][x] : '~';
            world[indexFor(x, y)] = tileFromChar(c);
        }
    }
}

static void initPlayer(void) {
    player.speed = 140.0f;
    player.halfW = tileW * 0.3f;
    player.halfH = tileH * 0.3f;

    // Find a safe spawn away from walls.
    for (int y = 1; y < MAP_H - 1; y++) {
        for (int x = 1; x < MAP_W - 1; x++) {
            if (!tileSolid(x, y)) {
                player.x = x * tileW + tileW * 0.5f;
                player.y = y * tileH + tileH * 0.5f;
                return;
            }
        }
    }
}

void gameInit() {
    SDL_Texture *baseTex = resGetTexture("sand");
    SDL_QueryTexture(baseTex, NULL, NULL, &tileW, &tileH);

    buildWorld();
    initPlayer();

    ZOOM = 1.5f;
    updateCamera();

    tickF_add(gameTick);
    renderF_add(gameRender);
}

