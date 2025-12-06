#include "gameui.h"
#include "player.h"
#include "../MENGINE/renderer.h"
#include "../MENGINE/res.h"
#include "../MENGINE/keys.h"
#include <SDL_ttf.h>
#include <stdio.h>

typedef struct {
    int startX;
    int startY;
    int slotSize;
    int spacing;
    int totalW;
} HotbarLayout;

static int clickConsumed = 0;
static int hoverUI = 0;
static const int INVENTORY_MARGIN = 40;

static HotbarLayout hotbarLayout(void) {
    HotbarLayout l;
    l.slotSize = 40;
    l.spacing = 6;
    l.totalW = PLAYER_HOTBAR_SLOTS * l.slotSize + (PLAYER_HOTBAR_SLOTS - 1) * l.spacing;
    l.startX = (WINW - l.totalW) / 2;
    l.startY = WINH - l.slotSize - 12;
    return l;
}

static int inventoryStartY(const HotbarLayout *l) {
    return l->startY - PLAYER_INVENTORY_ROWS * (l->slotSize + l->spacing) - INVENTORY_MARGIN;
}

static SDL_Rect slotRect(int startX, int startY, int slotSize, int spacing, int index,
                         int cols) {
    int col = index % cols;
    int row = index / cols;
    SDL_Rect r = {startX + col * (slotSize + spacing), startY + row * (slotSize + spacing),
                  slotSize, slotSize};
    return r;
}

static SDL_Rect hotbarSlotRect(int index) {
    HotbarLayout l = hotbarLayout();
    return slotRect(l.startX, l.startY, l.slotSize, l.spacing, index, PLAYER_HOTBAR_SLOTS);
}

static SDL_Rect inventorySlotRect(int index) {
    HotbarLayout l = hotbarLayout();
    int startY = inventoryStartY(&l);
    return slotRect(l.startX, startY, l.slotSize, l.spacing, index, PLAYER_INVENTORY_COLS);
}

