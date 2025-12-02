#define FNL_IMPL
#include "level.h"
#include "../MENGINE/renderer.h"
#include "../lib/FastNoiseLite.h"
#include <math.h>
#include <stdio.h>

MeshInstance faces[MAP_W * MAP_H * 6];
int faceCount = 0;

MeshInstance floorFaces[MAP_W * MAP_H];
int floorCount = 0;

static int heightMap[MAP_W][MAP_H];
static SDL_Texture *grassTexture = NULL;

static float heightToWorld(int h) {
    return (float)h * WALL_HEIGHT;
}

static SDL_Texture *createGrassTexture(void) {
    if (!renderer) { return NULL; }

    fnl_state state = fnlCreateState();
    state.seed = 4242;
    state.frequency = 0.35f;
    state.noise_type = FNL_NOISE_OPENSIMPLEX2;
    state.octaves = 2;

    const int texSize = 32;
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, texSize, texSize, 32, SDL_PIXELFORMAT_RGBA32);
    if (!surface) {
        printf("Failed to create grass surface: %s\n", SDL_GetError());
        return NULL;
    }

    SDL_Color palette[4] = {
        {65, 99, 44, 255},
        {80, 124, 52, 255},
        {96, 146, 62, 255},
        {120, 170, 78, 255},
    };

    Uint32 *pixels = (Uint32 *)surface->pixels;
    for (int y = 0; y < texSize; y++) {
        for (int x = 0; x < texSize; x++) {
            float nx = (float)x / (float)texSize * 4.0f;
            float ny = (float)y / (float)texSize * 4.0f;
            float n = fnlGetNoise2D(&state, nx, ny); // -1..1
            float v = (n + 1.0f) * 0.5f;
            int idx = (int)floorf(v * 4.0f);
            if (idx < 0) idx = 0;
            if (idx > 3) idx = 3;
            SDL_Color c = palette[idx];
            pixels[y * texSize + x] = SDL_MapRGBA(surface->format, c.r, c.g, c.b, c.a);
        }
    }

    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    if (!tex) {
        printf("Failed to create grass texture: %s\n", SDL_GetError());
        return NULL;
    }

    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_NONE);
    SDL_SetTextureScaleMode(tex, SDL_ScaleModeNearest);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    return tex;
}

static void generateHeightMap(void) {
    fnl_state state = fnlCreateState();
    state.seed = 9001;
    state.frequency = 0.08f;
    state.noise_type = FNL_NOISE_OPENSIMPLEX2;
    state.fractal_type = FNL_FRACTAL_FBM;
    state.octaves = 4;
    state.gain = 0.45f;

    for (int z = 0; z < MAP_H; z++) {
        for (int x = 0; x < MAP_W; x++) {
            float nx = (float)x / (float)MAP_W;
            float nz = (float)z / (float)MAP_H;
            float radialFalloff = 1.0f - sqrtf((nx - 0.5f) * (nx - 0.5f) + (nz - 0.5f) * (nz - 0.5f));
            if (radialFalloff < 0.0f) radialFalloff = 0.0f;
            float n = fnlGetNoise2D(&state, (float)x, (float)z);
            float h = ((n + 1.0f) * 0.5f * 10.0f + radialFalloff * 6.0f) - 1.0f;
            if (h < 0.0f) h = 0.0f;
            if (h > 12.0f) h = 12.0f;
            heightMap[x][z] = (int)roundf(h);
        }
    }
}

static SDL_Color heightColor(int h) {
    const SDL_Color low = {90, 72, 44, 255};
    const SDL_Color mid = {104, 120, 60, 255};
    const SDL_Color high = {130, 150, 78, 255};

    float t = (float)h / 12.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    SDL_Color a = (t < 0.5f) ? low : mid;
    SDL_Color b = (t < 0.5f) ? mid : high;
    float localT = (t < 0.5f) ? (t * 2.0f) : ((t - 0.5f) * 2.0f);

    SDL_Color out = {
        (Uint8)(a.r + (b.r - a.r) * localT),
        (Uint8)(a.g + (b.g - a.g) * localT),
        (Uint8)(a.b + (b.b - a.b) * localT),
        255
    };
    return out;
}

static void addTopFace(int x, int z) {
    float x0 = (float)x * TILE_SIZE;
    float z0 = (float)z * TILE_SIZE;
    float x1 = x0 + TILE_SIZE;
    float z1 = z0 + TILE_SIZE;
    float y = heightToWorld(heightMap[x][z]);

    MeshInstance *inst = &faces[faceCount++];
    render3dInitQuadMeshUV(inst,
        v3(x0, y, z0),
        v3(x1, y, z0),
        v3(x1, y, z1),
        v3(x0, y, z1),
        (SDL_Color){255, 255, 255, 255},
        (SDL_FPoint){0.0f, 0.0f}, (SDL_FPoint){1.0f, 0.0f}, (SDL_FPoint){1.0f, 1.0f}, (SDL_FPoint){0.0f, 1.0f});
    inst->texture = grassTexture;
}

