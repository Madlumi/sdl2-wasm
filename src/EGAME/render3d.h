#ifndef RENDER3D_H
#define RENDER3D_H

#include <SDL.h>
#include "../MENGINE/mutil.h"

typedef struct { float x, y, z; } Vec3;

typedef struct {
    Vec3 *verts;
    SDL_FPoint *uvs;
    int vertCount;
    const int *indices;
    int indexCount;
} Mesh;

typedef struct {
    Vec3 position;
    float yaw;
    float pitch;
    float fov;
} Camera3D;

Vec3 v3(float x, float y, float z);
Vec3 v3_add(Vec3 a, Vec3 b);
Vec3 v3_sub(Vec3 a, Vec3 b);
Vec3 v3_scale(Vec3 a, float s);
float v3_dot(Vec3 a, Vec3 b);
Vec3 v3_cross(Vec3 a, Vec3 b);

void render3dSetCamera(Camera3D cam);
Camera3D render3dGetCamera(void);
float render3dMeshDepth(const Mesh *mesh, Vec3 position, Vec3 rotation);
void drawMesh(SDL_Renderer *renderer, const Mesh *mesh, Vec3 position, Vec3 rotation, SDL_Color baseColor);

#endif
