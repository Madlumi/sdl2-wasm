#include "wolf3d.h"
#include "../MENGINE/keys.h"
#include "../MENGINE/renderer.h"
#include "../MENGINE/tick.h"

#define FNL_IMPL
#include "../lib/FastNoiseLite.h"
#include "render3d.h"
#include <math.h>
#include <stdlib.h>


// -------------------------------------------------------------
// Map data (height map)
// -------------------------------------------------------------
#define MAP_W 50
#define MAP_H 50

// -------------------------------------------------------------
// Config + globals
// -------------------------------------------------------------

static const float TILE_SIZE   = 1.0f;
static const float WALL_HEIGHT = 0.1f;

static MeshInstance faces[MAP_W * MAP_H * 6];
static int faceCount = 0;
static MeshInstance floorFaces[MAP_W * MAP_H];
static int floorCount = 0;

static Vec3 camPos = {1.5f, 0.4f, 1.5f};
static float camYaw = 0.0f;
static float camPitch = 0.0f;
static float fov = 75.0f * (float)M_PI / 180.0f;

// jumping / stepping
static float camVelY = 0.0f;
static const float CAM_EYE_HEIGHT = 0.4f;
static const float GRAVITY        = -9.0f;
static const float JUMP_IMPULSE   = 3.0f;

// per-tile floor level in world space
static float currentFloorHeight = 0.0f;

static int   isGrounded = 1;     // start on ground

static const float MAX_FALL_SPEED = -30.0f;  // clamp

static const float MAX_STEP_HEIGHT = 1.0f * WALL_HEIGHT + 0.01f; // walk up 1 tile

// heightmap storage
static int LEVEL[MAP_H][MAP_W];

static fnl_state gNoise;

static inline int iabs(int v) { return v < 0 ? -v : v; }



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

            // ----------------------------------------------------
            // Central hill (Chebyshev distance from center)
            // rings: 1 / 2 / 3 => all step-1 differences
            // ----------------------------------------------------
            {
                int dx = iabs(x - cx);
                int dz = iabs(z - cz);
                int d  = dx > dz ? dx : dz;

                if      (d <= 2)        h = 3;     // top
                else if (d <= 4 && h < 2) h = 2;
                else if (d <= 6 && h < 1) h = 1;
            }

            // ----------------------------------------------------
            // Smaller side hill
            // ----------------------------------------------------
            {
                int dx2 = iabs(x - cx2);
                int dz2 = iabs(z - cz2);
                int d2  = dx2 > dz2 ? dx2 : dz2;

                if      (d2 <= 1 && h < 2) h = 2;
                else if (d2 <= 3 && h < 1) h = 1;
            }

            // ----------------------------------------------------
            // Pit: blocked by your movement rule (newH >= 0)
            // we keep this exact, no noise, so it's a clean test area
            // ----------------------------------------------------
            if (x >= 25 && x <= 28 && z >= 10 && z <= 13) {
                LEVEL[z][x] = -1;
                continue;
            }

            // ----------------------------------------------------
            // 2-step cliff test #1: horizontal cliff
            // ----------------------------------------------------
            if (x >= 10 && x <= 20) {
                if (z == 30) h = 0;
                if (z == 31) h = 2;
            }

            // ----------------------------------------------------
            // 2-step cliff test #2: vertical wall with 1 and 3
            // ----------------------------------------------------
            if (z >= 5 && z <= 10) {
                if (x == 34) h = 1;
                if (x == 35) h = 3;
            }

            // ----------------------------------------------------
            // Perlin noise modulation
            //
            // - fnlGetNoise2D returns roughly [-1, 1]
            // - scale coords to control “wavelength”
            // - convert to small integer bump (-1, 0, +1)
            // ----------------------------------------------------
            {
                float nx = (float)x * 0.15f;   // spatial scale, tweak
                float nz = (float)z * 0.15f;

                float n = fnlGetNoise2D(&gNoise, nx, nz);   // [-1,1]
                // small bump in [-1,1], mostly -1/0/1 after rounding
                int bump = (int)roundf(n * 10.0f);

                h += bump;
            }

            // clamp heights a bit so it doesn't go crazy
            if (h > 4)  h = 4;
            if (h < -2) h = -2;

            LEVEL[z][x] = h;
        }
    }
}





// -------------------------------------------------------------
// Types (use your real ones)
// -------------------------------------------------------------





// -------------------------------------------------------------
// Tile helpers
// -------------------------------------------------------------

static int tileHeight(int x, int z) {
    if (x < 0 || z < 0 || x >= MAP_W || z >= MAP_H)
        return 9;  // big solid border
    return LEVEL[z][x];
}

static int isWall(int x, int z) {
    return tileHeight(x, z) > 0;
}

// -------------------------------------------------------------
// Level build
// -------------------------------------------------------------

