#define FNL_IMPL
#include "world.h"
#include "worldgen.h"
#include "../MENGINE/renderer.h"
#include "../MENGINE/res.h"
#include "../lib/FastNoiseLite.h"
#include <math.h>

enum TileType {
    TILE_GRASS,
    TILE_SAND,
    TILE_WATER,
    TILE_WALL,
    TILE_WOOD,
    TILE_TOTAL
};

typedef struct {
    SDL_Color baseColor;
    SDL_Color accentColor;
    float frequency;
    Uint32 seed;
    int solid;
} TileInfo;

static const TileInfo TILE_INFO[TILE_TOTAL] = {
    [TILE_GRASS] = {{70, 190, 85, 255}, {40, 140, 60, 255}, 0.6f, 1337, 0},
    [TILE_SAND]  = {{230, 220, 170, 255}, {255, 255, 210, 255}, 0.55f, 4242, 0},
    [TILE_WATER] = {{40, 100, 180, 255}, {15, 60, 130, 255}, 0.4f, 7777, 1},
    [TILE_WALL]  = {{110, 110, 120, 255}, {70, 70, 80, 255}, 0.8f, 9876, 1},
    [TILE_WOOD]  = {{150, 110, 70, 255}, {180, 140, 90, 255}, 0.3f, 2468, 1},
};

static SDL_Texture *tileTextures[TILE_TOTAL];
static const int TILE_SIZE = 8;

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
static SDL_Texture *createNoiseTile(const TileInfo *info);

int worldMapWidth(void) { return MAP_W; }
int worldMapHeight(void) { return MAP_H; }
int worldTileWidth(void) { return tileW; }
int worldTileHeight(void) { return tileH; }
float worldPixelWidth(void) { return MAP_W * tileW; }
float worldPixelHeight(void) { return MAP_H * tileH; }

static int indexFor(int tx, int ty) { return ty * MAP_W + tx; }

static Uint8 clampU8(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (Uint8)v;
}

static Uint8 lerpU8(Uint8 a, Uint8 b, float t) {
    float v = a + (b - a) * t;
    return clampU8((int)lroundf(v));
}

static SDL_Texture *createNoiseTile(const TileInfo *info) {
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, TILE_SIZE, TILE_SIZE, 32, SDL_PIXELFORMAT_RGBA32);
    if (!surface) return NULL;

    if (SDL_LockSurface(surface) != 0) {
        SDL_FreeSurface(surface);
        return NULL;
    }

    fnl_state state = fnlCreateState();
    state.noise_type = FNL_NOISE_OPENSIMPLEX2;
    state.frequency = info->frequency;
    state.seed = info->seed;

    for (int y = 0; y < TILE_SIZE; y++) {
        for (int x = 0; x < TILE_SIZE; x++) {
            float n = fnlGetNoise2D(&state, (float)x, (float)y);
            float t = fmaxf(0.0f, fminf(1.0f, (n + 1.0f) * 0.5f));
            float mix = 0.25f + t * 0.75f;

            Uint8 r = lerpU8(info->baseColor.r, info->accentColor.r, mix);
            Uint8 g = lerpU8(info->baseColor.g, info->accentColor.g, mix);
            Uint8 b = lerpU8(info->baseColor.b, info->accentColor.b, mix);

            Uint32 pixel = SDL_MapRGBA(surface->format, r, g, b, 255);
            ((Uint32 *)surface->pixels)[y * TILE_SIZE + x] = pixel;
        }
    }

    SDL_UnlockSurface(surface);
    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    return tex;
}

int worldTileSolid(int tx, int ty) {
    if (tx < 0 || tx >= MAP_W || ty < 0 || ty >= MAP_H) return 1;
    return TILE_INFO[world[indexFor(tx, ty)]].solid;
}

int worldTileIsSand(int tx, int ty) {
    if (tx < 0 || tx >= MAP_W || ty < 0 || ty >= MAP_H) return 0;
    return world[indexFor(tx, ty)] == TILE_SAND;
}

int worldPlaceWoodTile(int tx, int ty) {
    if (tx < 0 || tx >= MAP_W || ty < 0 || ty >= MAP_H) return 0;

    unsigned char *tile = &world[indexFor(tx, ty)];
    if (TILE_INFO[*tile].solid) return 0;

    *tile = TILE_WOOD;
    return 1;
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
    tileW = TILE_SIZE;
    tileH = TILE_SIZE;
    for (int i = 0; i < TILE_TOTAL; i++) {
        tileTextures[i] = createNoiseTile(&TILE_INFO[i]);
    }
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
            SDL_Texture *tex = tileTextures[t];
            if (!tex) continue;
            SDL_Rect dst = tileScreenRect(tx, ty);
            SDL_RenderCopy(r, tex, NULL, &dst);
        }
    }
}
