#include "player.h"
#include "enemy.h"
#include "trees.h"
#include "world.h"
#include "gameui.h"
#include "../MENGINE/keys.h"
#include "../MENGINE/renderer.h"
#include "../MENGINE/res.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>

typedef struct {
    Entity area;
    float timer;
    float lifetime;
    int active;
    int id;
} Slash;

typedef struct {
    int active;
    float timer;
    float duration;
    float riseSpeed;
    float baseOffset;
    char text[64];
} PlayerNotification;

static Player player;
static Slash slash;
static PlayerNotification notification;
static int slashIdCounter = 0;
static ItemSlot inventory[PLAYER_INVENTORY_SLOTS];
static ItemSlot cursorItem;
static int selectedHotbar = 0;
static int inventoryOpen = 0;

static float clampf(float v, float min, float max) {
    return v < min ? min : (v > max ? max : v);
}

const Player *playerGet(void) { return &player; }

static int maxStackFor(ItemType type) {
    switch (type) {
    case ITEM_WOOD:
        return 99;
    default:
        return 0;
    }
}

static void clearSlot(ItemSlot *slot) {
    if (!slot) return;
    slot->type = ITEM_NONE;
    slot->count = 0;
}

static void clearInventory(void) {
    for (int i = 0; i < PLAYER_INVENTORY_SLOTS; i++) clearSlot(&inventory[i]);
    clearSlot(&cursorItem);
}

static int addToStack(ItemSlot *slot, ItemType type, int amount) {
    if (!slot || amount <= 0 || type == ITEM_NONE) return amount;
    int maxStack = maxStackFor(type);
    if (slot->type != ITEM_NONE && slot->type != type) return amount;
    if (maxStack <= 0) return amount;

    if (slot->type == ITEM_NONE) slot->type = type;
    int space = maxStack - slot->count;
    if (space <= 0) return amount;

    int toAdd = amount < space ? amount : space;
    slot->count += toAdd;
    return amount - toAdd;
}

static void restack(ItemSlot *slot) {
    if (slot && slot->count <= 0) clearSlot(slot);
}

static int removeFromSlot(ItemSlot *slot, int amount) {
    if (!slot || amount <= 0 || slot->type == ITEM_NONE) return 0;
    int removed = amount < slot->count ? amount : slot->count;
    slot->count -= removed;
    restack(slot);
    return removed;
}

static int addItem(ItemType type, int amount) {
    if (type == ITEM_NONE || amount <= 0) return 0;

    int remaining = amount;
    for (int i = 0; i < PLAYER_INVENTORY_SLOTS && remaining > 0; i++) {
        if (inventory[i].type == type) remaining = addToStack(&inventory[i], type, remaining);
    }

    for (int i = 0; i < PLAYER_INVENTORY_SLOTS && remaining > 0; i++) {
        if (inventory[i].type == ITEM_NONE) remaining = addToStack(&inventory[i], type, remaining);
    }

    return amount - remaining;
}

static int removeItem(ItemType type, int amount, int preferred) {
    if (amount <= 0 || type == ITEM_NONE) return 0;
    int removed = 0;

    if (preferred >= 0 && preferred < PLAYER_INVENTORY_SLOTS && inventory[preferred].type == type) {
        removed += removeFromSlot(&inventory[preferred], amount - removed);
    }

    for (int i = 0; i < PLAYER_INVENTORY_SLOTS && removed < amount; i++) {
        if (i == preferred) continue;
        if (inventory[i].type == type) removed += removeFromSlot(&inventory[i], amount - removed);
    }
    return removed;
}

static void swapSlotWithCursor(ItemSlot *slot) {
    if (!slot) return;
    ItemSlot tmp = *slot;
    *slot = cursorItem;
    cursorItem = tmp;
    restack(slot);
    restack(&cursorItem);
}

static void mergeSlotWithCursor(ItemSlot *slot) {
    if (!slot) return;
    if (cursorItem.type == ITEM_NONE) {
        swapSlotWithCursor(slot);
        return;
    }

    if (slot->type == ITEM_NONE) {
        swapSlotWithCursor(slot);
        return;
    }

    if (slot->type == cursorItem.type) {
        int remaining = addToStack(slot, cursorItem.type, cursorItem.count);
        cursorItem.count = remaining;
        restack(&cursorItem);
    } else {
        swapSlotWithCursor(slot);
    }
}

static void playerNotification(const char *fmt, ...) {
    if (!fmt) return;
    notification.active = 1;
    notification.timer = 0.0f;
    notification.duration = 0.9f;
    notification.riseSpeed = worldTileHeight() * 0.4f;
    notification.baseOffset = worldTileHeight() * 0.2f;

    va_list args;
    va_start(args, fmt);
    vsnprintf(notification.text, sizeof(notification.text), fmt, args);
    va_end(args);
}

