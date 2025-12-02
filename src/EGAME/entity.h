#ifndef EGAME_ENTITY_H
#define EGAME_ENTITY_H

typedef struct { float x, y, halfW, halfH; } Entity;

int entityCollidesAt(const Entity *e, float cx, float cy);
int entityMove(Entity *e, float dx, float dy);

#endif