static void buildLevel(void) {
    faceCount = 0;

    for (int z = 0; z < MAP_H; z++) {
        for (int x = 0; x < MAP_W; x++) {
            int h = tileHeight(x, z);
            if (h <= 0) continue;

            float fx  = x * TILE_SIZE;
            float fz  = z * TILE_SIZE;
            float top = h * WALL_HEIGHT;

            SDL_Color base   = (SDL_Color){180, 180, 200, 255};
            SDL_Color shadow = (SDL_Color){140, 140, 170, 255};
            SDL_Color light  = (SDL_Color){220, 220, 240, 255};

            // north face
            if (tileHeight(x, z - 1) < h && faceCount < (int)(sizeof(faces)/sizeof(faces[0]))) {
                render3dInitQuadMesh(&faces[faceCount++],
                    v3(fx,           0,   fz),
                    v3(fx+TILE_SIZE, 0,   fz),
                    v3(fx+TILE_SIZE, top, fz),
                    v3(fx,           top, fz),
                    base);
            }

            // east face
            if (tileHeight(x + 1, z) < h && faceCount < (int)(sizeof(faces)/sizeof(faces[0]))) {
                render3dInitQuadMesh(&faces[faceCount++],
                    v3(fx+TILE_SIZE, 0,   fz),
                    v3(fx+TILE_SIZE, 0,   fz+TILE_SIZE),
                    v3(fx+TILE_SIZE, top, fz+TILE_SIZE),
                    v3(fx+TILE_SIZE, top, fz),
                    light);
            }

            // south face
            if (tileHeight(x, z + 1) < h && faceCount < (int)(sizeof(faces)/sizeof(faces[0]))) {
                render3dInitQuadMesh(&faces[faceCount++],
                    v3(fx+TILE_SIZE, 0,   fz+TILE_SIZE),
                    v3(fx,           0,   fz+TILE_SIZE),
                    v3(fx,           top, fz+TILE_SIZE),
                    v3(fx+TILE_SIZE, top, fz+TILE_SIZE),
                    base);
            }

            // west face
            if (tileHeight(x - 1, z) < h && faceCount < (int)(sizeof(faces)/sizeof(faces[0]))) {
                render3dInitQuadMesh(&faces[faceCount++],
                    v3(fx, 0,   fz+TILE_SIZE),
                    v3(fx, 0,   fz),
                    v3(fx, top, fz),
                    v3(fx, top, fz+TILE_SIZE),
                    shadow);
            }

            // top face of this column
            if (faceCount < (int)(sizeof(faces)/sizeof(faces[0]))) {
                render3dInitQuadMesh(&faces[faceCount++],
                    v3(fx,           top, fz),
                    v3(fx+TILE_SIZE, top, fz),
                    v3(fx+TILE_SIZE, top, fz+TILE_SIZE),
                    v3(fx,           top, fz+TILE_SIZE),
                    light);
            }
        }
    }
}

// flat floor at y=0 for now
static void buildFloor(void) {
    floorCount = 0;

    SDL_Color floorColor = (SDL_Color){70, 90, 110, 255};
    float tileScale = 0.5f;

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

            if (floorCount < (int)(sizeof(floorFaces) / sizeof(floorFaces[0]))) {
                render3dInitQuadMeshUV(&floorFaces[floorCount++],
                    f0, f1, f2, f3, floorColor,
                    uv0, uv1, uv2, uv3);
            }
        }
    }
}

// -------------------------------------------------------------
// Init / tick / render
// -------------------------------------------------------------

void wolf3dInit(void) {


   
    // init noise
    gNoise = fnlCreateState();
    gNoise.noise_type = FNL_NOISE_PERLIN;
    gNoise.frequency = 0.5f;     // tweak scale: smaller => smoother, larger => more bumpy
    gNoise.seed = 1337;           // or whatever, make configurable if you like
    generateLevel();     

    buildLevel();
    buildFloor();

    currentFloorHeight = 0.0f;

    camPos = v3(1.5f, currentFloorHeight + CAM_EYE_HEIGHT, 1.5f);
    camYaw = 0.0f;
    camPitch = 0.0f;
    camVelY = 0.0f;
}


