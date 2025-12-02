#include "enemy.h"
#include "entity.h"
#include "player.h"
#include "world.h"
#include "../MENGINE/renderer.h"
#include <math.h>
#include <stdlib.h>

typedef struct {
    Entity body;
    Health health;
    HealthBar bar;
    int lastHitSlashId;
} Enemy;

static Enemy enemies[64];
static int enemyCount = 0;
static int spawnTick = 0;
static float chaseSpeed = 80.0f;
static const int spawnIntervalTicks = 180;
static const int spawnDistanceTiles = 6;

static float distanceSq(float ax, float ay, float bx, float by) {
    float dx = ax - bx;
    float dy = ay - by;
    return dx * dx + dy * dy;
}

static int entityOverlap(const Entity *a, const Entity *b) {
    float leftA = a->x - a->halfW;
    float rightA = a->x + a->halfW;
    float topA = a->y - a->halfH;
    float bottomA = a->y + a->halfH;

    float leftB = b->x - b->halfW;
    float rightB = b->x + b->halfW;
    float topB = b->y - b->halfH;
    float bottomB = b->y + b->halfH;

    return leftA < rightB && rightA > leftB && topA < bottomB && bottomA > topB;
}

static void initEnemy(Enemy *e, float x, float y) {
    e->body.halfW = worldTileWidth() * 0.25f;
    e->body.halfH = worldTileHeight() * 0.25f;
    e->body.x = x;
    e->body.y = y;

    healthInit(&e->health, 10);
    e->bar.width = worldTileWidth() * 0.6f;
    e->bar.height = worldTileHeight() * 0.12f;
    e->bar.yOffset = worldTileHeight() * 0.1f;
    e->bar.backColor = (SDL_Color){20, 20, 20, 255};
    e->bar.fillColor = (SDL_Color){200, 60, 60, 255};
    e->lastHitSlashId = -1;
}

static int findSpawn(float *outX, float *outY) {
    const Player *p = playerGet();
    float requiredDistSq = (float)(spawnDistanceTiles * spawnDistanceTiles *
                                   worldTileWidth() * worldTileWidth());
    Entity probe = {.halfW = worldTileWidth() * 0.25f,
                    .halfH = worldTileHeight() * 0.25f};

    for (int attempt = 0; attempt < 64; attempt++) {
        int tx = rand() % worldMapWidth();
        int ty = rand() % worldMapHeight();
        if (worldTileSolid(tx, ty)) continue;

        float px = tx * worldTileWidth() + worldTileWidth() * 0.5f;
        float py = ty * worldTileHeight() + worldTileHeight() * 0.5f;

        probe.x = px;
        probe.y = py;
        if (entityCollidesAt(&probe, px, py)) continue;
        if (distanceSq(px, py, p->body.x, p->body.y) < requiredDistSq) continue;

        *outX = px;
        *outY = py;
        return 1;
    }
    return 0;
}

static void spawnEnemy(void) {
    if (enemyCount >= (int)(sizeof(enemies) / sizeof(enemies[0]))) return;
    float x = worldPixelWidth() * 0.8f;
    float y = worldPixelHeight() * 0.8f;

    if (!findSpawn(&x, &y)) return;

    initEnemy(&enemies[enemyCount], x, y);
    enemyCount++;
}

void enemyInit(void) {
    enemyCount = 0;
    spawnTick = 0;
    srand((unsigned int)SDL_GetTicks());
    spawnEnemy();
}

static void updateEnemy(Enemy *e, double dt) {
    const Player *p = playerGet();
    float dirX = p->body.x - e->body.x;
    float dirY = p->body.y - e->body.y;
    float len = sqrtf(dirX * dirX + dirY * dirY);
    if (len > 1.0f) {
        dirX /= len;
        dirY /= len;
        (void)entityMove(&e->body, dirX * chaseSpeed * (float)dt,
                         dirY * chaseSpeed * (float)dt);
    }
}

void enemyTick(double dt) {
    for (int i = 0; i < enemyCount; i++) {
        updateEnemy(&enemies[i], dt);
    }

    spawnTick++;
    if (spawnTick >= spawnIntervalTicks) {
        spawnTick = 0;
        spawnEnemy();
    }
}

void enemyRender(SDL_Renderer *r) {
    (void)r;
    SDL_Color c = {220, 80, 80, 255};
    for (int i = 0; i < enemyCount; i++) {
        Enemy *e = &enemies[i];
        float px = e->body.x - e->body.halfW;
        float py = e->body.y - e->body.halfH;
        float pw = e->body.halfW * 2;
        float ph = e->body.halfH * 2;
        drawWorldRect(px, py, pw, ph, c);
        healthbarRender(&e->body, &e->health, &e->bar);
    }
}

void enemyApplySlash(const Entity *slash, int damage, int slashId) {
    if (!slash || damage <= 0) return;
    int i = 0;
    while (i < enemyCount) {
        Enemy *e = &enemies[i];
        if (e->lastHitSlashId != slashId && entityOverlap(&e->body, slash)) {
            e->lastHitSlashId = slashId;
            healthApplyDamage(&e->health, damage);
            if (healthIsDepleted(&e->health)) {
                playerAddExp(1);
                enemies[i] = enemies[enemyCount - 1];
                enemyCount--;
                continue;
            }
        }
        i++;
    }
}
