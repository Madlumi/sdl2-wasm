#include "entity.h"
#include "world.h"
#include "trees.h"
#include "enemy.h"
#include "player.h"
#include "../MENGINE/renderer.h"
#include <math.h>

void healthInit(Health *health, int max) {
    if (!health) return;
    health->max = max;
    health->current = max;
}

int healthIsDepleted(const Health *health) { return !health || health->current <= 0; }

void healthApplyDamage(Health *health, int amount) {
    if (!health || amount <= 0) return;
    health->current -= amount;
    if (health->current < 0) health->current = 0;
}

float healthRatio(const Health *health) {
    if (!health || health->max <= 0) return 0.0f;
    return (float)health->current / (float)health->max;
}

void healthbarRender(const Entity *anchor, const Health *health, const HealthBar *bar) {
    if (!anchor || !health || !bar) return;

    float ratio = healthRatio(health);
    float totalW = fmaxf(bar->width, 1.0f);
    float totalH = fmaxf(bar->height, 1.0f);

    float left = anchor->x - totalW * 0.5f;
    float top = anchor->y - anchor->halfH - bar->yOffset - totalH;

    SDL_Color bg = bar->backColor;
    SDL_Color fg = bar->fillColor;

    drawWorldRect(left, top, totalW, totalH, bg);
    drawWorldRect(left, top, totalW * ratio, totalH, fg);
}

static int tileSpan(float start, float end, int size, int *outMin, int *outMax) {
    *outMin = (int)floorf(start / size);
    *outMax = (int)floorf(end / size);
    return (*outMax >= *outMin);
}

int entityCollidesAt(const Entity *e, float cx, float cy) {
    const float epsilon = 0.001f;
    float left = cx - e->halfW + epsilon;
    float right = cx + e->halfW - epsilon;
    float top = cy - e->halfH + epsilon;
    float bottom = cy + e->halfH - epsilon;

    int startX, endX, startY, endY;
    tileSpan(left, right, worldTileWidth(), &startX, &endX);
    tileSpan(top, bottom, worldTileHeight(), &startY, &endY);

    for (int ty = startY; ty <= endY; ty++) {
        for (int tx = startX; tx <= endX; tx++) {
            if (worldTileSolid(tx, ty)) return 1;
        }
    }

    if (treesCollidesAt(left, right, top, bottom)) return 1;
    if (enemyCollidesAt(left, right, top, bottom, e)) return 1;
    if (playerCollidesAt(left, right, top, bottom, e)) return 1;
    return 0;
}

int entityMove(Entity *e, float dx, float dy) {
    int blocked = 0;

    if (dx != 0.0f) {
        float newX = e->x + dx;
        if (!entityCollidesAt(e, newX, e->y)) {
            e->x = newX;
        } else {
            float low = 0.0f, high = 1.0f, best = 0.0f;
            for (int i = 0; i < 12; i++) {
                float mid = (low + high) * 0.5f;
                float testX = e->x + dx * mid;
                if (!entityCollidesAt(e, testX, e->y)) {
                    best = mid;
                    low = mid;
                } else {
                    high = mid;
                }
            }

            if (best > 0.0f) e->x += dx * best;
            blocked = 1;
        }
    }

    if (dy != 0.0f) {
        float newY = e->y + dy;
        if (!entityCollidesAt(e, e->x, newY)) {
            e->y = newY;
        } else {
            float low = 0.0f, high = 1.0f, best = 0.0f;
            for (int i = 0; i < 12; i++) {
                float mid = (low + high) * 0.5f;
                float testY = e->y + dy * mid;
                if (!entityCollidesAt(e, e->x, testY)) {
                    best = mid;
                    low = mid;
                } else {
                    high = mid;
                }
            }

            if (best > 0.0f) e->y += dy * best;
            blocked = 1;
        }
    }

    return blocked;
}
