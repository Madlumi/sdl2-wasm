
#include "wolf3d.h"
#include "../MENGINE/renderer.h"

#define FNL_IMPL
#include "../lib/FastNoiseLite.h"
#include "render3d.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// -------------------------------------------------------------
// Shared map config
// -------------------------------------------------------------

#define MAP_W 50
#define MAP_H 50

static const float TILE_SIZE   = 1.0f;
static const float WALL_HEIGHT = 0.1f;
static const int   GRASS_TEX_SIZE = 32;

// Big height jump treated as a wall
static const int WALL_DIFF_THRESHOLD = 4;

// -------------------------------------------------------------
// Global geometry buffers (used from wolf3d.c via extern)
// -------------------------------------------------------------

MeshInstance faces[MAP_W * MAP_H * 6];
int faceCount = 0;

MeshInstance floorFaces[MAP_W * MAP_H];
int floorCount = 0;

// -------------------------------------------------------------
// Heightmap + noise
// -------------------------------------------------------------

static int LEVEL[MAP_H][MAP_W];
static fnl_state gNoise;
static SDL_Texture *gGrassTexture = NULL;

static inline int iabs_int(int v) { return v < 0 ? -v : v; }

static inline int imax3(int a, int b, int c) {
    int m = a > b ? a : b;
    return m > c ? m : c;
}
static inline int imax4(int a, int b, int c, int d) {
    return imax3(a, b, c) > d ? imax3(a, b, c) : d;
}
static inline int imin4(int a, int b, int c, int d) {
    int m = a < b ? a : b;
    if (c < m) m = c;
    if (d < m) m = d;
    return m;
}

static SDL_Texture *createGrassTexture(void) {
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, GRASS_TEX_SIZE, GRASS_TEX_SIZE, 32, SDL_PIXELFORMAT_RGBA32);
    if (!surface) {
        printf("Failed to create grass surface: %s\n", SDL_GetError());
        return NULL;
    }

    fnl_state coarse = fnlCreateState();
    coarse.noise_type = FNL_NOISE_OPENSIMPLEX2;
    coarse.frequency = 2.2f;
    coarse.seed = gNoise.seed + 7;

    fnl_state detail = fnlCreateState();
    detail.noise_type = FNL_NOISE_PERLIN;
    detail.frequency = 6.5f;
    detail.seed = gNoise.seed + 31;

    const SDL_Color palette[4] = {
        {36, 90, 28, 255},
        {62, 118, 44, 255},
        {88, 146, 68, 255},
        {128, 174, 98, 255},
    };

    SDL_LockSurface(surface);
    Uint32 *pixels = (Uint32 *)surface->pixels;
    for (int y = 0; y < GRASS_TEX_SIZE; y++) {
        for (int x = 0; x < GRASS_TEX_SIZE; x++) {
            float nx = (float)x / (float)GRASS_TEX_SIZE;
            float ny = (float)y / (float)GRASS_TEX_SIZE;

            float large = fnlGetNoise2D(&coarse, nx * GRASS_TEX_SIZE, ny * GRASS_TEX_SIZE);
            float fine  = fnlGetNoise2D(&detail, nx * GRASS_TEX_SIZE, ny * GRASS_TEX_SIZE);
            float value = (large * 0.75f + fine * 0.25f + 1.0f) * 0.5f;

            int idx = (int)floorf(value * 4.0f);
            if (idx < 0) idx = 0;
            if (idx > 3) idx = 3;

            SDL_Color c = palette[idx];
            pixels[y * GRASS_TEX_SIZE + x] = SDL_MapRGBA(surface->format, c.r, c.g, c.b, c.a);
        }
    }
    SDL_UnlockSurface(surface);

    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surface);
    if (!tex) {
        printf("Failed to create grass texture: %s\n", SDL_GetError());
    } else {
        SDL_SetTextureScaleMode(tex, SDL_ScaleModeNearest);
    }

    SDL_FreeSurface(surface);
    return tex;
}

// -------------------------------------------------------------
// Height helpers exported to wolf3d.c
// -------------------------------------------------------------

int tileHeight(int x, int z) {
    if (x < 0 || z < 0 || x >= MAP_W || z >= MAP_H)
        return 9;  // big solid outside
    return LEVEL[z][x];
}

