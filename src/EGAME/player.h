#ifndef EGAME_PLAYER_H
#define EGAME_PLAYER_H
#include <SDL.h>
#include "entity.h"

typedef struct {
    Entity body;
    float speed;
    int wallBlocked;
    float facingX;
    float facingY;
    int exp;
} Player;

typedef enum {
    ITEM_NONE = 0,
    ITEM_WOOD,
} ItemType;

typedef struct {
    ItemType type;
    int count;
} ItemSlot;

#define PLAYER_HOTBAR_SLOTS 9
#define PLAYER_INVENTORY_COLS 9
#define PLAYER_INVENTORY_ROWS 3
#define PLAYER_INVENTORY_SLOTS (PLAYER_INVENTORY_COLS * PLAYER_INVENTORY_ROWS)

const Player *playerGet(void);
void playerInit(void);
void playerTick(double dt);
void playerRender(SDL_Renderer *r);
void playerAddExp(int amount);
void playerAddWood(int amount);
int playerSpendWood(int amount);
int playerGetWood(void);
int playerInventorySlotCount(void);
const ItemSlot *playerInventorySlots(void);
const ItemSlot *playerCursorSlot(void);
void playerInventoryClick(int slotIndex);
int playerInventoryIsOpen(void);
void playerToggleInventory(void);
int playerHotbarSelection(void);
void playerSetHotbarSelection(int index);
void playerClearCursor(void);
int playerCollidesAt(float left, float right, float top, float bottom,
                     const Entity *ignore);

#endif
