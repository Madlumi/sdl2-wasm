
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

static float currentFloorHeight = 0.0f;  // kept but not really used now

static int   isGrounded = 1;     // start on ground

static const float MAX_FALL_SPEED = -30.0f;  // clamp

// left here for reference, not used in the new slope logic
static const float MAX_STEP_HEIGHT = 1.0f * WALL_HEIGHT + 0.01f;

// heightmap storage
static int LEVEL[MAP_H][MAP_W];

static fnl_state gNoise;

static inline int iabs(int v) { return v < 0 ? -v : v; }


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

            // ----------------------------------------------------
            // Central hill (Chebyshev distance from center)
            // rings: 1 / 2 / 3 => all step-1 differences
            // ----------------------------------------------------
            {
                int dx = iabs(x - cx);
                int dz = iabs(z - cz);
                int d  = dx > dz ? dx : dz;

                if      (d <= 2)           h = 3;     // top
                else if (d <= 4 && h < 2)  h = 2;
                else if (d <= 6 && h < 1)  h = 1;
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
            // Pit: test area, can be fallen into
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
            // ----------------------------------------------------
            {
                float n = fnlGetNoise2D(&gNoise, (float)x, (float)z); // [-1,1]
                int bump = (int)roundf(n * 2.0f); // mostly -2..+2
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
// Tile helpers
// -------------------------------------------------------------

static int tileHeight(int x, int z) {
    if (x < 0 || z < 0 || x >= MAP_W || z >= MAP_H)
        return 9;  // big solid outside
    return LEVEL[z][x];
}

static int isWall(int x, int z) {
    return tileHeight(x, z) > 0;
}

// sample continuous terrain height at world-space position (x,z)
static float sampleHeightAt(float wx, float wz) {
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

    float y00 = h00 * WALL_HEIGHT;
    float y10 = h10 * WALL_HEIGHT;
    float y01 = h01 * WALL_HEIGHT;
    float y11 = h11 * WALL_HEIGHT;

    // bilinear interpolation
    float y0 = y00 + (y10 - y00) * tx;
    float y1 = y01 + (y11 - y01) * tx;
    return y0 + (y1 - y0) * tz;
}


// -------------------------------------------------------------
// Level build: sloped terrain + walls where diff >= 4
// -------------------------------------------------------------

static void buildLevel(void) {
    faceCount = 0;

    SDL_Color terrainColor = (SDL_Color){180, 180, 200, 255};
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

            float y00 = h00 * WALL_HEIGHT;
            float y10 = h10 * WALL_HEIGHT;
            float y01 = h01 * WALL_HEIGHT;
            float y11 = h11 * WALL_HEIGHT;

            float fx = x * TILE_SIZE;
            float fz = z * TILE_SIZE;

            render3dInitQuadMesh(&faces[faceCount++],
                v3(fx,            y00, fz),
                v3(fx+TILE_SIZE,  y10, fz),
                v3(fx+TILE_SIZE,  y11, fz+TILE_SIZE),
                v3(fx,            y01, fz+TILE_SIZE),
                terrainColor);
        }
    }

    // 2) vertical walls where height diff >= 4 between neighbors
    const int WALL_DIFF_THRESHOLD = 4;

    // horizontal edges between (x,z) and (x+1,z)
    for (int z = 0; z < MAP_H; z++) {
        for (int x = 0; x < MAP_W - 1; x++) {
            if (faceCount >= (int)(sizeof(faces)/sizeof(faces[0]))) break;

            int hA = tileHeight(x,     z);
            int hB = tileHeight(x + 1, z);
            int diff = hB - hA;
            if (iabs(diff) < WALL_DIFF_THRESHOLD) continue;

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
            if (iabs(diff) < WALL_DIFF_THRESHOLD) continue;

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
    gNoise.frequency = 0.5f;     // higher => more bumps
    gNoise.seed = 1337;

    generateLevel();
    buildLevel();
    buildFloor();

    currentFloorHeight = 0.0f;

    // start somewhere near (1.5,1.5)
    float groundY = sampleHeightAt(1.5f, 1.5f);
    camPos = v3(1.5f, groundY + CAM_EYE_HEIGHT, 1.5f);
    camYaw = 0.0f;
    camPitch = 0.0f;
    camVelY = 0.0f;
    isGrounded = 1;
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
    //    Uses continuous sampled terrain height
    // ============================================================

    float groundY = sampleHeightAt(camPos.x, camPos.z);
    float feetY   = camPos.y - CAM_EYE_HEIGHT;

    // walked off a ledge? -> not grounded anymore
    if (feetY > groundY + 0.01f) {
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

    // recompute ground+feet after vertical move
    groundY = sampleHeightAt(camPos.x, camPos.z);
    feetY   = camPos.y - CAM_EYE_HEIGHT;

    // landing: only when moving downward and feet go below ground
    if (camVelY <= 0.0f && feetY <= groundY) {
        camPos.y  = groundY + CAM_EYE_HEIGHT;
        camVelY   = 0.0f;
        isGrounded = 1;
        feetY = groundY;
    }

    // ============================================================
    // 2) HORIZONTAL MOVE WITH WALL LOGIC
    //    - If grounded: cannot cross edges where |Δheight| >= 4
    //    - If airborne: can cross walls (if jump arc reaches)
    // ============================================================

    // tile indices (using integer tile heights for wall detection)
    int curTx = (int)(camPos.x / TILE_SIZE);
    int curTz = (int)(camPos.z / TILE_SIZE);
    int curH  = tileHeight(curTx, curTz);

    Vec3 newPos = camPos;
    newPos.x += move.x;
    newPos.z += move.z;

    int newTx = (int)(newPos.x / TILE_SIZE);
    int newTz = (int)(newPos.z / TILE_SIZE);
    int newH  = tileHeight(newTx, newTz);

    const int WALL_DIFF_THRESHOLD = 4;

    if (isGrounded) {
        // grounded: treat |Δheight| >= 4 as a wall edge
        if (iabs(newH - curH) < WALL_DIFF_THRESHOLD) {
            camPos.x = newPos.x;
            camPos.z = newPos.z;
        } else {
            // hit a wall while grounded -> blocked
        }
    } else {
        // airborne: free horizontal movement, can jump over walls
        camPos.x = newPos.x;
        camPos.z = newPos.z;
    }
}


void wolf3dRender(SDL_Renderer *renderer) {
    Camera3D cam = {camPos, camYaw, camPitch, fov};
    render3dSetCamera(cam);

    // floor (optional, mostly hidden by terrain)
    for (int i = 0; i < floorCount; i++) {
        drawMesh(renderer,
            &floorFaces[i].mesh,
            floorFaces[i].position,
            floorFaces[i].rotation,
            floorFaces[i].color);
    }

    // sort terrain + walls back-to-front
    FaceDepth order[MAP_W * MAP_H * 6];
    for (int i = 0; i < faceCount; i++) {
        order[i].index = i;
        order[i].depth = render3dMeshDepth(
            &faces[i].mesh,
            faces[i].position,
            faces[i].rotation);
    }
    qsort(order, faceCount, sizeof(FaceDepth), render3dCompareFaceDepth);

    // draw with distance shading
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
             "3D Terrain | WASD move, A/D turn, SPACE jump");
    drawText("default_font", 10, 28, ANCHOR_TOP_L, white,
             "FPS: %d", getFPS());
}

