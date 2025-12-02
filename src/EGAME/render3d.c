#include "render3d.h"
#include "../MENGINE/renderer.h"
#include <math.h>
#include <stdlib.h>

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

void drawMesh(SDL_Renderer *renderer, const Mesh *mesh, Vec3 position, Vec3 rotation, SDL_Color baseColor, SDL_Texture *texture) {
    if (!renderer || !mesh || !mesh->verts || mesh->indexCount % 3 != 0) return;

    int vertTotal = mesh->indexCount * 2; // allow room for clipped triangles
    SDL_Vertex *verts = calloc((size_t)vertTotal, sizeof(SDL_Vertex));
    if (!verts) return;

    Vec3 forward, right, upVec;
    cameraBasis(&forward, &right, &upVec);
    const float NEAR_PLANE = 0.05f;
    float aspect = (float)WINW / (float)WINH;
    float f = 1.0f / tanf(gCamera.fov * 0.5f);

    int v = 0;
    for (int i = 0; i < mesh->indexCount; i += 3) {
        typedef struct { float x, y, z, u, t; } ViewVert;
        ViewVert in[3];
        int inCount = 0;

        for (int j = 0; j < 3; j++) {
            int idx = mesh->indices ? mesh->indices[i + j] : (i + j);
            if (idx < 0 || idx >= mesh->vertCount) { continue; }

            Vec3 world = rotateVector(mesh->verts[idx], rotation);
            world = v3_add(world, position);

            float u = (mesh->uvs && idx < mesh->vertCount) ? mesh->uvs[idx].x : 0.0f;
            float t = (mesh->uvs && idx < mesh->vertCount) ? mesh->uvs[idx].y : 0.0f;
            Vec3 rel = v3_sub(world, gCamera.position);
            in[inCount].x = v3_dot(rel, right);
            in[inCount].y = v3_dot(rel, upVec);
            in[inCount].z = v3_dot(rel, forward);
            in[inCount].u = u;
            in[inCount].t = t;
            inCount++;
        }

        if (inCount < 3) continue;

        ViewVert clipped[6];
        int clipCount = 0;

        for (int j = 0; j < inCount; j++) {
            ViewVert cur = in[j];
            ViewVert prev = in[(j + inCount - 1) % inCount];
            int curInside = cur.z >= NEAR_PLANE;
            int prevInside = prev.z >= NEAR_PLANE;

            if (curInside != prevInside) {
                float t = (NEAR_PLANE - prev.z) / (cur.z - prev.z);
                ViewVert inter;
                inter.x = prev.x + (cur.x - prev.x) * t;
                inter.y = prev.y + (cur.y - prev.y) * t;
                inter.z = NEAR_PLANE;
                inter.u = prev.u + (cur.u - prev.u) * t;
                inter.t = prev.t + (cur.t - prev.t) * t;
                if (clipCount < (int)(sizeof(clipped) / sizeof(clipped[0]))) clipped[clipCount++] = inter;
            }

            if (curInside) {
                if (clipCount < (int)(sizeof(clipped) / sizeof(clipped[0]))) clipped[clipCount++] = cur;
            }
        }

        if (clipCount < 3) continue;

        for (int j = 1; j < clipCount - 1; j++) {
            ViewVert tri[3] = {clipped[0], clipped[j], clipped[j + 1]};
            for (int k = 0; k < 3; k++) {
                float uWrap = tri[k].u - floorf(tri[k].u);
                float tWrap = tri[k].t - floorf(tri[k].t);
                if (uWrap == 0.0f && tri[k].u > 0.0f) uWrap = 1.0f;
                if (tWrap == 0.0f && tri[k].t > 0.0f) tWrap = 1.0f;

                float nx = (tri[k].x * f / aspect) / tri[k].z;
                float ny = (tri[k].y * f) / tri[k].z;

                verts[v].position.x = (nx * 0.5f + 0.5f) * WINW;
                verts[v].position.y = (1.0f - (ny * 0.5f + 0.5f)) * WINH;
                verts[v].tex_coord.x = uWrap;
                verts[v].tex_coord.y = tWrap;

                if (texture) {
                    verts[v].color = baseColor;
                } else {
                    float rScale = 0.5f + uWrap * 0.5f;
                    float gScale = 0.5f + tWrap * 0.5f;
                    float bScale = 0.35f + (1.0f - (uWrap + tWrap) * 0.5f) * 0.35f;
                    verts[v].color.r = (Uint8)fminf(255.0f, baseColor.r * rScale);
                    verts[v].color.g = (Uint8)fminf(255.0f, baseColor.g * gScale);
                    verts[v].color.b = (Uint8)fminf(255.0f, baseColor.b * bScale);
                    verts[v].color.a = baseColor.a;
                }
                v++;
            }
        }
    }

    if (v > 0) {
        SDL_RenderGeometry(renderer, texture, verts, v, NULL, 0);
    }

    free(verts);
}

void render3dInitQuadMeshUV(MeshInstance *inst, Vec3 v0, Vec3 v1, Vec3 v2, Vec3 v3p, SDL_Color color, SDL_FPoint uv0, SDL_FPoint uv1, SDL_FPoint uv2, SDL_FPoint uv3) {
    if (!inst) return;
    inst->verts[0] = v0;
    inst->verts[1] = v1;
    inst->verts[2] = v2;
    inst->verts[3] = v3p;
    inst->uvs[0] = uv0;
    inst->uvs[1] = uv1;
    inst->uvs[2] = uv2;
    inst->uvs[3] = uv3;
    inst->indices[0] = 0; inst->indices[1] = 1; inst->indices[2] = 2;
    inst->indices[3] = 0; inst->indices[4] = 2; inst->indices[5] = 3;
    inst->mesh.verts = inst->verts;
    inst->mesh.uvs = inst->uvs;
    inst->mesh.vertCount = 4;
    inst->mesh.indices = inst->indices;
    inst->mesh.indexCount = 6;
    inst->color = color;
    inst->position = v3(0.0f, 0.0f, 0.0f);
    inst->rotation = v3(0.0f, 0.0f, 0.0f);
    inst->texture = NULL;
}

void render3dInitQuadMesh(MeshInstance *inst, Vec3 v0, Vec3 v1, Vec3 v2, Vec3 v3, SDL_Color color) {
    render3dInitQuadMeshUV(inst, v0, v1, v2, v3, color,
                           (SDL_FPoint){0.0f, 0.0f}, (SDL_FPoint){1.0f, 0.0f}, (SDL_FPoint){1.0f, 1.0f}, (SDL_FPoint){0.0f, 1.0f});
}

int render3dCompareFaceDepth(const void *a, const void *b) {
    float da = ((const FaceDepth *)a)->depth;
    float db = ((const FaceDepth *)b)->depth;
    if (da < db) return 1;
    if (da > db) return -1;
    return 0;
}
