
#include "wolf3d.h"
#include "../MENGINE/keys.h"
#include "../MENGINE/renderer.h"
#include "../MENGINE/tick.h"
#include "../MENGINE/ui.h"
#include "render3d.h"
#include <math.h>
#include <stdlib.h>

// -------------------------------------------------------------
// Shared map config (must match level.c)
// -------------------------------------------------------------

#define MAP_W 50
#define MAP_H 50

static const float TILE_SIZE   = 1.0f;
static const float WALL_HEIGHT = 0.1f;

// -------------------------------------------------------------
// Externs from level.c
// -------------------------------------------------------------

extern MeshInstance faces[MAP_W * MAP_H * 6];
extern int faceCount;

extern MeshInstance floorFaces[MAP_W * MAP_H];
extern int floorCount;

void levelInit(void);
int   tileHeight(int x, int z);
float sampleHeightAt(float wx, float wz);

// -------------------------------------------------------------
// Camera / physics globals
// -------------------------------------------------------------

static Vec3 camPos = {1.5f, 0.4f, 1.5f};
static float camYaw = 0.0f;
static float camPitch = 0.0f;
static float fov = 75.0f * (float)M_PI / 180.0f;
static float fovDegrees = 75.0f;

// jumping / physics
static float camVelY = 0.0f;
static const float CAM_EYE_HEIGHT = 0.4f;
static const float GRAVITY        = -9.0f;
static const float JUMP_IMPULSE   = 3.0f;
static const float MAX_FALL_SPEED = -30.0f;

static int   isGrounded = 1;     // start on ground
static int   uiHandlerIndex = -1;

static void startJump(void) {
    if (isGrounded) {
        camVelY = JUMP_IMPULSE;
        isGrounded = 0;
    }
}

static void jumpButtonPressed(Elem *e) {
    (void)e;
    startJump();
}

static void initUi(void) {
    RECT screenArea = {0, 0, WINW, WINH};
    uiHandlerIndex = initUiHandler(screenArea);
    if (uiHandlerIndex < 0) { return; }

    UiHandler *handler = &ui[uiHandlerIndex];

    RECT sliderArea = {20, WINH - 70, 240, 18};
    Elem *fovSlider = createSlider(sliderArea, "FOV", &fovDegrees, 50.0f, 110.0f);
    if (fovSlider != NULL) { addElem(handler, fovSlider); }

    RECT buttonArea = {20, WINH - 110, 120, 28};
    Elem *jumpButton = createButton(buttonArea, "Jump", jumpButtonPressed);
    if (jumpButton != NULL) { addElem(handler, jumpButton); }

    RECT textboxArea = {WINW - 220, 20, 200, 28};
    Elem *textbox = createTextbox(textboxArea, "Ready to explore!");
    if (textbox != NULL) { addElem(handler, textbox); }
}

// -------------------------------------------------------------
// Init / tick / render
// -------------------------------------------------------------

void wolf3dInit(void) {
    levelInit();

    // start somewhere near (1.5, 1.5)
    float groundY = sampleHeightAt(1.5f, 1.5f);
    camPos = v3(1.5f, groundY + CAM_EYE_HEIGHT, 1.5f);
    camYaw = 0.0f;
    camPitch = 0.0f;
    camVelY = 0.0f;
    isGrounded = 1;
    fovDegrees = fov * 180.0f / (float)M_PI;

    initUi();
}

void wolf3dTick(double dt) {
    fov = fovDegrees * (float)M_PI / 180.0f;
    float moveSpeed = 3.0f * (float)dt;
    float rotSpeed  = 1.5f * (float)dt;

    // rotation
    if (Held(INP_A)) camYaw -= rotSpeed;
    if (Held(INP_D)) camYaw += rotSpeed;

    if (camYaw < -M_PI) camYaw += 2.0f * (float)M_PI;
    if (camYaw >  M_PI) camYaw -= 2.0f * (float)M_PI;

    // input: forward/back
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
    if (Pressed(INP_SPACE)) {
        startJump();
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
    //    - If airborne: can cross anything (if jump reaches)
// ============================================================

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
        if (abs(newH - curH) < WALL_DIFF_THRESHOLD) {
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

