#ifndef RENDER3D_H
#define RENDER3D_H

#include <SDL.h>
#include "mutil.h"

// Basic vector helpers for 3D usage.
typedef struct { float x, y, z; } Vec3;
typedef struct { float u, v; } Vec2;

typedef struct {
    Vec3 position;
    Vec2 uv;
} Vertex3D;

typedef struct {
    const Vertex3D *vertices;
    const U32 *indices;
    size_t vertex_count;
    size_t index_count;
} Mesh;

// Configure camera parameters used by the 3D renderer.
void render3dSetCamera(Vec3 pos, float yaw, float pitch, float fov_deg);

// Draw a mesh with the supplied transform and tint.
void drawMesh(SDL_Renderer *renderer,
              const Mesh *mesh,
              Vec3 position,
              Vec3 rotation, // pitch (x), yaw (y), roll (z) in radians
              Vec3 scale,
              SDL_Color tint);

// Convenience constructors
static inline Vec3 v3(float x, float y, float z) { Vec3 v = {x, y, z}; return v; }
static inline Vec3 v3_add(Vec3 a, Vec3 b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
static inline Vec3 v3_sub(Vec3 a, Vec3 b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
static inline Vec3 v3_scale(Vec3 a, float s) { return v3(a.x * s, a.y * s, a.z * s); }
static inline float v3_dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

#endif
