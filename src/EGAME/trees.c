#include "trees.h"
#include "player.h"
#include "world.h"
#include "../MENGINE/renderer.h"
#include "../MENGINE/res.h"
#include <math.h>

typedef struct {
    Entity body;
    Health health;
    HealthBar bar;
    int lastHitSlashId;
    float shakeTimer;
    float shakeDuration;
    float shakeMagnitude;
    int falling;
    float fallTimer;
    float fallDuration;
    int woodYield;
} Tree;

static Tree trees[32];
static int treeCount = 0;

static const int treePositions[][2] = {{6, 6}, {10, 14}, {20, 8}, {14, 18}, {23, 16}};

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

static void initTree(Tree *t, float x, float y) {
    t->body.halfW = worldTileWidth() * 0.35f;
    t->body.halfH = worldTileHeight() * 0.5f;
    t->body.x = x;
    t->body.y = y;

    healthInit(&t->health, 15);
    t->bar.width = worldTileWidth() * 0.6f;
    t->bar.height = worldTileHeight() * 0.12f;
    t->bar.yOffset = worldTileHeight() * 0.05f;
    t->bar.backColor = (SDL_Color){30, 30, 30, 200};
    t->bar.fillColor = (SDL_Color){70, 180, 90, 220};

    t->lastHitSlashId = -1;
    t->shakeTimer = 0.0f;
    t->shakeDuration = 0.22f;
    t->shakeMagnitude = worldTileWidth() * 0.05f;
    t->falling = 0;
    t->fallTimer = 0.0f;
    t->fallDuration = 0.6f;
    t->woodYield = 4;
}

void treesInit(void) {
    treeCount = 0;
    int options = (int)(sizeof(treePositions) / sizeof(treePositions[0]));
    for (int i = 0; i < options && treeCount < (int)(sizeof(trees) / sizeof(trees[0])); i++) {
        int tx = treePositions[i][0];
        int ty = treePositions[i][1];
        if (worldTileSolid(tx, ty)) continue;

        float px = tx * worldTileWidth() + worldTileWidth() * 0.5f;
        float py = ty * worldTileHeight() + worldTileHeight() * 0.5f;

        initTree(&trees[treeCount], px, py);
        treeCount++;
    }
}

static void updateTree(Tree *t, double dt) {
    if (t->shakeTimer > 0.0f) {
        t->shakeTimer -= (float)dt;
        if (t->shakeTimer < 0.0f) t->shakeTimer = 0.0f;
    }

    if (t->falling) {
        t->fallTimer += (float)dt;
    }
}

void treesTick(double dt) {
    int i = 0;
    while (i < treeCount) {
        Tree *t = &trees[i];
        updateTree(t, dt);
        if (t->falling && t->fallTimer >= t->fallDuration) {
            trees[i] = trees[treeCount - 1];
            treeCount--;
            continue;
        }
        i++;
    }
}

static float shakeOffset(const Tree *t) {
    if (t->shakeTimer <= 0.0f || t->shakeDuration <= 0.0f) return 0.0f;
    float progress = 1.0f - (t->shakeTimer / t->shakeDuration);
    float wobble = sinf((t->shakeTimer) * 80.0f);
    return wobble * t->shakeMagnitude * (1.0f - progress * 0.5f);
}

void treesRender(SDL_Renderer *r) {
    (void)r;
    SDL_Texture *palm = resGetTexture("palm");
    SDL_Color trunk = {100, 70, 40, 255};
    SDL_Color leaves = {60, 160, 80, 255};

    for (int i = 0; i < treeCount; i++) {
        const Tree *t = &trees[i];
        float fallProgress = t->falling ? (t->fallTimer / t->fallDuration) : 0.0f;
        if (fallProgress > 1.0f) fallProgress = 1.0f;

        float offsetX = shakeOffset(t);
        float drop = t->falling ? fallProgress * worldTileHeight() * 0.3f : 0.0f;
        float alphaScale = t->falling ? (1.0f - fallProgress) : 1.0f;
        Uint8 alpha = (Uint8)(255 * alphaScale);

        int px = (int)(t->body.x - t->body.halfW + offsetX);
        int py = (int)(t->body.y - t->body.halfH + drop);
        int pw = (int)(t->body.halfW * 2);
        int ph = (int)(t->body.halfH * 2);

        if (palm) {
            SDL_SetTextureAlphaMod(palm, alpha);
            SDL_Rect dst = {px, py, pw, ph};
            SDL_RenderCopy(renderer, palm, NULL, &dst);
            SDL_SetTextureAlphaMod(palm, 255);
        } else {
            SDL_Color trunkShade = trunk;
            SDL_Color leafShade = leaves;
            trunkShade.a = alpha;
            leafShade.a = alpha;
            drawRect(px + pw / 3, py + ph / 3, pw / 3, ph * 2 / 3, ANCHOR_NONE, trunkShade);
            drawRect(px, py, pw, ph / 2, ANCHOR_NONE, leafShade);
        }

        if (!t->falling && !healthIsDepleted(&t->health)) {
            healthbarRender(&t->body, &t->health, &t->bar);
        }
    }
}

void treesApplySlash(const Entity *slash, int damage, int slashId) {
    if (!slash || damage <= 0) return;

    for (int i = 0; i < treeCount; i++) {
        Tree *t = &trees[i];
        if (t->falling) continue;
        if (t->lastHitSlashId == slashId) continue;
        if (!entityOverlap(&t->body, slash)) continue;

        t->lastHitSlashId = slashId;
        healthApplyDamage(&t->health, damage);
        t->shakeTimer = t->shakeDuration;
        Mix_PlayChannel(-1, resGetSound("tree_hit"), 0);

        if (healthIsDepleted(&t->health)) {
            playerAddWood(t->woodYield);
            t->falling = 1;
            t->fallTimer = 0.0f;
            Mix_PlayChannel(-1, resGetSound("tree_fall"), 0);
        }
    }
}

int treesCollidesAt(float left, float right, float top, float bottom) {
    for (int i = 0; i < treeCount; i++) {
        Tree *t = &trees[i];
        if (t->falling || healthIsDepleted(&t->health)) continue;
        float tLeft = t->body.x - t->body.halfW;
        float tRight = t->body.x + t->body.halfW;
        float tTop = t->body.y - t->body.halfH;
        float tBottom = t->body.y + t->body.halfH;
        if (left < tRight && right > tLeft && top < tBottom && bottom > tTop) {
            return 1;
        }
    }
    return 0;
}
