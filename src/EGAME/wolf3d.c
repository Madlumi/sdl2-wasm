#include "wolf3d.h"
#include "../MENGINE/keys.h"
#include "../MENGINE/renderer.h"
#include "../MENGINE/tick.h"
#include "render3d.h"
#include <math.h>
#include <stdlib.h>

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

static MeshInstance faces[MAP_W * MAP_H * 6];
static int faceCount = 0;
static MeshInstance floorMeshes[2];

static Vec3 camPos = {1.5f, 0.4f, 1.5f};
static float camYaw = 0.0f;
static float camPitch = 0.0f;
static float fov = 75.0f * (float)M_PI / 180.0f;

static int isWall(int x, int z) {
    if (x < 0 || z < 0 || x >= MAP_W || z >= MAP_H) return 1;
    return LEVEL[z][x] == '1';
}

static void buildLevel(void) {
    faceCount = 0;
    for (int z = 0; z < MAP_H; z++) {
        for (int x = 0; x < MAP_W; x++) {
            if (!isWall(x, z)) continue;
            float fx = x * TILE_SIZE;
            float fz = z * TILE_SIZE;
            SDL_Color base = {180, 180, 200, 255};
            SDL_Color shadow = {140, 140, 170, 255};
            SDL_Color light = {220, 220, 240, 255};

            if (!isWall(x, z - 1) && faceCount < (int)(sizeof(faces) / sizeof(faces[0]))) {
                render3dInitQuadMesh(&faces[faceCount++], v3(fx, 0, fz), v3(fx + TILE_SIZE, 0, fz), v3(fx + TILE_SIZE, WALL_HEIGHT, fz), v3(fx, WALL_HEIGHT, fz), base);
            }
            if (!isWall(x + 1, z) && faceCount < (int)(sizeof(faces) / sizeof(faces[0]))) {
                render3dInitQuadMesh(&faces[faceCount++], v3(fx + TILE_SIZE, 0, fz), v3(fx + TILE_SIZE, 0, fz + TILE_SIZE), v3(fx + TILE_SIZE, WALL_HEIGHT, fz + TILE_SIZE), v3(fx + TILE_SIZE, WALL_HEIGHT, fz), light);
            }
            if (!isWall(x, z + 1) && faceCount < (int)(sizeof(faces) / sizeof(faces[0]))) {
                render3dInitQuadMesh(&faces[faceCount++], v3(fx + TILE_SIZE, 0, fz + TILE_SIZE), v3(fx, 0, fz + TILE_SIZE), v3(fx, WALL_HEIGHT, fz + TILE_SIZE), v3(fx + TILE_SIZE, WALL_HEIGHT, fz + TILE_SIZE), base);
            }
            if (!isWall(x - 1, z) && faceCount < (int)(sizeof(faces) / sizeof(faces[0]))) {
                render3dInitQuadMesh(&faces[faceCount++], v3(fx, 0, fz + TILE_SIZE), v3(fx, 0, fz), v3(fx, WALL_HEIGHT, fz), v3(fx, WALL_HEIGHT, fz + TILE_SIZE), shadow);
            }
            if (faceCount < (int)(sizeof(faces) / sizeof(faces[0]))) {
                render3dInitQuadMesh(&faces[faceCount++], v3(fx, WALL_HEIGHT, fz), v3(fx + TILE_SIZE, WALL_HEIGHT, fz), v3(fx + TILE_SIZE, WALL_HEIGHT, fz + TILE_SIZE), v3(fx, WALL_HEIGHT, fz + TILE_SIZE), light);
            }
        }
    }
}

