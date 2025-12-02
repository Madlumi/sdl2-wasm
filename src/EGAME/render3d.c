#include "render3d.h"
#include "../MENGINE/renderer.h"
#include <math.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static Camera3D gCamera = {{0.0f, 0.0f, 0.0f}, 0.0f, 0.0f, 75.0f * (float)M_PI / 180.0f};

Vec3 v3(float x, float y, float z) { Vec3 v = {x, y, z}; return v; }
Vec3 v3_add(Vec3 a, Vec3 b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
Vec3 v3_sub(Vec3 a, Vec3 b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
Vec3 v3_scale(Vec3 a, float s) { return v3(a.x * s, a.y * s, a.z * s); }
float v3_dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 v3_cross(Vec3 a, Vec3 b) {
    return v3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

static Vec3 rotateVector(Vec3 v, Vec3 rot) {
    float cy = cosf(rot.y), sy = sinf(rot.y);
    float cx = cosf(rot.x), sx = sinf(rot.x);
    float cz = cosf(rot.z), sz = sinf(rot.z);

    // Yaw (Y axis)
    float x1 = cy * v.x + sy * v.z;
    float z1 = -sy * v.x + cy * v.z;
    float y1 = v.y;

    // Pitch (X axis)
    float y2 = cx * y1 - sx * z1;
    float z2 = sx * y1 + cx * z1;
    float x2 = x1;

    // Roll (Z axis)
    float x3 = cz * x2 - sz * y2;
    float y3 = sz * x2 + cz * y2;

    return v3(x3, y3, z2);
}

void render3dSetCamera(Camera3D cam) { gCamera = cam; }
Camera3D render3dGetCamera(void) { return gCamera; }

static void cameraBasis(Vec3 *forward, Vec3 *right, Vec3 *upVec) {
    Vec3 f = v3(cosf(gCamera.yaw) * cosf(gCamera.pitch), sinf(gCamera.pitch), sinf(gCamera.yaw) * cosf(gCamera.pitch));
    Vec3 r = v3(cosf(gCamera.yaw + (float)M_PI * 0.5f), 0.0f, sinf(gCamera.yaw + (float)M_PI * 0.5f));
    Vec3 u = v3_cross(r, f);
    if (forward) *forward = f;
    if (right) *right = r;
    if (upVec) *upVec = u;
}

static int projectPoint(Vec3 p, SDL_FPoint *out, float *depth) {
    Vec3 forward, right, upVec;
    cameraBasis(&forward, &right, &upVec);

    Vec3 rel = v3_sub(p, gCamera.position);
    float viewX = v3_dot(rel, right);
    float viewY = v3_dot(rel, upVec);
    float viewZ = v3_dot(rel, forward);

    const float NEAR_PLANE = 0.05f;
    if (viewZ <= NEAR_PLANE) return 0;

    float aspect = (float)WINW / (float)WINH;
    float f = 1.0f / tanf(gCamera.fov * 0.5f);

    float nx = (viewX * f / aspect) / viewZ;
    float ny = (viewY * f) / viewZ;

    out->x = (nx * 0.5f + 0.5f) * WINW;
    out->y = (1.0f - (ny * 0.5f + 0.5f)) * WINH;
    if (depth) *depth = viewZ;
    return 1;
}

float render3dMeshDepth(const Mesh *mesh, Vec3 position, Vec3 rotation) {
    if (!mesh || !mesh->verts || mesh->vertCount == 0) return 0.0f;
    Vec3 forward;
    cameraBasis(&forward, NULL, NULL);

    float depthSum = 0.0f;
    for (int i = 0; i < mesh->vertCount; i++) {
        Vec3 world = rotateVector(mesh->verts[i], rotation);
        world = v3_add(world, position);
        depthSum += v3_dot(v3_sub(world, gCamera.position), forward);
    }
    return depthSum / (float)mesh->vertCount;
}

void drawMesh(SDL_Renderer *renderer, const Mesh *mesh, Vec3 position, Vec3 rotation, SDL_Color baseColor) {
    if (!renderer || !mesh || !mesh->verts || mesh->indexCount % 3 != 0) return;

    int vertTotal = mesh->indexCount;
    SDL_Vertex *verts = calloc((size_t)vertTotal, sizeof(SDL_Vertex));
    if (!verts) return;

    int v = 0;
    for (int i = 0; i < mesh->indexCount; i += 3) {
        SDL_Vertex tri[3];
        int triCount = 0;

        for (int j = 0; j < 3; j++) {
            int idx = mesh->indices ? mesh->indices[i + j] : (i + j);
            if (idx < 0 || idx >= mesh->vertCount) { continue; }

            Vec3 world = rotateVector(mesh->verts[idx], rotation);
            world = v3_add(world, position);

            float depth;
            if (!projectPoint(world, &tri[triCount].position, &depth)) { triCount = -1; break; }

            float u = (mesh->uvs && idx < mesh->vertCount) ? mesh->uvs[idx].x : 0.0f;
            float t = (mesh->uvs && idx < mesh->vertCount) ? mesh->uvs[idx].y : 0.0f;
            tri[triCount].tex_coord.x = u;
            tri[triCount].tex_coord.y = t;

            float rScale = 0.5f + u * 0.5f;
            float gScale = 0.5f + t * 0.5f;
            float bScale = 0.35f + (1.0f - (u + t) * 0.5f) * 0.35f;
            tri[triCount].color.r = (Uint8)fminf(255.0f, baseColor.r * rScale);
            tri[triCount].color.g = (Uint8)fminf(255.0f, baseColor.g * gScale);
            tri[triCount].color.b = (Uint8)fminf(255.0f, baseColor.b * bScale);
            tri[triCount].color.a = baseColor.a;
            triCount++;
        }

        if (triCount == 3) {
            for (int j = 0; j < 3; j++) {
                verts[v++] = tri[j];
            }
        }
    }

    if (v > 0) {
        SDL_RenderGeometry(renderer, NULL, verts, v, NULL, 0);
    }

    free(verts);
}