// sample continuous terrain height at world-space position (x,z)
float sampleHeightAt(float wx, float wz) {
    float gx = wx / TILE_SIZE;
    float gz = wz / TILE_SIZE;

    int x0 = (int)floorf(gx);
    int z0 = (int)floorf(gz);

    // clamp to valid cell range [0 .. MAP_W-2] / [0 .. MAP_H-2]
    if (x0 < 0) x0 = 0;
    if (z0 < 0) z0 = 0;
    if (x0 > MAP_W - 2) x0 = MAP_W - 2;
    if (z0 > MAP_H - 2) z0 = MAP_H - 2;

    int x1 = x0 + 1;
    int z1 = z0 + 1;

    float tx = gx - (float)x0;
    float tz = gz - (float)z0;

    int h00 = tileHeight(x0, z0);
    int h10 = tileHeight(x1, z0);
    int h01 = tileHeight(x0, z1);
    int h11 = tileHeight(x1, z1);

    // If this cell spans a big cliff, do NOT smooth across it.
    int d0 = iabs_int(h00 - h10);
    int d1 = iabs_int(h10 - h11);
    int d2 = iabs_int(h11 - h01);
    int d3 = iabs_int(h01 - h00);
    int maxDiff = imax4(d0, d1, d2, d3);

    if (maxDiff >= WALL_DIFF_THRESHOLD) {
        // treat as local tile plateau
        return h00 * WALL_HEIGHT;
    }

    float y00 = h00 * WALL_HEIGHT;
    float y10 = h10 * WALL_HEIGHT;
    float y01 = h01 * WALL_HEIGHT;
    float y11 = h11 * WALL_HEIGHT;

    // bilinear interpolation for smooth slopes
    float y0 = y00 + (y10 - y00) * tx;
    float y1 = y01 + (y11 - y01) * tx;
    return y0 + (y1 - y0) * tz;
}

// -------------------------------------------------------------
// Level generation
// -------------------------------------------------------------

static void generateLevel(void) {
    int cx  = MAP_W / 2;      // center hill
    int cz  = MAP_H / 2;

    int cx2 = MAP_W / 4;      // side hill
    int cz2 = MAP_H / 3;

    for (int z = 0; z < MAP_H; z++) {
        for (int x = 0; x < MAP_W; x++) {

            // outer border wall (solid, ignore noise)
            if (x == 0 || z == 0 || x == MAP_W - 1 || z == MAP_H - 1) {
                LEVEL[z][x] = 4;
                continue;
            }

            int h = 0;

            // central hill
            {
                int dx = iabs_int(x - cx);
                int dz = iabs_int(z - cz);
                int d  = dx > dz ? dx : dz;

                if      (d <= 2)           h = 3;
                else if (d <= 4 && h < 2)  h = 2;
                else if (d <= 6 && h < 1)  h = 1;
            }

            // smaller side hill
            {
                int dx2 = iabs_int(x - cx2);
                int dz2 = iabs_int(z - cz2);
                int d2  = dx2 > dz2 ? dx2 : dz2;

                if      (d2 <= 1 && h < 2) h = 2;
                else if (d2 <= 3 && h < 1) h = 1;
            }

            // pit test area
            if (x >= 25 && x <= 28 && z >= 10 && z <= 13) {
                LEVEL[z][x] = -1;
                continue;
            }

            // 2-step cliff test #1
            if (x >= 10 && x <= 20) {
                if (z == 30) h = 0;
                if (z == 31) h = 2;
            }

            // 2-step cliff test #2
            if (z >= 5 && z <= 10) {
                if (x == 34) h = 1;
                if (x == 35) h = 3;
            }

            // Perlin noise modulation
            {
                float n = fnlGetNoise2D(&gNoise, (float)x, (float)z); // [-1,1]
                int bump = (int)roundf(n * 2.0f); // mostly -2..+2
                h += bump;
            }

            if (h > 4)  h = 4;
            if (h < -2) h = -2;

            LEVEL[z][x] = h;
        }
    }
}

// -------------------------------------------------------------
// Geometry build: sloped terrain + walls where diff >= 4
// -------------------------------------------------------------

