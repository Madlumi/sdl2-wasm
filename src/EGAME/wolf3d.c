#include "wolf3d.h"
#include "../MENGINE/keys.h"
#include "../MENGINE/renderer.h"
#include "../MENGINE/tick.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct { float x, y, z; } Vec3;

typedef struct {
    Vec3 verts[4];
    SDL_Color color;
} WallFace;

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

static WallFace faces[MAP_W * MAP_H * 6];
static int faceCount = 0;

static Vec3 camPos = {5.0f, 0.4f, 5.0f};
static float camYaw = 0.0f;
static float camPitch = 0.0f;
static float fov = 75.0f * (float)M_PI / 180.0f;

static int isWall(int x, int z) {
    if (x < 0 || z < 0 || x >= MAP_W || z >= MAP_H) return 1;
    return LEVEL[z][x] == '1';
}

static Vec3 v3(float x, float y, float z) { Vec3 v = {x, y, z}; return v; }

static Vec3 v3_sub(Vec3 a, Vec3 b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
static float v3_dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static Vec3 v3_cross(Vec3 a, Vec3 b) {
    return v3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}
static Vec3 v3_add(Vec3 a, Vec3 b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
static Vec3 v3_scale(Vec3 a, float s) { return v3(a.x * s, a.y * s, a.z * s); }

static void addFace(Vec3 v0, Vec3 v1, Vec3 v2, Vec3 v3, SDL_Color color) {
    if (faceCount >= (int)(sizeof(faces) / sizeof(faces[0]))) return;
    faces[faceCount].verts[0] = v0;
    faces[faceCount].verts[1] = v1;
    faces[faceCount].verts[2] = v2;
    faces[faceCount].verts[3] = v3;
    faces[faceCount].color = color;
    faceCount++;
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

            if (!isWall(x, z - 1)) {
                addFace(v3(fx, 0, fz), v3(fx + TILE_SIZE, 0, fz), v3(fx + TILE_SIZE, WALL_HEIGHT, fz), v3(fx, WALL_HEIGHT, fz), base);
            }
            if (!isWall(x + 1, z)) {
                addFace(v3(fx + TILE_SIZE, 0, fz), v3(fx + TILE_SIZE, 0, fz + TILE_SIZE), v3(fx + TILE_SIZE, WALL_HEIGHT, fz + TILE_SIZE), v3(fx + TILE_SIZE, WALL_HEIGHT, fz), light);
            }
            if (!isWall(x, z + 1)) {
                addFace(v3(fx + TILE_SIZE, 0, fz + TILE_SIZE), v3(fx, 0, fz + TILE_SIZE), v3(fx, WALL_HEIGHT, fz + TILE_SIZE), v3(fx + TILE_SIZE, WALL_HEIGHT, fz + TILE_SIZE), base);
            }
            if (!isWall(x - 1, z)) {
                addFace(v3(fx, 0, fz + TILE_SIZE), v3(fx, 0, fz), v3(fx, WALL_HEIGHT, fz), v3(fx, WALL_HEIGHT, fz + TILE_SIZE), shadow);
            }
            addFace(v3(fx, WALL_HEIGHT, fz), v3(fx + TILE_SIZE, WALL_HEIGHT, fz), v3(fx + TILE_SIZE, WALL_HEIGHT, fz + TILE_SIZE), v3(fx, WALL_HEIGHT, fz + TILE_SIZE), light);
        }
    }
}

static int projectPoint(Vec3 p, SDL_FPoint *out, float *depth, Vec3 forward, Vec3 right, Vec3 upVec, float aspect) {
    Vec3 rel = v3_sub(p, camPos);
    float viewX = v3_dot(rel, right);
    float viewY = v3_dot(rel, upVec);
    float viewZ = v3_dot(rel, forward);

    const float NEAR_PLANE = 0.05f;
    if (viewZ <= NEAR_PLANE) return 0;

    float f = 1.0f / tanf(fov * 0.5f);
    float nx = (viewX * f / aspect) / viewZ;
    float ny = (viewY * f) / viewZ;

    out->x = (nx * 0.5f + 0.5f) * WINW;
    out->y = (1.0f - (ny * 0.5f + 0.5f)) * WINH;
    if (depth) *depth = viewZ;
    return 1;
}