int playerInventorySlotCount(void) { return PLAYER_INVENTORY_SLOTS; }

const ItemSlot *playerInventorySlots(void) { return inventory; }

const ItemSlot *playerCursorSlot(void) { return &cursorItem; }

void playerInventoryClick(int slotIndex) {
    if (slotIndex < 0 || slotIndex >= PLAYER_INVENTORY_SLOTS) return;
    mergeSlotWithCursor(&inventory[slotIndex]);
}

int playerInventoryIsOpen(void) { return inventoryOpen; }

void playerToggleInventory(void) {
    inventoryOpen = !inventoryOpen;
    if (!inventoryOpen && cursorItem.type != ITEM_NONE) {
        int added = addItem(cursorItem.type, cursorItem.count);
        if (added >= cursorItem.count) {
            clearSlot(&cursorItem);
        } else {
            cursorItem.count -= added;
        }
    }
}

int playerHotbarSelection(void) { return selectedHotbar; }

void playerSetHotbarSelection(int index) {
    if (index < 0 || index >= PLAYER_HOTBAR_SLOTS) return;
    selectedHotbar = index;
}

void playerClearCursor(void) { clearSlot(&cursorItem); }

static void updateCamera(void) {
    float viewW = WINW / ZOOM;
    float viewH = WINH / ZOOM;
    XOFF = player.body.x - viewW * 0.5f;
    YOFF = player.body.y - viewH * 0.5f;
    float maxXOff = fmaxf(0.0f, worldPixelWidth() - viewW);
    float maxYOff = fmaxf(0.0f, worldPixelHeight() - viewH);
    XOFF = clampf(XOFF, 0, maxXOff);
    YOFF = clampf(YOFF, 0, maxYOff);
}

static void handleMovement(double dt) {
    float dirX = 0.0f, dirY = 0.0f;
    if (Held(INP_W)) dirY -= 1.0f;
    if (Held(INP_S)) dirY += 1.0f;
    if (Held(INP_A)) dirX -= 1.0f;
    if (Held(INP_D)) dirX += 1.0f;

    float len = sqrtf(dirX * dirX + dirY * dirY);
    if (len > 0.0f) {
        dirX /= len;
        dirY /= len;
        player.facingX = dirX;
        player.facingY = dirY;
        float stepX = dirX * player.speed * (float)dt;
        float stepY = dirY * player.speed * (float)dt;
        int blocked = entityMove(&player.body, stepX, stepY);
        if (blocked && !player.wallBlocked) {
            Mix_PlayChannel(-1, resGetSound("bump"), 0);
            player.wallBlocked = 1;
        } else if (!blocked) {
            player.wallBlocked = 0;
        }
    } else {
        player.wallBlocked = 0;
    }
}

static void handlePlacement(int uiBlocked) {
    if (!Pressed(INP_RCLICK) || playerGetWood() <= 0 || uiBlocked) return;

    double wx = 0.0, wy = 0.0;
    screenToWorld(mpos.x, mpos.y, &wx, &wy);

    int tx = (int)floor(wx / worldTileWidth());
    int ty = (int)floor(wy / worldTileHeight());

    if (worldTileSolid(tx, ty)) return;

    float left = tx * worldTileWidth();
    float right = left + worldTileWidth();
    float top = ty * worldTileHeight();
    float bottom = top + worldTileHeight();

    if (treesCollidesAt(left, right, top, bottom)) return;
    if (enemyCollidesAt(left, right, top, bottom, NULL)) return;
    if (playerCollidesAt(left, right, top, bottom, &player.body)) return;

    if (worldPlaceWoodTile(tx, ty) && playerSpendWood(1)) {
        Mix_PlayChannel(-1, resGetSound("bump"), 0);
    }
}

static void findSpawn(void) {
    for (int y = 1; y < worldMapHeight() - 1; y++) {
        for (int x = 1; x < worldMapWidth() - 1; x++) {
            if (!worldTileSolid(x, y)) {
                player.body.x = x * worldTileWidth() + worldTileWidth() * 0.5f;
                player.body.y = y * worldTileHeight() + worldTileHeight() * 0.5f;
                float left = player.body.x - player.body.halfW;
                float right = player.body.x + player.body.halfW;
                float top = player.body.y - player.body.halfH;
                float bottom = player.body.y + player.body.halfH;
                if (!treesCollidesAt(left, right, top, bottom)) return;
            }
        }
    }
}

