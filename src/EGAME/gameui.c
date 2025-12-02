#include "gameui.h"
#include "player.h"
#include "../MENGINE/renderer.h"

void gameuiRender(SDL_Renderer *r) {
    (void)r;
    SDL_Color white = {255, 255, 255, 255};
    drawText("default_font", 10, 10, ANCHOR_TOP_L, white,
             "WASD to move. Left click to slash.");
    drawText("default_font", WINW - 10, 10, ANCHOR_TOP_R, white,
             "Zoom %.1fx", ZOOM);

    const Player *p = playerGet();
    int cycleExp = p ? (p->exp % 10) : 0;
    float ratio = (float)cycleExp / 10.0f;
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;

    int barW = 200;
    int barH = 12;
    int margin = 14;

    SDL_Color back = {25, 50, 70, 220};
    SDL_Color fill = {80, 180, 255, 240};

    drawRect(0, margin, barW, barH, ANCHOR_MID_BOT, back);
    drawRect(0 + 2, margin + 2, (int)((barW - 4) * ratio), barH - 4, ANCHOR_MID_BOT,
             fill);
}