static void buildLevelGeometry(void) {
    faceCount = 0;

    SDL_Color terrainColor = gGrassTexture ?
        (SDL_Color){255, 255, 255, 255} :
        (SDL_Color){180, 180, 200, 255};
    SDL_Color wallColorPos = (SDL_Color){220, 220, 240, 255};
    SDL_Color wallColorNeg = (SDL_Color){140, 140, 170, 255};

    // 1) heightfield terrain as quads with per-corner heights
    for (int z = 0; z < MAP_H - 1; z++) {
        for (int x = 0; x < MAP_W - 1; x++) {
            if (faceCount >= (int)(sizeof(faces)/sizeof(faces[0]))) break;

            int h00 = tileHeight(x,     z);
            int h10 = tileHeight(x + 1, z);
            int h01 = tileHeight(x,     z + 1);
            int h11 = tileHeight(x + 1, z + 1);

            int d0 = iabs_int(h00 - h10);
            int d1 = iabs_int(h10 - h11);
            int d2 = iabs_int(h11 - h01);
            int d3 = iabs_int(h01 - h00);
            int maxDiff = imax4(d0, d1, d2, d3);

            float y00, y10, y01, y11;

            if (maxDiff >= WALL_DIFF_THRESHOLD) {
                // Big cliff crosses this cell: flatten to the lower side
                int hMin = imin4(h00, h10, h01, h11);
                y00 = y10 = y01 = y11 = hMin * WALL_HEIGHT;
            } else {
                // Normal smooth slope
                y00 = h00 * WALL_HEIGHT;
                y10 = h10 * WALL_HEIGHT;
                y01 = h01 * WALL_HEIGHT;
                y11 = h11 * WALL_HEIGHT;
            }

            float fx = x * TILE_SIZE;
            float fz = z * TILE_SIZE;

            SDL_FPoint uv0 = {0.0f, 0.0f};
            SDL_FPoint uv1 = {1.0f, 0.0f};
            SDL_FPoint uv2 = {1.0f, 1.0f};
            SDL_FPoint uv3 = {0.0f, 1.0f};

            render3dInitQuadMeshUV(&faces[faceCount++],
                v3(fx,            y00, fz),
                v3(fx+TILE_SIZE,  y10, fz),
                v3(fx+TILE_SIZE,  y11, fz+TILE_SIZE),
                v3(fx,            y01, fz+TILE_SIZE),
                terrainColor,
                uv0, uv1, uv2, uv3);

            if (faceCount > 0) {
                faces[faceCount - 1].mesh.texture = gGrassTexture;
            }
        }
    }

    // 2) vertical walls where height diff >= 4 between neighbors

    // horizontal edges between (x,z) and (x+1,z)
    for (int z = 0; z < MAP_H; z++) {
        for (int x = 0; x < MAP_W - 1; x++) {
            if (faceCount >= (int)(sizeof(faces)/sizeof(faces[0]))) break;

            int hA = tileHeight(x,     z);
            int hB = tileHeight(x + 1, z);
            int diff = hB - hA;
            if (iabs_int(diff) < WALL_DIFF_THRESHOLD) continue;

            float fx = (x + 1) * TILE_SIZE;   // edge at x+1
            float fz = z * TILE_SIZE;

            float yLow  = (diff > 0 ? hA : hB) * WALL_HEIGHT;
            float yHigh = (diff > 0 ? hB : hA) * WALL_HEIGHT;

            SDL_Color col = diff > 0 ? wallColorPos : wallColorNeg;

            // vertical wall quad along z
            render3dInitQuadMesh(&faces[faceCount++],
                v3(fx, yLow,  fz),
                v3(fx, yLow,  fz+TILE_SIZE),
                v3(fx, yHigh, fz+TILE_SIZE),
                v3(fx, yHigh, fz),
                col);
        }
    }

    // vertical edges between (x,z) and (x,z+1)
    for (int z = 0; z < MAP_H - 1; z++) {
        for (int x = 0; x < MAP_W; x++) {
            if (faceCount >= (int)(sizeof(faces)/sizeof(faces[0]))) break;

            int hA = tileHeight(x, z);
            int hB = tileHeight(x, z + 1);
            int diff = hB - hA;
            if (iabs_int(diff) < WALL_DIFF_THRESHOLD) continue;

            float fx = x * TILE_SIZE;
            float fz = (z + 1) * TILE_SIZE;  // edge at z+1

            float yLow  = (diff > 0 ? hA : hB) * WALL_HEIGHT;
            float yHigh = (diff > 0 ? hB : hA) * WALL_HEIGHT;

            SDL_Color col = diff > 0 ? wallColorPos : wallColorNeg;

            // vertical wall quad along x
            render3dInitQuadMesh(&faces[faceCount++],
                v3(fx,           yLow,  fz),
                v3(fx+TILE_SIZE, yLow,  fz),
                v3(fx+TILE_SIZE, yHigh, fz),
                v3(fx,           yHigh, fz),
                col);
        }
    }
}

// flat floor at y=0 for now (optional, mostly hidden by terrain)
static void buildFloorGeometry(void) {
    floorCount = 0;

    SDL_Color floorColor = gGrassTexture ?
        (SDL_Color){255, 255, 255, 255} :
        (SDL_Color){70, 90, 110, 255};
    float tileScale = 1.0f;

    for (int z = 0; z < MAP_H; z++) {
        for (int x = 0; x < MAP_W; x++) {
            Vec3 f0 = v3(x * TILE_SIZE,          0, z * TILE_SIZE);
            Vec3 f1 = v3((x + 1) * TILE_SIZE,    0, z * TILE_SIZE);
            Vec3 f2 = v3((x + 1) * TILE_SIZE,    0, (z + 1) * TILE_SIZE);
            Vec3 f3 = v3(x * TILE_SIZE,          0, (z + 1) * TILE_SIZE);

            SDL_FPoint uv0 = {0.0f,      0.0f};
            SDL_FPoint uv1 = {tileScale, 0.0f};
            SDL_FPoint uv2 = {tileScale, tileScale};
            SDL_FPoint uv3 = {0.0f,      tileScale};

            if (floorCount < (int)(sizeof(floorFaces)/sizeof(floorFaces[0]))) {
                render3dInitQuadMeshUV(&floorFaces[floorCount++],
                    f0, f1, f2, f3, floorColor,
                    uv0, uv1, uv2, uv3);

                if (floorCount > 0) {
                    floorFaces[floorCount - 1].mesh.texture = gGrassTexture;
                }
            }
        }
    }
}

// -------------------------------------------------------------
// Level init entry point (called from wolf3dInit)
// -------------------------------------------------------------

void levelInit(void) {
    gNoise = fnlCreateState();
    gNoise.noise_type = FNL_NOISE_PERLIN;
    gNoise.frequency = 0.5f;     // tweak to taste
    gNoise.seed = 1337;

    gGrassTexture = createGrassTexture();

    generateLevel();
    buildLevelGeometry();
    buildFloorGeometry();
}