static void addSideFace(int x, int z, int dx, int dz, int neighborH) {
    int h = heightMap[x][z];
    if (h <= neighborH) return;

    float x0 = (float)x * TILE_SIZE;
    float z0 = (float)z * TILE_SIZE;
    float x1 = x0 + TILE_SIZE;
    float z1 = z0 + TILE_SIZE;

    float yBottom = heightToWorld(neighborH);
    float yTop = heightToWorld(h);

    MeshInstance *inst = &faces[faceCount++];

    if (dz == -1) { // north
        render3dInitQuadMesh(inst, v3(x0, yBottom, z0), v3(x1, yBottom, z0), v3(x1, yTop, z0), v3(x0, yTop, z0), heightColor(h));
    } else if (dz == 1) { // south
        render3dInitQuadMesh(inst, v3(x1, yBottom, z1), v3(x0, yBottom, z1), v3(x0, yTop, z1), v3(x1, yTop, z1), heightColor(h));
    } else if (dx == -1) { // west
        render3dInitQuadMesh(inst, v3(x0, yBottom, z1), v3(x0, yBottom, z0), v3(x0, yTop, z0), v3(x0, yTop, z1), heightColor(h));
    } else if (dx == 1) { // east
        render3dInitQuadMesh(inst, v3(x1, yBottom, z0), v3(x1, yBottom, z1), v3(x1, yTop, z1), v3(x1, yTop, z0), heightColor(h));
    }
}

static void buildGeometry(void) {
    faceCount = 0;
    floorCount = 0;

    for (int z = 0; z < MAP_H; z++) {
        for (int x = 0; x < MAP_W; x++) {
            addTopFace(x, z);

            int northH = (z > 0) ? heightMap[x][z - 1] : 0;
            int southH = (z < MAP_H - 1) ? heightMap[x][z + 1] : 0;
            int westH  = (x > 0) ? heightMap[x - 1][z] : 0;
            int eastH  = (x < MAP_W - 1) ? heightMap[x + 1][z] : 0;

            addSideFace(x, z, 0, -1, northH);
            addSideFace(x, z, 0, 1, southH);
            addSideFace(x, z, -1, 0, westH);
            addSideFace(x, z, 1, 0, eastH);

            MeshInstance *floor = &floorFaces[floorCount++];
            float x0 = (float)x * TILE_SIZE;
            float z0 = (float)z * TILE_SIZE;
            float x1 = x0 + TILE_SIZE;
            float z1 = z0 + TILE_SIZE;
            render3dInitQuadMeshUV(floor,
                v3(x0, 0.0f, z0),
                v3(x1, 0.0f, z0),
                v3(x1, 0.0f, z1),
                v3(x0, 0.0f, z1),
                (SDL_Color){80, 90, 110, 255},
                (SDL_FPoint){0.0f, 0.0f}, (SDL_FPoint){1.0f, 0.0f}, (SDL_FPoint){1.0f, 1.0f}, (SDL_FPoint){0.0f, 1.0f});
            floor->texture = NULL;
        }
    }
}

void levelInit(void) {
    generateHeightMap();
    grassTexture = createGrassTexture();
    buildGeometry();
}

int tileHeight(int x, int z) {
    if (x < 0 || x >= MAP_W || z < 0 || z >= MAP_H) return 0;
    return heightMap[x][z];
}

float sampleHeightAt(float wx, float wz) {
    float tx = wx / TILE_SIZE;
    float tz = wz / TILE_SIZE;

    int x0 = (int)floorf(tx);
    int z0 = (int)floorf(tz);
    int x1 = x0 + 1;
    int z1 = z0 + 1;

    if (x0 < 0) x0 = 0; if (x0 >= MAP_W) x0 = MAP_W - 1;
    if (z0 < 0) z0 = 0; if (z0 >= MAP_H) z0 = MAP_H - 1;
    if (x1 < 0) x1 = 0; if (x1 >= MAP_W) x1 = MAP_W - 1;
    if (z1 < 0) z1 = 0; if (z1 >= MAP_H) z1 = MAP_H - 1;

    float fx = tx - (float)x0;
    float fz = tz - (float)z0;

    float h00 = heightToWorld(heightMap[x0][z0]);
    float h10 = heightToWorld(heightMap[x1][z0]);
    float h01 = heightToWorld(heightMap[x0][z1]);
    float h11 = heightToWorld(heightMap[x1][z1]);

    float hx0 = h00 + (h10 - h00) * fx;
    float hx1 = h01 + (h11 - h01) * fx;
    float h = hx0 + (hx1 - hx0) * fz;
    return h;
}

SDL_Texture *levelGetGrassTexture(void) {
    return grassTexture;
}
