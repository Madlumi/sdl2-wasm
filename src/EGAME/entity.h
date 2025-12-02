#ifndef EGAME_ENTITY_H
#define EGAME_ENTITY_H

#include <SDL.h>

typedef struct { float x, y, halfW, halfH; } Entity;

typedef struct {
    int current;
    int max;
} Health;

typedef struct {
    float width;
    float height;
    float yOffset;
    SDL_Color backColor;
    SDL_Color fillColor;
} HealthBar;

int entityCollidesAt(const Entity *e, float cx, float cy);
int entityMove(Entity *e, float dx, float dy);

void healthInit(Health *health, int max);
int healthIsDepleted(const Health *health);
void healthApplyDamage(Health *health, int amount);
float healthRatio(const Health *health);

void healthbarRender(const Entity *anchor, const Health *health,
                     const HealthBar *bar);

#endif
