#include "game.h"
#include "../MENGINE/renderer.h"
#include "../MENGINE/tick.h"
#include "../MENGINE/res.h"
#include "../MENGINE/keys.h"
#define FNL_IMPL
#include "../lib/FastNoiseLite.h"
#undef FNL_IMPL

#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define GRID_RESOLUTION 64
#define PLANE_EXTENT 8.0f

typedef struct {
    float x;
    float z;
    float u;
    float v;
} GridPoint;

static GridPoint gridPoints[GRID_RESOLUTION * GRID_RESOLUTION];
static float heightMap[GRID_RESOLUTION * GRID_RESOLUTION];
static SDL_Vertex *meshVertices = NULL;
static int meshVertexCount = 0;
static SDL_Texture *terrainTexture = NULL;
static fnl_state noiseState;

static float rotationX = 1.1f;
static float rotationY = 0.65f;
static float amplitude = 0.8f;
static float animationTime = 0.0f;
static float animationSpeed = 0.35f;
static float cameraDistance = 7.5f;
static float cameraHeight = 1.8f;

static void updateHeightMap(float timeValue);
static void rebuildMesh(void);
static void renderTerrain(SDL_Renderer *renderer);
static void gameTick(double dt);
static void shutdownTerrain(void);
static void updateControls(double dt);

static inline float clampf(float v, float min, float max) {
    if (v < min) return min;
    if (v > max) return max;
    return v;
}

static inline Uint8 clampColor(float value) {
    value = clampf(value, 0.0f, 255.0f);
    return (Uint8)(value + 0.5f);
}

static int pointIndex(int gx, int gz) {
    return gz * GRID_RESOLUTION + gx;
}

static SDL_FPoint projectPoint(float x, float y, float z) {
    float cosY = cosf(rotationY);
    float sinY = sinf(rotationY);
    float rotatedX = x * cosY + z * sinY;
    float rotatedZ = -x * sinY + z * cosY;

    float cosX = cosf(rotationX);
    float sinX = sinf(rotationX);
    float rotatedY = y * cosX - rotatedZ * sinX;
    float rotatedZ2 = y * sinX + rotatedZ * cosX;

    float viewX = rotatedX;
    float viewY = rotatedY - cameraHeight;
    float viewZ = rotatedZ2 + cameraDistance;
    if (viewZ < 0.1f) viewZ = 0.1f;

    float screenScale = (WINW < WINH ? WINW : WINH) * 0.45f;
    float perspective = screenScale / viewZ;

    SDL_FPoint screen;
    screen.x = WINW * 0.5f + viewX * perspective;
    screen.y = WINH * 0.5f - viewY * perspective;
    return screen;
}

static void calculateNormal(int gx, int gz, float *nx, float *ny, float *nz) {
    int left = gx > 0 ? gx - 1 : gx;
    int right = gx < GRID_RESOLUTION - 1 ? gx + 1 : gx;
    int down = gz > 0 ? gz - 1 : gz;
    int up = gz < GRID_RESOLUTION - 1 ? gz + 1 : gz;

    float heightLeft = heightMap[pointIndex(left, gz)];
    float heightRight = heightMap[pointIndex(right, gz)];
    float heightDown = heightMap[pointIndex(gx, down)];
    float heightUp = heightMap[pointIndex(gx, up)];

    float dx = heightRight - heightLeft;
    float dz = heightUp - heightDown;

    *nx = -dx;
    *ny = 2.5f;
    *nz = -dz;

    float length = sqrtf((*nx) * (*nx) + (*ny) * (*ny) + (*nz) * (*nz));
    if (length > 0.0001f) {
        *nx /= length;
        *ny /= length;
        *nz /= length;
    } else {
        *nx = 0.0f;
        *ny = 1.0f;
        *nz = 0.0f;
    }
}

static SDL_Color shadedColor(int gx, int gz) {
    float nx, ny, nz;
    calculateNormal(gx, gz, &nx, &ny, &nz);

    const float lightX = -0.35f;
    const float lightY = 0.8f;
    const float lightZ = 0.45f;
    float lightLen = sqrtf(lightX * lightX + lightY * lightY + lightZ * lightZ);
    float lx = lightX / lightLen;
    float ly = lightY / lightLen;
    float lz = lightZ / lightLen;

    float diffuse = clampf(nx * lx + ny * ly + nz * lz, 0.2f, 1.0f);

    float normalizedHeight = (heightMap[pointIndex(gx, gz)] / amplitude) * 0.5f + 0.5f;
    normalizedHeight = clampf(normalizedHeight, 0.0f, 1.0f);

    float baseR = 120.0f + 70.0f * normalizedHeight;
    float baseG = 140.0f + 80.0f * normalizedHeight;
    float baseB = 155.0f + 50.0f * normalizedHeight;

    SDL_Color color = {
        clampColor(baseR * diffuse),
        clampColor(baseG * diffuse),
        clampColor(baseB * diffuse),
        255
    };
    return color;
}

static void buildVertex(int gx, int gz, SDL_Vertex *outVertex) {
    int idx = pointIndex(gx, gz);
    GridPoint gp = gridPoints[idx];
    float height = heightMap[idx];

    SDL_FPoint projected = projectPoint(gp.x, height, gp.z);
    outVertex->position.x = projected.x;
    outVertex->position.y = projected.y;
    outVertex->tex_coord.x = gp.u;
    outVertex->tex_coord.y = gp.v;
    outVertex->color = shadedColor(gx, gz);
}