static void buildFloor(void) {
    Vec3 floorVerts[4] = {
        v3(0, 0, 0),
        v3(MAP_W * TILE_SIZE, 0, 0),
        v3(MAP_W * TILE_SIZE, 0, MAP_H * TILE_SIZE),
        v3(0, 0, MAP_H * TILE_SIZE)
    };
    Vec3 ceilingVerts[4] = {
        v3(0, WALL_HEIGHT, 0),
        v3(MAP_W * TILE_SIZE, WALL_HEIGHT, 0),
        v3(MAP_W * TILE_SIZE, WALL_HEIGHT, MAP_H * TILE_SIZE),
        v3(0, WALL_HEIGHT, MAP_H * TILE_SIZE)
    };

    SDL_Color floorColor = {70, 90, 110, 255};
    SDL_Color ceilingColor = {40, 45, 65, 255};
    float tileScale = 0.5f;
    SDL_FPoint floorUV[4] = {{0.0f, 0.0f}, {MAP_W * tileScale, 0.0f}, {MAP_W * tileScale, MAP_H * tileScale}, {0.0f, MAP_H * tileScale}};
    SDL_FPoint ceilUV[4] = {{0.0f, 0.0f}, {MAP_W * tileScale, 0.0f}, {MAP_W * tileScale, MAP_H * tileScale}, {0.0f, MAP_H * tileScale}};
    render3dInitQuadMeshUV(&floorMeshes[0], floorVerts[0], floorVerts[1], floorVerts[2], floorVerts[3], floorColor,
                           floorUV[0], floorUV[1], floorUV[2], floorUV[3]);
    render3dInitQuadMeshUV(&floorMeshes[1], ceilingVerts[0], ceilingVerts[1], ceilingVerts[2], ceilingVerts[3], ceilingColor,
                           ceilUV[0], ceilUV[1], ceilUV[2], ceilUV[3]);
}

void wolf3dInit() {
    buildLevel();
    buildFloor();
    camPos = v3(1.5f, 0.4f, 1.5f);
    camYaw = 0.0f;
    camPitch = 0.0f;
}

void wolf3dTick(double dt) {
    float moveSpeed = 3.0f * (float)dt;
    float rotSpeed = 1.5f * (float)dt;

    if (Held(INP_A)) camYaw -= rotSpeed;
    if (Held(INP_D)) camYaw += rotSpeed;

    if (camYaw < -M_PI) camYaw += 2.0f * (float)M_PI;
    if (camYaw > M_PI) camYaw -= 2.0f * (float)M_PI;

    Vec3 forward = v3(cosf(camYaw), 0.0f, sinf(camYaw));
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

void wolf3dRender(SDL_Renderer *renderer) {
    Camera3D cam = {camPos, camYaw, camPitch, fov};
    render3dSetCamera(cam);

    drawMesh(renderer, &floorMeshes[0].mesh, floorMeshes[0].position, floorMeshes[0].rotation, floorMeshes[0].color);
    drawMesh(renderer, &floorMeshes[1].mesh, floorMeshes[1].position, floorMeshes[1].rotation, floorMeshes[1].color);

    FaceDepth order[MAP_W * MAP_H * 6];
    for (int i = 0; i < faceCount; i++) {
        order[i].index = i;
        order[i].depth = render3dMeshDepth(&faces[i].mesh, faces[i].position, faces[i].rotation);
    }
    qsort(order, faceCount, sizeof(FaceDepth), render3dCompareFaceDepth);

    for (int i = 0; i < faceCount; i++) {
        int idx = order[i].index;
        float shade = 1.2f / (0.6f + order[i].depth);
        if (shade > 1.0f) shade = 1.0f;
        if (shade < 0.25f) shade = 0.25f;
        SDL_Color c = faces[idx].color;
        SDL_Color shaded = {
            (Uint8)(c.r * shade),
            (Uint8)(c.g * shade),
            (Uint8)(c.b * shade),
            c.a
        };
        drawMesh(renderer, &faces[idx].mesh, faces[idx].position, faces[idx].rotation, shaded);
    }

    SDL_Color white = {255, 255, 255, 255};
    drawText("default_font", 10, 10, ANCHOR_TOP_L, white, "3D Maze | WASD move/turn");
    drawText("default_font", 10, 28, ANCHOR_TOP_L, white, "FPS: %d", getFPS());
}
