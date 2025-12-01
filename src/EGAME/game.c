#include "game.h"
#include "../MENGINE/renderer.h"
#include "../MENGINE/tick.h"
#include "../MENGINE/keys.h"
#include <SDL.h>
#include <SDL_opengl.h>
#include <math.h>
#include <stdbool.h>

#define MAP_W 12
#define MAP_H 12

static int worldMap[MAP_H][MAP_W] = {
    {1,1,1,1,1,1,1,1,1,1,1,1},
    {1,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,1,0,1,0,1,0,1,0,0,1},
    {1,0,1,0,1,0,1,0,1,0,0,1},
    {1,0,1,0,0,0,1,0,1,0,0,1},
    {1,0,1,1,1,0,1,0,1,1,0,1},
    {1,0,0,0,1,0,0,0,0,0,0,1},
    {1,0,1,0,1,1,1,1,1,1,0,1},
    {1,0,1,0,0,0,0,0,0,1,0,1},
    {1,0,1,1,1,1,1,1,0,1,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,1},
    {1,1,1,1,1,1,1,1,1,1,1,1}
};

static float camX = 2.5f;
static float camY = 0.6f;
static float camZ = 2.5f;
static float yaw = 0.0f;
static float pitch = 0.0f;
static float bobTime = 0.0f;

static const float MOVE_SPEED = 3.0f;
static const float TURN_SPEED = 1.8f;

static void setPerspective(float fovDeg, float aspect, float nearZ, float farZ) {
    float halfRad = fovDeg * 0.5f * (float)M_PI / 180.0f;
    float ymax = nearZ * tanf(halfRad);
    float xmax = ymax * aspect;
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-xmax, xmax, -ymax, ymax, nearZ, farZ);
}

static bool isWall(float wx, float wz) {
    int gx = (int)floorf(wx);
    int gz = (int)floorf(wz);
    if (gx < 0 || gz < 0 || gx >= MAP_W || gz >= MAP_H) {
        return true;
    }
    return worldMap[gz][gx] == 1;
}

static void tryMove(float dx, float dz, float dt) {
    float nextX = camX + dx * dt;
    float nextZ = camZ + dz * dt;

    if (!isWall(nextX, camZ)) {
        camX = nextX;
    }
    if (!isWall(camX, nextZ)) {
        camZ = nextZ;
    }
}

static void handleMovement(float dt) {
    float forwardX = cosf(yaw);
    float forwardZ = sinf(yaw);

    float moveX = 0.0f;
    float moveZ = 0.0f;
    bool walking = false;

    if (Held(INP_W)) {
        moveX += forwardX;
        moveZ += forwardZ;
        walking = true;
    }
    if (Held(INP_S)) {
        moveX -= forwardX;
        moveZ -= forwardZ;
        walking = true;
    }

    if (Held(INP_A)) {
        yaw -= TURN_SPEED * dt;
    }
    if (Held(INP_D)) {
        yaw += TURN_SPEED * dt;
    }

    float mag = sqrtf(moveX * moveX + moveZ * moveZ);
    if (mag > 0.0f) {
        moveX = (moveX / mag) * MOVE_SPEED;
        moveZ = (moveZ / mag) * MOVE_SPEED;
        tryMove(moveX, moveZ, dt);
    }

    if (walking) {
        bobTime += dt * MOVE_SPEED;
    } else {
        bobTime *= 0.9f;
    }
}

static void lookAround(float dt) {
    float targetPitch = 0.05f * sinf(bobTime * 6.0f);
    pitch += (targetPitch - pitch) * fminf(dt * 8.0f, 1.0f);
}

static void drawFloorAndCeiling(void) {
    glBegin(GL_QUADS);
    glColor3f(0.05f, 0.1f, 0.15f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f((float)MAP_W, 0.0f, 0.0f);
    glVertex3f((float)MAP_W, 0.0f, (float)MAP_H);
    glVertex3f(0.0f, 0.0f, (float)MAP_H);

    glColor3f(0.07f, 0.07f, 0.1f);
    glVertex3f(0.0f, 1.6f, 0.0f);
    glVertex3f((float)MAP_W, 1.6f, 0.0f);
    glVertex3f((float)MAP_W, 1.6f, (float)MAP_H);
    glVertex3f(0.0f, 1.6f, (float)MAP_H);
    glEnd();
}

static void drawWallCell(int gx, int gz) {
    float x = (float)gx;
    float z = (float)gz;
    float h = 1.5f;

    glBegin(GL_QUADS);
    glColor3f(0.6f, 0.55f, 0.5f);
    glVertex3f(x, 0.0f, z);
    glVertex3f(x + 1.0f, 0.0f, z);
    glVertex3f(x + 1.0f, h, z);
    glVertex3f(x, h, z);

    glColor3f(0.5f, 0.45f, 0.4f);
    glVertex3f(x + 1.0f, 0.0f, z);
    glVertex3f(x + 1.0f, 0.0f, z + 1.0f);
    glVertex3f(x + 1.0f, h, z + 1.0f);
    glVertex3f(x + 1.0f, h, z);

    glColor3f(0.55f, 0.5f, 0.45f);
    glVertex3f(x + 1.0f, 0.0f, z + 1.0f);
    glVertex3f(x, 0.0f, z + 1.0f);
    glVertex3f(x, h, z + 1.0f);
    glVertex3f(x + 1.0f, h, z + 1.0f);

    glColor3f(0.58f, 0.53f, 0.48f);
    glVertex3f(x, 0.0f, z + 1.0f);
    glVertex3f(x, 0.0f, z);
    glVertex3f(x, h, z);
    glVertex3f(x, h, z + 1.0f);
    glEnd();
}

static void drawWorld(void) {
    drawFloorAndCeiling();

    for (int z = 0; z < MAP_H; z++) {
        for (int x = 0; x < MAP_W; x++) {
            if (worldMap[z][x]) {
                drawWallCell(x, z);
            }
        }
    }
}

static void drawCrosshair(void) {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, WINW, WINH, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glBegin(GL_LINES);
    glColor3f(0.9f, 0.9f, 0.9f);
    glVertex2f(WINW * 0.5f - 8.0f, WINH * 0.5f);
    glVertex2f(WINW * 0.5f + 8.0f, WINH * 0.5f);
    glVertex2f(WINW * 0.5f, WINH * 0.5f - 8.0f);
    glVertex2f(WINW * 0.5f, WINH * 0.5f + 8.0f);
    glEnd();
    glEnable(GL_DEPTH_TEST);
}

static void gameTick(double dt) {
    handleMovement((float)dt);
    lookAround((float)dt);
}

static void gameRender(SDL_Renderer *unused) {
    (void)unused;

    float aspect = (float)WINW / (float)WINH;
    setPerspective(70.0f, aspect, 0.1f, 100.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glRotatef(-pitch * 57.2957795f, 1.0f, 0.0f, 0.0f);
    glRotatef(-yaw * 57.2957795f, 0.0f, 1.0f, 0.0f);
    glTranslatef(-camX, -(camY + 0.05f * sinf(bobTime * 8.0f)), -camZ);

    drawWorld();
    drawCrosshair();
}

void gameInit() {
    SDL_SetRelativeMouseMode(SDL_FALSE);
    tickF_add(gameTick);
    renderF_add(gameRender);
}