static void drawQuad(SDL_Renderer *renderer, WallFace *face, Vec3 forward, Vec3 right, Vec3 upVec) {
    SDL_FPoint proj[4];
    float depth[4];
    float aspect = (float)WINW / (float)WINH;
    for (int i = 0; i < 4; i++) {
        if (!projectPoint(face->verts[i], &proj[i], &depth[i], forward, right, upVec, aspect)) {
            return;
        }
    }

    float avgDepth = (depth[0] + depth[1] + depth[2] + depth[3]) * 0.25f;
    float shade = 1.2f / (0.6f + avgDepth);
    if (shade > 1.0f) shade = 1.0f;
    if (shade < 0.25f) shade = 0.25f;

    SDL_Color c = face->color;
    SDL_Color shaded = {
        (Uint8)(c.r * shade),
        (Uint8)(c.g * shade),
        (Uint8)(c.b * shade),
        c.a
    };

    SDL_Vertex verts[6];
    SDL_Color colors[2] = {shaded, shaded};
    (void)colors; // silence unused in older SDL versions
    verts[0].position = proj[0];
    verts[1].position = proj[1];
    verts[2].position = proj[2];
    verts[3].position = proj[0];
    verts[4].position = proj[2];
    verts[5].position = proj[3];
    for (int i = 0; i < 6; i++) {
        verts[i].color = shaded;
        verts[i].tex_coord.x = verts[i].tex_coord.y = 0.0f;
    }

    SDL_RenderGeometry(renderer, NULL, verts, 6, NULL, 0);
}

static void drawFloor(SDL_Renderer *renderer, Vec3 forward, Vec3 right, Vec3 upVec) {
    SDL_FPoint proj[4];
    float aspect = (float)WINW / (float)WINH;
    float depth;
    Vec3 verts[4] = {
        v3(0, 0, 0),
        v3(MAP_W * TILE_SIZE, 0, 0),
        v3(MAP_W * TILE_SIZE, 0, MAP_H * TILE_SIZE),
        v3(0, 0, MAP_H * TILE_SIZE)
    };

    for (int i = 0; i < 4; i++) {
        if (!projectPoint(verts[i], &proj[i], &depth, forward, right, upVec, aspect)) return;
    }

    SDL_Color floorColor = {70, 90, 110, 255};
    SDL_Color ceilingColor = {40, 45, 65, 255};

    SDL_Vertex floorVerts[6];
    floorVerts[0].position = proj[0];
    floorVerts[1].position = proj[1];
    floorVerts[2].position = proj[2];
    floorVerts[3].position = proj[0];
    floorVerts[4].position = proj[2];
    floorVerts[5].position = proj[3];
    for (int i = 0; i < 6; i++) {
        floorVerts[i].color = floorColor;
        floorVerts[i].tex_coord.x = floorVerts[i].tex_coord.y = 0.0f;
    }
    SDL_RenderGeometry(renderer, NULL, floorVerts, 6, NULL, 0);

    // Ceiling just mirrored at WALL_HEIGHT
    Vec3 cVerts[4] = {
        v3(0, WALL_HEIGHT, 0),
        v3(MAP_W * TILE_SIZE, WALL_HEIGHT, 0),
        v3(MAP_W * TILE_SIZE, WALL_HEIGHT, MAP_H * TILE_SIZE),
        v3(0, WALL_HEIGHT, MAP_H * TILE_SIZE)
    };
    for (int i = 0; i < 4; i++) {
        if (!projectPoint(cVerts[i], &proj[i], &depth, forward, right, upVec, aspect)) return;
    }
    SDL_Vertex ceilVerts[6];
    ceilVerts[0].position = proj[0];
    ceilVerts[1].position = proj[1];
    ceilVerts[2].position = proj[2];
    ceilVerts[3].position = proj[0];
    ceilVerts[4].position = proj[2];
    ceilVerts[5].position = proj[3];
    for (int i = 0; i < 6; i++) {
        ceilVerts[i].color = ceilingColor;
        ceilVerts[i].tex_coord.x = ceilVerts[i].tex_coord.y = 0.0f;
    }
    SDL_RenderGeometry(renderer, NULL, ceilVerts, 6, NULL, 0);
}

void wolf3dInit() {
    buildLevel();
    camPos = v3(2.5f, 0.4f, 2.5f);
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

void wolf3dRender(SDL_Renderer *renderer) {
    Vec3 forward = v3(cosf(camYaw) * cosf(camPitch), sinf(camPitch), sinf(camYaw) * cosf(camPitch));
    Vec3 right = v3(cosf(camYaw + (float)M_PI * 0.5f), 0.0f, sinf(camYaw + (float)M_PI * 0.5f));
    Vec3 upVec = v3_cross(right, forward);

    drawFloor(renderer, forward, right, upVec);
    for (int i = 0; i < faceCount; i++) {
        drawQuad(renderer, &faces[i], forward, right, upVec);
    }

    SDL_Color white = {255, 255, 255, 255};
    drawText("default_font", 10, 10, ANCHOR_TOP_L, white, "3D Maze | WASD move/turn");
    drawText("default_font", 10, 28, ANCHOR_TOP_L, white, "FPS: %d", getFPS());
}
