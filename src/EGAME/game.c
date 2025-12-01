#include "game.h"
#include "../MENGINE/renderer.h"
#include "../MENGINE/keys.h"
#include "../MENGINE/tick.h"
#include "../MENGINE/debug.h"
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_opengl.h>
#include <math.h>
#include <stdbool.h>

#define RAD2DEG 57.29577951308232f

// Simple maze layout (1 = wall, 0 = open space)
static const int MAP_W = 10;
static const int MAP_H = 10;
static const int level[MAP_H][MAP_W] = {
    {1,1,1,1,1,1,1,1,1,1},
    {1,0,0,0,0,0,0,0,0,1},
    {1,0,1,0,1,1,1,1,0,1},
    {1,0,1,0,0,0,0,1,0,1},
    {1,0,1,1,1,0,0,1,0,1},
    {1,0,0,0,1,0,0,1,0,1},
    {1,0,1,0,1,1,0,1,0,1},
    {1,0,1,0,0,0,0,1,0,1},
    {1,0,0,0,0,0,0,0,0,1},
    {1,1,1,1,1,1,1,1,1,1}
};

static GLuint wallTexture = 0;
static GLuint floorTexture = 0;
static GLuint ceilingTexture = 0;

static struct {
    float x, y, z;
    float yaw;
    float pitch;
} camera = { 2.5f, 0.4f, 2.5f, 0.0f, 0.0f };

static float moveSpeed = 3.5f;
static float mouseSensitivity = 0.0025f;

static GLuint loadTexture(const char *path) {
    SDL_Surface *surface = IMG_Load(path);
    if (!surface) {
        THROW("Failed to load texture %s: %s\n", path, IMG_GetError());
        return 0;
    }

    SDL_Surface *converted = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_ABGR8888, 0);
    SDL_FreeSurface(surface);
    if (!converted) {
        THROW("Failed to convert texture %s\n", path);
        return 0;
    }

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, converted->w, converted->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, converted->pixels);

    SDL_FreeSurface(converted);
    return tex;
}

static int isWall(float x, float z) {
    int gx = (int)floorf(x);
    int gz = (int)floorf(z);
    if (gx < 0 || gz < 0 || gx >= MAP_W || gz >= MAP_H) return 1;
    return level[gz][gx];
}

static void movePlayer(float dx, float dz, double dt) {
    float nextX = camera.x + dx * (float)dt;
    float nextZ = camera.z + dz * (float)dt;
    float radius = 0.2f;

    if (!isWall(nextX + radius, camera.z) && !isWall(nextX - radius, camera.z)) {
        camera.x = nextX;
    }
    if (!isWall(camera.x, nextZ + radius) && !isWall(camera.x, nextZ - radius)) {
        camera.z = nextZ;
    }
}

static void updateCamera(double dt) {
    // Mouse look
    int mx = 0, my = 0;
    SDL_GetRelativeMouseState(&mx, &my);
    camera.yaw   += mx * mouseSensitivity;
    camera.pitch -= my * mouseSensitivity;
    if (camera.pitch > 1.2f) camera.pitch = 1.2f;
    if (camera.pitch < -1.2f) camera.pitch = -1.2f;

    // Keyboard movement
    float forwardX = cosf(camera.yaw);
    float forwardZ = sinf(camera.yaw);
    float rightX = -sinf(camera.yaw);
    float rightZ = cosf(camera.yaw);

    float moveX = 0.0f;
    float moveZ = 0.0f;

    if (Held(INP_W)) { moveX += forwardX; moveZ += forwardZ; }
    if (Held(INP_S)) { moveX -= forwardX; moveZ -= forwardZ; }
    if (Held(INP_A)) { moveX -= rightX;   moveZ -= rightZ;   }
    if (Held(INP_D)) { moveX += rightX;   moveZ += rightZ;   }

    float len = sqrtf(moveX * moveX + moveZ * moveZ);
    if (len > 0.001f) {
        moveX = (moveX / len) * moveSpeed;
        moveZ = (moveZ / len) * moveSpeed;
        movePlayer(moveX, moveZ, dt);
    }

    if (Pressed(INP_EXIT)) {
        QUIT = 1;
    }
}

static void setPerspective(float fovDeg, float aspect, float nearZ, float farZ) {
    float f = 1.0f / tanf(fovDeg * 0.5f / RAD2DEG);
    float proj[16] = {
        f / aspect, 0, 0, 0,
        0, f, 0, 0,
        0, 0, (farZ + nearZ) / (nearZ - farZ), -1,
        0, 0, (2 * farZ * nearZ) / (nearZ - farZ), 0
    };
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(proj);
}

