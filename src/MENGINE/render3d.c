#include <math.h>
#include <stdlib.h>
#include "render3d.h"
#include "renderer.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static Vec3 camPos = {0.0f, 0.0f, 0.0f};
static float camYaw = 0.0f;
static float camPitch = 0.0f;
static float camFov = 70.0f;
static Vec3 camForward = {0.0f, 0.0f, 1.0f};
static Vec3 camRight = {1.0f, 0.0f, 0.0f};
static Vec3 camUp = {0.0f, 1.0f, 0.0f};

static float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

static Vec3 v3_cross(Vec3 a, Vec3 b) {
    Vec3 v = {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
    return v;
}

static Vec3 v3_mul(Vec3 a, Vec3 b) { return v3(a.x * b.x, a.y * b.y, a.z * b.z); }

void render3dSetCamera(Vec3 pos, float yaw, float pitch, float fov_deg) {
    camPos = pos;
    camYaw = yaw;
    camPitch = pitch;
    camFov = fov_deg;

    camForward = v3(cosf(camYaw) * cosf(camPitch), sinf(camPitch), sinf(camYaw) * cosf(camPitch));
    camRight = v3(cosf(camYaw + (float)M_PI * 0.5f), 0.0f, sinf(camYaw + (float)M_PI * 0.5f));
    camUp = v3_cross(camRight, camForward);
}

static Vec3 applyRotation(Vec3 v, Vec3 rot) {
    float cx = cosf(rot.x), sx = sinf(rot.x);
    float cy = cosf(rot.y), sy = sinf(rot.y);
    float cz = cosf(rot.z), sz = sinf(rot.z);

    // Yaw (Y axis)
    float nx = v.x * cy + v.z * sy;
    float nz = -v.x * sy + v.z * cy;
    float ny = v.y;

    // Pitch (X axis)
    float py = ny * cx - nz * sx;
    float pz = ny * sx + nz * cx;
    float px = nx;

    // Roll (Z axis)
    float rx = px * cz - py * sz;
    float ry = px * sz + py * cz;
    float rz = pz;

    return v3(rx, ry, rz);
}

static int projectPoint(Vec3 world, SDL_FPoint *out, float *depth) {
    Vec3 rel = v3_sub(world, camPos);
    float viewX = v3_dot(rel, camRight);
    float viewY = v3_dot(rel, camUp);
    float viewZ = v3_dot(rel, camForward);

    const float NEAR_PLANE = 0.05f;
    if (viewZ <= NEAR_PLANE) return 0;

    float aspect = (float)WINW / (float)WINH;
    float f = 1.0f / tanf((camFov * (float)M_PI / 180.0f) * 0.5f);
    float nx = (viewX * f / aspect) / viewZ;
    float ny = (viewY * f) / viewZ;

    out->x = (nx * 0.5f + 0.5f) * WINW;
    out->y = (1.0f - (ny * 0.5f + 0.5f)) * WINH;
    if (depth) *depth = viewZ;
    return 1;
}

void drawMesh(SDL_Renderer *renderer,
              const Mesh *mesh,
              Vec3 position,
              Vec3 rotation,
              Vec3 scale,
              SDL_Color tint) {
    if (!renderer || !mesh || !mesh->vertices || !mesh->indices) return;

    SDL_Vertex *triVerts = malloc(sizeof(SDL_Vertex) * mesh->index_count);
    if (!triVerts) return;

    for (size_t i = 0; i < mesh->index_count; i++) {
        U32 idx = mesh->indices[i];
        if (idx >= mesh->vertex_count) { free(triVerts); return; }

        Vertex3D v = mesh->vertices[idx];
        Vec3 transformed = v3_mul(v.position, scale);
        transformed = applyRotation(transformed, rotation);
        transformed = v3_add(transformed, position);

        SDL_FPoint proj;
        float depth;
        if (!projectPoint(transformed, &proj, &depth)) { free(triVerts); return; }

        float gradientR = clamp01(v.uv.u) * 255.0f;
        float gradientG = clamp01(v.uv.v) * 255.0f;
        float gradientB = (1.0f - clamp01((v.uv.u + v.uv.v) * 0.5f)) * 255.0f;
        float depthShade = 1.1f / (0.4f + depth);
        if (depthShade > 1.0f) depthShade = 1.0f;

        SDL_Color c;
        c.r = (Uint8)fminf(255.0f, gradientR * (tint.r / 255.0f) * depthShade);
        c.g = (Uint8)fminf(255.0f, gradientG * (tint.g / 255.0f) * depthShade);
        c.b = (Uint8)fminf(255.0f, gradientB * (tint.b / 255.0f) * depthShade);
        c.a = tint.a;

        triVerts[i].position = proj;
        triVerts[i].color = c;
        triVerts[i].tex_coord.x = v.uv.u;
        triVerts[i].tex_coord.y = v.uv.v;
    }

    SDL_RenderGeometry(renderer, NULL, triVerts, (int)mesh->index_count, NULL, 0);
    free(triVerts);
}
