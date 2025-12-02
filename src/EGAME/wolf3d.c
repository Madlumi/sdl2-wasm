#include "../MENGINE/keys.h"
#include "../MENGINE/keys.h"
#include "../MENGINE/render3d.h"
#include "../MENGINE/renderer.h"
#include "../MENGINE/tick.h"
#include <math.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const char *LEVEL[] = {
    "1111111111",
    "1........1",
    "1.1111...1",
    "1.1..1...1",
    "1.1..1...1",
    "1....1...1",
    "1....1...1",
    "1111111111"
};

#define MAP_W 10
#define MAP_H 8
static const float TILE_SIZE = 1.0f;
static const float WALL_HEIGHT = 1.5f;

static Vec3 camPos = {1.5f, 0.4f, 1.5f};
static float camYaw = 0.0f;
static float camPitch = 0.0f;
static float fov = 75.0f;

typedef struct {
    const Mesh *mesh;
    Vec3 position;
    Vec3 rotation;
    Vec3 scale;
    SDL_Color tint;
    float depth;
} MeshInstance;

static MeshInstance instances[MAP_W * MAP_H * 8];
static int instanceCount = 0;

static const Vertex3D WALL_VERTS[] = {
    {{-0.5f, 0.0f, 0.0f}, {0.0f, 1.0f}},
    {{0.5f, 0.0f, 0.0f}, {1.0f, 1.0f}},
    {{0.5f, 1.0f, 0.0f}, {1.0f, 0.0f}},
    {{-0.5f, 1.0f, 0.0f}, {0.0f, 0.0f}},
};

static const U32 WALL_INDICES[] = {0, 1, 2, 0, 2, 3};
static const Mesh WALL_MESH = {WALL_VERTS, WALL_INDICES, 4, 6};

static const Vertex3D FLOOR_VERTS[] = {
    {{-0.5f, 0.0f, -0.5f}, {0.0f, 0.0f}},
    {{0.5f, 0.0f, -0.5f}, {1.0f, 0.0f}},
    {{0.5f, 0.0f, 0.5f}, {1.0f, 1.0f}},
    {{-0.5f, 0.0f, 0.5f}, {0.0f, 1.0f}},
};

static const Mesh FLOOR_MESH = {FLOOR_VERTS, WALL_INDICES, 4, 6};

static int isWall(int x, int z) {
    if (x < 0 || z < 0 || x >= MAP_W || z >= MAP_H) return 1;
    return LEVEL[z][x] == '1';
}

static void addInstance(const Mesh *mesh, Vec3 pos, Vec3 rot, Vec3 scale, SDL_Color tint) {
    if (instanceCount >= (int)(sizeof(instances) / sizeof(instances[0]))) return;
    instances[instanceCount].mesh = mesh;
    instances[instanceCount].position = pos;
    instances[instanceCount].rotation = rot;
    instances[instanceCount].scale = scale;
    instances[instanceCount].tint = tint;
    instances[instanceCount].depth = 0.0f;
    instanceCount++;
}

