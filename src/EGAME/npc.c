#include "npc.h"
#include "entity.h"
#include "world.h"
#include "../MENGINE/renderer.h"

static Entity npc;

void npcInit(void) {
    npc.halfW = worldTileWidth() * 0.2f;
    npc.halfH = worldTileHeight() * 0.2f;
    npc.x = worldPixelWidth() * 0.25f;
    npc.y = worldPixelHeight() * 0.25f;
}

void npcTick(double dt) { (void)dt; }

void npcRender(SDL_Renderer *r) {
    (void)r;
    SDL_Color c = {120, 180, 255, 255};
    int px = (int)(npc.x - npc.halfW);
    int py = (int)(npc.y - npc.halfH);
    int pw = (int)(npc.halfW * 2);
    int ph = (int)(npc.halfH * 2);
    drawRect(px, py, pw, ph, ANCHOR_NONE, c);
}