static void drawFloorAndCeiling(void) {
    glEnable(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, floorTexture);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(0.0f, 0.0f, 0.0f);
    glTexCoord2f((float)MAP_W, 0.0f); glVertex3f((float)MAP_W, 0.0f, 0.0f);
    glTexCoord2f((float)MAP_W, (float)MAP_H); glVertex3f((float)MAP_W, 0.0f, (float)MAP_H);
    glTexCoord2f(0.0f, (float)MAP_H); glVertex3f(0.0f, 0.0f, (float)MAP_H);
    glEnd();

    glBindTexture(GL_TEXTURE_2D, ceilingTexture);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(0.0f, 1.2f, 0.0f);
    glTexCoord2f((float)MAP_W, 0.0f); glVertex3f((float)MAP_W, 1.2f, 0.0f);
    glTexCoord2f((float)MAP_W, (float)MAP_H); glVertex3f((float)MAP_W, 1.2f, (float)MAP_H);
    glTexCoord2f(0.0f, (float)MAP_H); glVertex3f(0.0f, 1.2f, (float)MAP_H);
    glEnd();
}

static void drawWalls(void) {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, wallTexture);

    for (int z = 0; z < MAP_H; ++z) {
        for (int x = 0; x < MAP_W; ++x) {
            if (!level[z][x]) continue;
            float fx = (float)x;
            float fz = (float)z;
            float top = 1.2f;

            // Front
            glBegin(GL_QUADS);
            glTexCoord2f(0.0f, 0.0f); glVertex3f(fx, 0.0f, fz);
            glTexCoord2f(1.0f, 0.0f); glVertex3f(fx + 1.0f, 0.0f, fz);
            glTexCoord2f(1.0f, 1.0f); glVertex3f(fx + 1.0f, top, fz);
            glTexCoord2f(0.0f, 1.0f); glVertex3f(fx, top, fz);
            glEnd();

            // Back
            glBegin(GL_QUADS);
            glTexCoord2f(0.0f, 0.0f); glVertex3f(fx + 1.0f, 0.0f, fz + 1.0f);
            glTexCoord2f(1.0f, 0.0f); glVertex3f(fx, 0.0f, fz + 1.0f);
            glTexCoord2f(1.0f, 1.0f); glVertex3f(fx, top, fz + 1.0f);
            glTexCoord2f(0.0f, 1.0f); glVertex3f(fx + 1.0f, top, fz + 1.0f);
            glEnd();

            // Left
            glBegin(GL_QUADS);
            glTexCoord2f(0.0f, 0.0f); glVertex3f(fx, 0.0f, fz + 1.0f);
            glTexCoord2f(1.0f, 0.0f); glVertex3f(fx, 0.0f, fz);
            glTexCoord2f(1.0f, 1.0f); glVertex3f(fx, top, fz);
            glTexCoord2f(0.0f, 1.0f); glVertex3f(fx, top, fz + 1.0f);
            glEnd();

            // Right
            glBegin(GL_QUADS);
            glTexCoord2f(0.0f, 0.0f); glVertex3f(fx + 1.0f, 0.0f, fz);
            glTexCoord2f(1.0f, 0.0f); glVertex3f(fx + 1.0f, 0.0f, fz + 1.0f);
            glTexCoord2f(1.0f, 1.0f); glVertex3f(fx + 1.0f, top, fz + 1.0f);
            glTexCoord2f(0.0f, 1.0f); glVertex3f(fx + 1.0f, top, fz);
            glEnd();
        }
    }
}

static void drawCrosshair(void) {
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, WINW, WINH, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_LINES);
    glVertex2f(WINW / 2 - 8.0f, WINH / 2);
    glVertex2f(WINW / 2 + 8.0f, WINH / 2);
    glVertex2f(WINW / 2, WINH / 2 - 8.0f);
    glVertex2f(WINW / 2, WINH / 2 + 8.0f);
    glEnd();

    glEnable(GL_DEPTH_TEST);
}

static void renderScene(void) {
    glViewport(0, 0, WINW, WINH);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.05f, 0.08f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float aspect = (WINH == 0) ? 1.0f : (float)WINW / (float)WINH;
    setPerspective(70.0f, aspect, 0.05f, 64.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glRotatef(-camera.pitch * RAD2DEG, 1.0f, 0.0f, 0.0f);
    glRotatef(-camera.yaw * RAD2DEG, 0.0f, 1.0f, 0.0f);
    glTranslatef(-camera.x, -camera.y, -camera.z);

    drawFloorAndCeiling();
    drawWalls();
    drawCrosshair();
}

static void gameTick(double dt) {
    updateCamera(dt);
}

void gameInit() {
    IMG_Init(IMG_INIT_PNG);

    wallTexture = loadTexture("res/textures/island.png");
    floorTexture = loadTexture("res/textures/sand.png");
    ceilingTexture = loadTexture("res/textures/noise_a.png");

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    SDL_SetRelativeMouseMode(SDL_TRUE);
    SDL_GetRelativeMouseState(NULL, NULL);

    setGLRenderer(renderScene);
    tickF_add(gameTick);
}