static void buildLevel(void) {
    instanceCount = 0;
    SDL_Color base = {180, 180, 200, 255};
    SDL_Color shadow = {140, 140, 170, 255};
    SDL_Color light = {220, 220, 240, 255};

    // Floor and ceiling as single large quads
    Vec3 center = v3(MAP_W * TILE_SIZE * 0.5f, 0.0f, MAP_H * TILE_SIZE * 0.5f);
    addInstance(&FLOOR_MESH, center, v3(0, 0, 0), v3(MAP_W * TILE_SIZE, 1.0f, MAP_H * TILE_SIZE), (SDL_Color){70, 90, 110, 255});
    addInstance(&FLOOR_MESH, v3_add(center, v3(0, WALL_HEIGHT, 0)), v3((float)M_PI, 0, 0),
                v3(MAP_W * TILE_SIZE, 1.0f, MAP_H * TILE_SIZE), (SDL_Color){40, 45, 65, 255});

    for (int z = 0; z < MAP_H; z++) {
        for (int x = 0; x < MAP_W; x++) {
            if (!isWall(x, z)) continue;
            float fx = (x + 0.5f) * TILE_SIZE;
            float fz = (z + 0.5f) * TILE_SIZE;
            Vec3 wallScale = v3(TILE_SIZE, WALL_HEIGHT, 1.0f);

            if (!isWall(x, z - 1)) {
                addInstance(&WALL_MESH, v3(fx, WALL_HEIGHT * 0.5f, z * TILE_SIZE), v3(0, 0, 0), wallScale, base);
            }
            if (!isWall(x + 1, z)) {
                addInstance(&WALL_MESH, v3((x + 1) * TILE_SIZE, WALL_HEIGHT * 0.5f, fz), v3(0, (float)M_PI * 0.5f, 0), wallScale, light);
            }
            if (!isWall(x, z + 1)) {
                addInstance(&WALL_MESH, v3(fx, WALL_HEIGHT * 0.5f, (z + 1) * TILE_SIZE), v3(0, (float)M_PI, 0), wallScale, base);
            }
            if (!isWall(x - 1, z)) {
                addInstance(&WALL_MESH, v3(x * TILE_SIZE, WALL_HEIGHT * 0.5f, fz), v3(0, (float)-M_PI * 0.5f, 0), wallScale, shadow);
            }
        }
    }
}

void wolf3dInit() {
    buildLevel();
    camPos = v3(1.5f, 0.4f, 1.5f);
    camYaw = 0.0f;
    camPitch = 0.0f;
}

void wolf3dTick(double dt) {
    float moveSpeed = 3.0f * (float)dt;
    float rotSpeed = 1.5f * (float)dt;

    if (Held(INP_A)) camYaw -= rotSpeed;
    if (Held(INP_D)) camYaw += rotSpeed;

    if (camYaw < -(float)M_PI) camYaw += 2.0f * (float)M_PI;
    if (camYaw > (float)M_PI) camYaw -= 2.0f * (float)M_PI;

    Vec3 forward = v3(cosf(camYaw), 0.0f, sinf(camYaw));
    Vec3 right = v3(-sinf(camYaw), 0.0f, cosf(camYaw));

    Vec3 move = v3(0, 0, 0);
    if (Held(INP_W)) move = v3_add(move, forward);
    if (Held(INP_S)) move = v3_sub(move, forward);

    if (move.x != 0 || move.z != 0) {
        float len = sqrtf(move.x * move.x + move.z * move.z);
        if (len > 0.0001f) move = v3_scale(move, moveSpeed / len);
    }

    Vec3 newPos = v3_add(camPos, move);
    int cx = (int)(newPos.x / TILE_SIZE);
    int cz = (int)(newPos.z / TILE_SIZE);
    if (!isWall(cx, cz)) {
        camPos.x = newPos.x;
        camPos.z = newPos.z;
    }
}

static int compareDepth(const void *a, const void *b) {
    float da = ((const MeshInstance *)a)->depth;
    float db = ((const MeshInstance *)b)->depth;
    if (da < db) return 1;
    if (da > db) return -1;
    return 0;
}

void wolf3dRender(SDL_Renderer *renderer) {
    Vec3 forward = v3(cosf(camYaw) * cosf(camPitch), sinf(camPitch), sinf(camYaw) * cosf(camPitch));

    render3dSetCamera(camPos, camYaw, camPitch, fov);

    // Compute center depths for sorting (Painter's algorithm)
    for (int i = 0; i < instanceCount; i++) {
        instances[i].depth = v3_dot(v3_sub(instances[i].position, camPos), forward);
    }
    qsort(instances, instanceCount, sizeof(MeshInstance), compareDepth);

    for (int i = 0; i < instanceCount; i++) {
        drawMesh(renderer, instances[i].mesh, instances[i].position, instances[i].rotation, instances[i].scale, instances[i].tint);
    }

    SDL_Color white = {255, 255, 255, 255};
    drawText("default_font", 10, 10, ANCHOR_TOP_L, white, "3D Maze | WASD move/turn");
    drawText("default_font", 10, 28, ANCHOR_TOP_L, white, "FPS: %d", getFPS());
}