static int pointInRect(SDL_Rect r, int x, int y) {
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

static int hotbarSlotAt(int mx, int my) {
    for (int i = 0; i < PLAYER_HOTBAR_SLOTS; i++) {
        if (pointInRect(hotbarSlotRect(i), mx, my)) return i;
    }
    return -1;
}

static int inventorySlotAt(int mx, int my) {
    if (!playerInventoryIsOpen()) return -1;
    int total = playerInventorySlotCount();
    for (int i = 0; i < total; i++) {
        if (pointInRect(inventorySlotRect(i), mx, my)) return i;
    }
    return -1;
}

static void drawWoodIcon(int sx, int sy, int slotSize, SDL_Color textColor, int count) {
    SDL_Color woodColor = {140, 100, 50, 255};
    SDL_Color woodRings = {180, 140, 80, 255};
    SDL_Color woodShadow = {90, 60, 30, 255};

    int iconLeft = sx + 6;
    int iconTop = sy + 6;
    int iconW = slotSize - 12;
    int iconH = slotSize - 14;
    drawRect(iconLeft, iconTop + 2, iconW, iconH, ANCHOR_TOP_L, woodShadow);
    drawRect(iconLeft + 2, iconTop, iconW - 4, iconH - 4, ANCHOR_TOP_L, woodColor);
    drawRect(iconLeft + 4, iconTop + iconH / 2 - 4, iconW - 8, 4, ANCHOR_TOP_L, woodRings);
    drawRect(iconLeft + 4, iconTop + iconH / 2 + 2, iconW - 12, 3, ANCHOR_TOP_L,
             woodShadow);

    char woodBuf[32];
    snprintf(woodBuf, sizeof(woodBuf), "%d", count);
    int woodTextW = 0;
    int woodTextH = 0;
    TTF_Font *woodFont = resGetFont("default_font");
    if (woodFont && TTF_SizeUTF8(woodFont, woodBuf, &woodTextW, &woodTextH) != 0) {
        woodTextW = woodTextH = 0;
    }
    int textX = sx + slotSize - 6 - woodTextW;
    int textY = sy + slotSize - 6 - woodTextH;
    drawText("default_font", textX, textY, ANCHOR_TOP_L, textColor, "%s", woodBuf);
}

static void drawSlotContents(const ItemSlot *slot, int sx, int sy, int slotSize,
                             SDL_Color textColor) {
    if (!slot || slot->type == ITEM_NONE || slot->count <= 0) return;

    switch (slot->type) {
    case ITEM_WOOD:
        drawWoodIcon(sx, sy, slotSize, textColor, slot->count);
        break;
    default:
        break;
    }
}

int gameuiClickConsumed(void) { return clickConsumed; }

int gameuiMouseOverUI(void) { return hoverUI; }

void gameuiTick(double dt) {
    (void)dt;
    clickConsumed = 0;
    hoverUI = 0;

    int mx = mpos.x;
    int my = mpos.y;
    int hbIndex = hotbarSlotAt(mx, my);
    int invIndex = inventorySlotAt(mx, my);
    hoverUI = (hbIndex >= 0) || (invIndex >= 0);

    if (Pressed(INP_LCLICK)) {
        int consumedThisClick = 0;
        if (playerInventoryIsOpen() && invIndex >= 0) {
            playerInventoryClick(invIndex);
            consumedThisClick = 1;
        }

        if (hbIndex >= 0) {
            playerSetHotbarSelection(hbIndex);
            consumedThisClick = 1;
            if (playerInventoryIsOpen() && invIndex < 0) {
                playerInventoryClick(hbIndex);
            }
        }
        clickConsumed = consumedThisClick;
    }
}

void gameuiRender(SDL_Renderer *r) {
    (void)r;
    SDL_Color white = {255, 255, 255, 255};
    HotbarLayout l = hotbarLayout();

    drawText("default_font", 10, 10, ANCHOR_TOP_L, white,
             "WASD to move. Left click to slash.");
    drawText("default_font", WINW - 10, 10, ANCHOR_TOP_R, white,
             "Zoom %.1fx", ZOOM);

    const Player *p = playerGet();
    int cycleExp = p ? (p->exp % 10) : 0;
    float ratio = (float)cycleExp / 10.0f;
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;

    int barW = l.totalW;
    int barH = 12;
    int barY = l.startY - barH - 12;

    SDL_Color back = {25, 50, 70, 220};
    SDL_Color fill = {80, 180, 255, 240};

    drawRect(l.startX, barY, barW, barH, ANCHOR_TOP_L, back);
    drawRect(l.startX + 2, barY + 2, (int)((barW - 4) * ratio), barH - 4, ANCHOR_TOP_L,
             fill);

    SDL_Color slotBg = {25, 25, 25, 220};
    SDL_Color slotOutline = {90, 90, 90, 255};
    SDL_Color slotHighlight = {160, 160, 160, 255};

    const ItemSlot *slots = playerInventorySlots();
    for (int i = 0; i < PLAYER_HOTBAR_SLOTS; i++) {
        SDL_Rect rect = hotbarSlotRect(i);
        SDL_Color border = (i == playerHotbarSelection()) ? slotHighlight : slotOutline;
        drawRect(rect.x, rect.y, rect.w, rect.h, ANCHOR_TOP_L, border);
        drawRect(rect.x + 2, rect.y + 2, rect.w - 4, rect.h - 4, ANCHOR_TOP_L, slotBg);

        drawSlotContents(&slots[i], rect.x, rect.y, l.slotSize, white);
    }

    if (playerInventoryIsOpen()) {
        int invStartY = inventoryStartY(&l);
        int invTotalH = PLAYER_INVENTORY_ROWS * l.slotSize +
                        (PLAYER_INVENTORY_ROWS - 1) * l.spacing;
        SDL_Color panel = {10, 10, 15, 200};
        drawRect(l.startX - 12, invStartY - 14, l.totalW + 24, invTotalH + 28,
                 ANCHOR_TOP_L, panel);

        int hoveredSlot = inventorySlotAt(mpos.x, mpos.y);
        for (int i = 0; i < PLAYER_INVENTORY_SLOTS; i++) {
            SDL_Rect rect = inventorySlotRect(i);
            SDL_Color border = slotOutline;
            if (i == playerHotbarSelection()) border = slotHighlight;
            if (hoveredSlot == i) border = slotHighlight;
            drawRect(rect.x, rect.y, rect.w, rect.h, ANCHOR_TOP_L, border);
            drawRect(rect.x + 2, rect.y + 2, rect.w - 4, rect.h - 4, ANCHOR_TOP_L,
                     slotBg);
            drawSlotContents(&slots[i], rect.x, rect.y, l.slotSize, white);
        }

        const ItemSlot *cursor = playerCursorSlot();
        if (cursor && cursor->type != ITEM_NONE && cursor->count > 0) {
            int cx = mpos.x - l.slotSize / 2;
            int cy = mpos.y - l.slotSize / 2;
            SDL_Color cursorBorder = slotHighlight;
            drawRect(cx, cy, l.slotSize, l.slotSize, ANCHOR_TOP_L, cursorBorder);
            drawRect(cx + 2, cy + 2, l.slotSize - 4, l.slotSize - 4, ANCHOR_TOP_L,
                     slotBg);
            drawSlotContents(cursor, cx, cy, l.slotSize, white);
        }
    }
}
