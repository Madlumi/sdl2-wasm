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

typedef struct {
    Mesh mesh;
    Vec3 verts[4];
    SDL_FPoint uvs[4];
    int indices[6];
    SDL_Color color;
    Vec3 position;
    Vec3 rotation;
    SDL_Texture *texture;
} MeshInstance;

typedef struct {
    int index;
    float depth;
} FaceDepth;

Vec3 v3(float x, float y, float z);
Vec3 v3_add(Vec3 a, Vec3 b);
Vec3 v3_sub(Vec3 a, Vec3 b);
Vec3 v3_scale(Vec3 a, float s);
float v3_dot(Vec3 a, Vec3 b);
Vec3 v3_cross(Vec3 a, Vec3 b);

void render3dSetCamera(Camera3D cam);
Camera3D render3dGetCamera(void);
float render3dMeshDepth(const Mesh *mesh, Vec3 position, Vec3 rotation);
void drawMesh(SDL_Renderer *renderer, const Mesh *mesh, Vec3 position, Vec3 rotation, SDL_Color baseColor, SDL_Texture *texture);
void render3dInitQuadMeshUV(MeshInstance *inst, Vec3 v0, Vec3 v1, Vec3 v2, Vec3 v3p, SDL_Color color, SDL_FPoint uv0, SDL_FPoint uv1, SDL_FPoint uv2, SDL_FPoint uv3);
void render3dInitQuadMesh(MeshInstance *inst, Vec3 v0, Vec3 v1, Vec3 v2, Vec3 v3p, SDL_Color color);
int render3dCompareFaceDepth(const void *a, const void *b);

#endif
