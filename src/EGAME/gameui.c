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

    int hotbarSlots = 5;
    int slotSize = 36;
    int slotSpacing = 6;
    int totalW = hotbarSlots * slotSize + (hotbarSlots - 1) * slotSpacing;
    int startX = (WINW - totalW) / 2;
    int startY = WINH - slotSize - 12;

    SDL_Color slotBg = {25, 25, 25, 220};
    SDL_Color slotOutline = {90, 90, 90, 255};
    SDL_Color woodColor = {130, 90, 40, 255};

    int woodCount = p ? playerGetWood() : 0;
    for (int i = 0; i < hotbarSlots; i++) {
        int sx = startX + i * (slotSize + slotSpacing);
        int sy = startY;
        drawRect(sx, sy, slotSize, slotSize, ANCHOR_NONE, slotOutline);
        drawRect(sx + 2, sy + 2, slotSize - 4, slotSize - 4, ANCHOR_NONE, slotBg);

        if (i == 0) {
            drawRect(sx + 6, sy + 10, slotSize - 12, slotSize - 18, ANCHOR_NONE,
                     woodColor);
            drawText("default_font", sx + slotSize - 6, sy + slotSize - 6, ANCHOR_BOT_R,
                     white, "%d", woodCount);
            drawText("default_font", sx + slotSize / 2, sy - 2, ANCHOR_MID_BOT, white,
                     "Wood");
        }
    }
}