void wolf3dTick(double dt) {
    float moveSpeed = 3.0f * (float)dt;
    float rotSpeed  = 1.5f * (float)dt;

    // --------------------
    // rotation
    // --------------------
    if (Held(INP_A)) camYaw -= rotSpeed;
    if (Held(INP_D)) camYaw += rotSpeed;

    if (camYaw < -M_PI) camYaw += 2.0f * (float)M_PI;
    if (camYaw >  M_PI) camYaw -= 2.0f * (float)M_PI;

    // --------------------
    // input: forward/back
    // --------------------
    Vec3 forward = v3(cosf(camYaw), 0.0f, sinf(camYaw));
    Vec3 move    = v3(0, 0, 0);

    if (Held(INP_W)) move = v3_add(move, forward);
    if (Held(INP_S)) move = v3_sub(move, forward);

    if (move.x != 0 || move.z != 0) {
        float len = sqrtf(move.x * move.x + move.z * move.z);
        if (len > 0.0001f) move = v3_scale(move, moveSpeed / len);
    }

    // ============================================================
    // 1) VERTICAL PHYSICS (jumping, falling, landing)
    //    This runs BEFORE horizontal move so jump affects movement
    // ============================================================

    int tileX = (int)(camPos.x / TILE_SIZE);
    int tileZ = (int)(camPos.z / TILE_SIZE);
    int hCur  = tileHeight(tileX, tileZ);
    float floorY = hCur * WALL_HEIGHT;              // can be negative (pit)
    float feetY  = camPos.y - CAM_EYE_HEIGHT;

    // walked off a ledge? -> not grounded anymore
    if (feetY > floorY + 0.01f) {
        isGrounded = 0;
    }

    // start jump if on ground
    if (Pressed(INP_SPACE) && isGrounded) {
        camVelY = JUMP_IMPULSE;
        isGrounded = 0;
    }

    // gravity
    camVelY += GRAVITY * (float)dt;
    if (camVelY < MAX_FALL_SPEED) camVelY = MAX_FALL_SPEED;

    // integrate vertical
    camPos.y += camVelY * (float)dt;

    // recompute floor and feet after vertical move
    tileX = (int)(camPos.x / TILE_SIZE);
    tileZ = (int)(camPos.z / TILE_SIZE);
    hCur  = tileHeight(tileX, tileZ);
    floorY = hCur * WALL_HEIGHT;
    feetY  = camPos.y - CAM_EYE_HEIGHT;

    // landing: only when moving downward and feet go below floor
    if (camVelY <= 0.0f && feetY <= floorY) {
        camPos.y  = floorY + CAM_EYE_HEIGHT;
        camVelY   = 0.0f;
        isGrounded = 1;
        feetY = floorY;
    }

    // ============================================================
    // 2) HORIZONTAL MOVE WITH STEP LOGIC
    //    - If grounded: can step up <= 1 tile height difference
    //    - If airborne: ignore floor height, only block solid walls
    // ============================================================

    // current tile (AFTER vertical update)
    int curTx = (int)(camPos.x / TILE_SIZE);
    int curTz = (int)(camPos.z / TILE_SIZE);
    int curH  = tileHeight(curTx, curTz);
    float curFloorY = curH * WALL_HEIGHT;

    // candidate new position
    Vec3 newPos = camPos;
    newPos.x += move.x;
    newPos.z += move.z;

    int newTx = (int)(newPos.x / TILE_SIZE);
    int newTz = (int)(newPos.z / TILE_SIZE);
    int newH  = tileHeight(newTx, newTz);
    float newFloorY = newH * WALL_HEIGHT;

    // solid tiles (e.g., border walls) block always
        if (isGrounded) {
            // grounded: allow stepping up to 1 tile
            if (newFloorY <= curFloorY + MAX_STEP_HEIGHT) {
                camPos.x = newPos.x;
                camPos.z = newPos.z;
                // going down a ledge: feet are above new floor, so next frame we start falling
                // going up <=1 step: vertical will snap you onto new floor in a frame or two
            } else {
                // too high (>= 2 tiles): need jump, cannot just walk in
            }
        } else {
            // airborne: free horizontal movement over gaps / towards higher ledges
            camPos.x = newPos.x;
            camPos.z = newPos.z;
        }
}

void wolf3dRender(SDL_Renderer *renderer) {
    Camera3D cam = {camPos, camYaw, camPitch, fov};
    render3dSetCamera(cam);

    // floor
    for (int i = 0; i < floorCount; i++) {
        drawMesh(renderer,
            &floorFaces[i].mesh,
            floorFaces[i].position,
            floorFaces[i].rotation,
            floorFaces[i].color);
    }

    // sort walls back-to-front
    FaceDepth order[MAP_W * MAP_H * 6];
    for (int i = 0; i < faceCount; i++) {
        order[i].index = i;
        order[i].depth = render3dMeshDepth(
            &faces[i].mesh,
            faces[i].position,
            faces[i].rotation);
    }
    qsort(order, faceCount, sizeof(FaceDepth), render3dCompareFaceDepth);

    // draw walls with distance shading
    for (int i = 0; i < faceCount; i++) {
        int idx = order[i].index;
        float shade = 1.2f / (0.6f + order[i].depth);
        if (shade > 1.0f)  shade = 1.0f;
        if (shade < 0.25f) shade = 0.25f;

        SDL_Color c = faces[idx].color;
        SDL_Color shaded = {
            (Uint8)(c.r * shade),
            (Uint8)(c.g * shade),
            (Uint8)(c.b * shade),
            c.a
        };

        drawMesh(renderer,
            &faces[idx].mesh,
            faces[idx].position,
            faces[idx].rotation,
            shaded);
    }

    SDL_Color white = {255, 255, 255, 255};
    drawText("default_font", 10, 10, ANCHOR_TOP_L, white,
             "3D Maze | WASD move, A/D turn, SPACE jump");
    drawText("default_font", 10, 28, ANCHOR_TOP_L, white,
             "FPS: %d", getFPS());
}

