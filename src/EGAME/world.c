#include "world.h"
#include "worldgen.h"
#include "../MENGINE/renderer.h"
#include "../MENGINE/res.h"
#include <math.h>

enum TileType { TILE_GRASS, TILE_SAND, TILE_WATER, TILE_WALL, TILE_TOTAL };

typedef struct { const char *texture; SDL_Color tint; int solid; } TileInfo;
static const TileInfo TILE_INFO[TILE_TOTAL] = {
    [TILE_GRASS] = {"sand", {60, 200, 80, 255}, 0},
    [TILE_SAND]  = {"sand", {255, 255, 255, 255}, 0},
    [TILE_WATER] = {"water", {255, 255, 255, 255}, 1},
    [TILE_WALL]  = {"sand", {120, 120, 120, 255}, 1},
};

#define MAP_W 29
#define MAP_H 24

static const char *WORLD_LAYOUT[MAP_H] = {
    "~~~~~~~~~~~~~~~~~~~~~~~~~~~~", "~.............,,,,,,,,,,,,,~",
    "~..######.....,~~~~~~~~~~~,.~", "~..#....#.....,~.........#,.~",
    "~..#.~~.#.....,~.#####...#,.~", "~..#....#.....,~.#...#...#,.~",
    "~..######..,,,~.#...#...#,.~", "~........,,,,~.#...#...#,.~",
    "~,,,,,,,,,,,,~.#...#...#,.~", "~,,,,,,,,,,,,~.#...#####,.~",
    "~,,,,,,,,,,,,~.#.........~", "~,,,,,,,,,,,,~.#########.~",
    "~,,,,,,,,,,,,~...........~", "~,,,,,,,,,,,,~~~~~~~~~~~~~",
    "~,,,,,,,,,,,,,...........~", "~,,,,,,,,,,,,,.,,,,,,,,,,~",
    "~,,,,,~~~~~~~~~.,,,,,,,,,~", "~,,,~~~~~~~~~~#.,,,,,,,,,~",
    "~,,,~......~~##.,,,,,,,,,~", "~,,,~.####.~~##.,,,,,,,,,~",
    "~,,,~......~~##.,,,,,,,,,~", "~,,,~~~~~~~~~~##,,,,,,,,,~",
    "~.......................~", "~~~~~~~~~~~~~~~~~~~~~~~~~~~~" };

static unsigned char world[MAP_W * MAP_H];
static int tileW = 0, tileH = 0;

int worldMapWidth(void) { return MAP_W; }
int worldMapHeight(void) { return MAP_H; }
int worldTileWidth(void) { return tileW; }
int worldTileHeight(void) { return tileH; }
float worldPixelWidth(void) { return MAP_W * tileW; }
float worldPixelHeight(void) { return MAP_H * tileH; }

static int indexFor(int tx, int ty) { return ty * MAP_W + tx; }

int worldTileSolid(int tx, int ty) {
    if (tx < 0 || tx >= MAP_W || ty < 0 || ty >= MAP_H) return 1;
    return TILE_INFO[world[indexFor(tx, ty)]].solid;
}

static SDL_Rect tileScreenRect(int tx, int ty) {
    float fx = ((float)tx * tileW - XOFF) * ZOOM;
    float fy = ((float)ty * tileH - YOFF) * ZOOM;
    float fx2 = ((float)(tx + 1) * tileW - XOFF) * ZOOM;
    float fy2 = ((float)(ty + 1) * tileH - YOFF) * ZOOM;

    SDL_Rect r;
    r.x = (int)floorf(fx);
    r.y = (int)floorf(fy);
    r.w = (int)ceilf(fx2) - r.x;
    r.h = (int)ceilf(fy2) - r.y;
    return r;
}

void worldInit(void) {
    SDL_Texture *baseTex = resGetTexture("sand");
    SDL_QueryTexture(baseTex, NULL, NULL, &tileW, &tileH);
    worldgenBuild(world, MAP_W, MAP_H, WORLD_LAYOUT);
}

void worldTick(double dt) { (void)dt; }

void worldRender(SDL_Renderer *r) {
    float viewW = WINW / ZOOM;
    float viewH = WINH / ZOOM;
    int startX = (int)fmaxf(0.0f, floorf(XOFF / tileW));
    int endX = (int)fminf((float)MAP_W, ceilf((XOFF + viewW) / tileW));
    int startY = (int)fmaxf(0.0f, floorf(YOFF / tileH));
    int endY = (int)fminf((float)MAP_H, ceilf((YOFF + viewH) / tileH));

    for (int ty = startY; ty < endY; ty++) {
        for (int tx = startX; tx < endX; tx++) {
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