static void updateHeightMap(float timeValue) {
    for (int gz = 0; gz < GRID_RESOLUTION; ++gz) {
        for (int gx = 0; gx < GRID_RESOLUTION; ++gx) {
            GridPoint gp = gridPoints[pointIndex(gx, gz)];
            float sample = fnlGetNoise3D(&noiseState, gp.x, gp.z, timeValue * animationSpeed);
            heightMap[pointIndex(gx, gz)] = sample * amplitude;
        }
    }
}

static void rebuildMesh(void) {
    if (!meshVertices) {
        return;
    }

    int vertexCursor = 0;
    for (int gz = 0; gz < GRID_RESOLUTION - 1; ++gz) {
        for (int gx = 0; gx < GRID_RESOLUTION - 1; ++gx) {
            buildVertex(gx, gz, &meshVertices[vertexCursor++]);
            buildVertex(gx, gz + 1, &meshVertices[vertexCursor++]);
            buildVertex(gx + 1, gz, &meshVertices[vertexCursor++]);

            buildVertex(gx + 1, gz, &meshVertices[vertexCursor++]);
            buildVertex(gx, gz + 1, &meshVertices[vertexCursor++]);
            buildVertex(gx + 1, gz + 1, &meshVertices[vertexCursor++]);
        }
    }
}

static void updateControls(double dt) {
    float rotateSpeed = 1.4f;
    if (Held(INP_A)) {
        rotationY -= rotateSpeed * (float)dt;
    }
    if (Held(INP_D)) {
        rotationY += rotateSpeed * (float)dt;
    }
    if (Held(INP_W)) {
        rotationX -= rotateSpeed * (float)dt;
    }
    if (Held(INP_S)) {
        rotationX += rotateSpeed * (float)dt;
    }

    rotationX = clampf(rotationX, 0.2f, 1.5f);

    if (Pressed(INP_ENTER)) {
        rotationX = 1.1f;
        rotationY = 0.65f;
        amplitude = 0.8f;
    }

    if (mouseWheelMoved != 0) {
        amplitude += mouseWheelMoved * 0.08f;
        amplitude = clampf(amplitude, 0.2f, 3.5f);
        mouseWheelMoved = 0;
    }
}

static void gameTick(double dt) {
    updateControls(dt);
    animationTime += (float)dt;
    updateHeightMap(animationTime);
    rebuildMesh();
}

static void renderTerrain(SDL_Renderer *renderer) {
    if (!meshVertices || !terrainTexture) {
        return;
    }

    SDL_RenderGeometry(renderer, terrainTexture, meshVertices, meshVertexCount, NULL, 0);

    SDL_Color textColor = {220, 230, 240, 255};
    drawText("default_font", 18, 18, ANCHOR_TOP_L, textColor,
             "W/S tilt  A/D orbit  Wheel amplitude  Enter reset");

    drawText("default_font", 18, 40, ANCHOR_TOP_L, textColor,
             "Amplitude: %.2f  Speed: %.2f  FPS: %d",
             amplitude, animationSpeed, getFPS());
}

static void shutdownTerrain(void) {
    free(meshVertices);
    meshVertices = NULL;
}

void gameInit(void) {
    titleSet("FastNoise Terrain");

    fnl_state state = fnlCreateState();
    state.seed = 1337;
    state.frequency = 0.35f;
    state.noise_type = FNL_NOISE_OPENSIMPLEX2;
    state.fractal_type = FNL_FRACTAL_RIDGED;
    state.octaves = 5;
    state.lacunarity = 2.0f;
    state.gain = 0.45f;
    state.weighted_strength = 0.35f;
    noiseState = state;

    meshVertexCount = (GRID_RESOLUTION - 1) * (GRID_RESOLUTION - 1) * 6;
    meshVertices = malloc(sizeof(SDL_Vertex) * meshVertexCount);

    float minCoord = -PLANE_EXTENT * 0.5f;
    float maxCoord = PLANE_EXTENT * 0.5f;

    for (int gz = 0; gz < GRID_RESOLUTION; ++gz) {
        for (int gx = 0; gx < GRID_RESOLUTION; ++gx) {
            float tX = (float)gx / (float)(GRID_RESOLUTION - 1);
            float tZ = (float)gz / (float)(GRID_RESOLUTION - 1);

            GridPoint gp;
            gp.x = minCoord + (maxCoord - minCoord) * tX;
            gp.z = minCoord + (maxCoord - minCoord) * tZ;
            gp.u = tX * 4.0f;
            gp.v = tZ * 4.0f;
            gridPoints[pointIndex(gx, gz)] = gp;
        }
    }

    terrainTexture = resGetTexture("terrain_diffuse");
    if (!terrainTexture) {
        terrainTexture = resGetTexture("sand");
    }
    if (terrainTexture) {
        SDL_SetTextureScaleMode(terrainTexture, SDL_ScaleModeBest);
    }

    animationTime = 0.0f;
    updateHeightMap(animationTime);
    rebuildMesh();

    renderF_add(renderTerrain);
    tickF_add(gameTick);

    atexit(shutdownTerrain);
}
