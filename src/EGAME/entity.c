#include "entity.h"
#include "world.h"
#include <math.h>

static int tileSpan(float start, float end, int size, int *outMin, int *outMax) {
    *outMin = (int)floorf(start / size);
    *outMax = (int)floorf(end / size);
    return (*outMax >= *outMin);
}

int entityCollidesAt(const Entity *e, float cx, float cy) {
    float left = cx - e->halfW;
    float right = cx + e->halfW - 1;
    float top = cy - e->halfH;
    float bottom = cy + e->halfH - 1;

    int startX, endX, startY, endY;
    tileSpan(left, right, worldTileWidth(), &startX, &endX);
    tileSpan(top, bottom, worldTileHeight(), &startY, &endY);

    for (int ty = startY; ty <= endY; ty++) {
        for (int tx = startX; tx <= endX; tx++) {
            if (worldTileSolid(tx, ty)) return 1;
        }
    }
    return 0;
}

int entityMove(Entity *e, float dx, float dy) {
    int blocked = 0;

    if (dx != 0.0f) {
        float newX = e->x + dx;
        if (!entityCollidesAt(e, newX, e->y)) {
            e->x = newX;
        } else {
            blocked = 1;
        }
    }

    if (dy != 0.0f) {
        float newY = e->y + dy;
        if (!entityCollidesAt(e, e->x, newY)) {
            e->y = newY;
        } else {
            blocked = 1;
        }
    }

    return blocked;
}
