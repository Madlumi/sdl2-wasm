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

    int hotbarSlots = 9;
    int slotSize = 40;
    int slotSpacing = 6;
    int totalW = hotbarSlots * slotSize + (hotbarSlots - 1) * slotSpacing;
    int startX = (WINW - totalW) / 2;
    int startY = WINH - slotSize - 12;

    SDL_Color slotBg = {25, 25, 25, 220};
    SDL_Color slotOutline = {90, 90, 90, 255};
    SDL_Color slotHighlight = {160, 160, 160, 255};
    SDL_Color woodColor = {140, 100, 50, 255};
    SDL_Color woodRings = {180, 140, 80, 255};
    SDL_Color woodShadow = {90, 60, 30, 255};

    int woodCount = p ? playerGetWood() : 0;
    for (int i = 0; i < hotbarSlots; i++) {
        int sx = startX + i * (slotSize + slotSpacing);
        int sy = startY;
        drawRect(sx, sy, slotSize, slotSize, ANCHOR_NONE,
                 i == 0 ? slotHighlight : slotOutline);
        drawRect(sx + 2, sy + 2, slotSize - 4, slotSize - 4, ANCHOR_NONE, slotBg);

        if (i == 0) {
            int iconLeft = sx + 6;
            int iconTop = sy + 6;
            int iconW = slotSize - 12;
            int iconH = slotSize - 14;
            drawRect(iconLeft, iconTop + 2, iconW, iconH, ANCHOR_NONE, woodShadow);
            drawRect(iconLeft + 2, iconTop, iconW - 4, iconH - 4, ANCHOR_NONE, woodColor);
            drawRect(iconLeft + 4, iconTop + iconH / 2 - 4, iconW - 8, 4, ANCHOR_NONE,
                     woodRings);
            drawRect(iconLeft + 4, iconTop + iconH / 2 + 2, iconW - 12, 3, ANCHOR_NONE,
                     woodShadow);

            drawText("default_font", sx + slotSize - 6, sy + slotSize - 6, ANCHOR_BOT_R,
                     white, "%d", woodCount);
        }
    }
}