void playerInit(void) {
    player.speed = 120.0f;
    player.body.halfW = worldTileWidth() * 0.3f;
    player.body.halfH = worldTileHeight() * 0.3f;
    player.wallBlocked = 0;
    player.facingX = 0.0f;
    player.facingY = 1.0f;
    player.exp = 0;
    clearInventory();
    selectedHotbar = 0;
    inventoryOpen = 0;
    slash.active = 0;
    notification.active = 0;
    findSpawn();
    updateCamera();
}

void playerTick(double dt) {
    if (Pressed(INP_TAB)) playerToggleInventory();

    handleMovement(dt);
    int uiHover = gameuiMouseOverUI();
    handlePlacement(playerInventoryIsOpen() && uiHover);
    if (slash.active) {
        slash.timer += (float)dt;
        enemyApplySlash(&slash.area, 6, slash.id);
        treesApplySlash(&slash.area, 6, slash.id);
        if (slash.timer >= slash.lifetime) slash.active = 0;
    }

    if (!gameuiClickConsumed() && !playerInventoryIsOpen() && Pressed(INP_LCLICK)) {
        float dirX = player.facingX;
        float dirY = player.facingY;
        if (dirX == 0.0f && dirY == 0.0f) dirY = 1.0f;

        float range = worldTileWidth() * 0.8f;
        slash.area.halfW = (fabsf(dirX) > fabsf(dirY)) ? range * 0.55f
                                                      : worldTileWidth() * 0.35f;
        slash.area.halfH = (fabsf(dirX) > fabsf(dirY)) ? worldTileHeight() * 0.25f
                                                      : range * 0.55f;

        slash.area.x = player.body.x + dirX * (player.body.halfW + slash.area.halfW);
        slash.area.y = player.body.y + dirY * (player.body.halfH + slash.area.halfH);
        slash.timer = 0.0f;
        slash.lifetime = 0.18f;
        slash.active = 1;
        slash.id = ++slashIdCounter;
        enemyApplySlash(&slash.area, 6, slash.id);
        treesApplySlash(&slash.area, 6, slash.id);
    }

    if (notification.active) {
        notification.timer += (float)dt;
        if (notification.timer >= notification.duration) notification.active = 0;
    }
    updateCamera();
}

void playerRender(SDL_Renderer *r) {
    (void)r;
    SDL_Color c = {255, 240, 120, 255};
    float px = player.body.x - player.body.halfW;
    float py = player.body.y - player.body.halfH;
    float pw = player.body.halfW * 2;
    float ph = player.body.halfH * 2;
    drawWorldRect(px, py, pw, ph, c);

    if (slash.active) {
        SDL_Color slashColor = {220, 220, 255, 180};
        float sx = slash.area.x - slash.area.halfW;
        float sy = slash.area.y - slash.area.halfH;
        float sw = slash.area.halfW * 2;
        float sh = slash.area.halfH * 2;
        drawWorldRect(sx, sy, sw, sh, slashColor);
    }

    if (notification.active) {
        float alphaRatio = 1.0f - (notification.timer / notification.duration);
        if (alphaRatio < 0.0f) alphaRatio = 0.0f;
        SDL_Color textColor = {255, 255, 255, (Uint8)(255 * alphaRatio)};
        float rise = notification.baseOffset + notification.riseSpeed * notification.timer;
        float tx = player.body.x;
        float ty = player.body.y - player.body.halfH - rise;
        drawWorldTextScaled("default_font", tx, ty, ANCHOR_NONE, textColor, 0.25f,
                            "%s", notification.text);
    }
}

void playerAddExp(int amount) {
    if (amount <= 0) return;
    player.exp += amount;
    playerNotification("+%d xp", amount);
}

void playerAddWood(int amount) {
    if (amount <= 0) return;
    int added = addItem(ITEM_WOOD, amount);
    if (added > 0) playerNotification("+%d wood", added);
}

int playerSpendWood(int amount) {
    if (amount <= 0 || playerGetWood() < amount) return 0;
    int removed = removeItem(ITEM_WOOD, amount, selectedHotbar);
    return removed >= amount;
}

int playerCollidesAt(float left, float right, float top, float bottom,
                     const Entity *ignore) {
    if (&player.body == ignore) return 0;

    float pLeft = player.body.x - player.body.halfW;
    float pRight = player.body.x + player.body.halfW;
    float pTop = player.body.y - player.body.halfH;
    float pBottom = player.body.y + player.body.halfH;

    return left < pRight && right > pLeft && top < pBottom && bottom > pTop;
}

int playerGetWood(void) {
    int total = 0;
    for (int i = 0; i < PLAYER_INVENTORY_SLOTS; i++) {
        if (inventory[i].type == ITEM_WOOD) total += inventory[i].count;
    }
    return total;
}
