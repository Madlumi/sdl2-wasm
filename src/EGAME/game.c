#include "game.h"
#include "../MENGINE/renderer.h"
#include "../MENGINE/keys.h"
#include "../MENGINE/tick.h"
#include <SDL.h>

#define TILE 32
#define MAP_W 16
#define MAP_H 16
#define GRAVITY 0.5f
#define SPEED 2.0f
#define JUMP_VEL 10.0f

typedef struct {
    float x, y;
    float vx, vy;
    int w, h;
    int onGround;
} Player;

static Player player;
static SDL_Rect blocks[4];
static int block_count = 0;
static const int ground_y = (MAP_H - 1) * TILE;

static void gameTickInternal(void) {
    if (Held(INP_A)) player.vx = -SPEED; 
    else if (Held(INP_D)) player.vx = SPEED; 
    else player.vx = 0;

    if (Pressed(INP_W) && player.onGround) {
        player.vy = -JUMP_VEL;
        player.onGround = 0;
    }

    player.vy += GRAVITY;

    player.x += player.vx;
    player.y += player.vy;

    if (player.y + player.h > ground_y) {
        player.y = ground_y - player.h;
        player.vy = 0;
        player.onGround = 1;
    }

    for (int i = 0; i < block_count; ++i) {
        SDL_Rect b = blocks[i];
        if (player.x < b.x + b.w && player.x + player.w > b.x &&
            player.y < b.y + b.h && player.y + player.h > b.y) {
            if (player.vy > 0 && player.y - player.vy + player.h <= b.y) {
                player.y = b.y - player.h;
                player.vy = 0;
                player.onGround = 1;
            } else if (player.vy < 0 && player.y - player.vy >= b.y + b.h) {
                player.y = b.y + b.h;
                player.vy = 0;
            } else if (player.vx > 0) {
                player.x = b.x - player.w;
            } else if (player.vx < 0) {
                player.x = b.x + b.w;
            }
        }
    }
}

static void gameRenderInternal(SDL_Renderer *r) {
    SDL_Rect bgRect = {0, 0, MAP_W * TILE, MAP_H * TILE};
    drawTexture("bg", NULL, &bgRect);

    for (int i = 0; i < MAP_W; ++i) {
        SDL_Rect d = {i * TILE, ground_y, TILE, TILE};
        drawTexture("ground", NULL, &d);
    }

    for (int i = 0; i < block_count; ++i) {
        drawTexture("block", NULL, &blocks[i]);
    }

    SDL_Rect p = {(int)player.x, (int)player.y, player.w, player.h};
    drawTexture("player", NULL, &p);
}

void gameInit() {
    player.x = 64;
    player.y = (MAP_H - 2) * TILE;
    player.w = TILE;
    player.h = TILE;
    player.vx = 0;
    player.vy = 0;
    player.onGround = 0;

    block_count = 2;
    blocks[0] = (SDL_Rect){5 * TILE, (MAP_H - 3) * TILE, TILE, TILE};
    blocks[1] = (SDL_Rect){9 * TILE, (MAP_H - 5) * TILE, TILE, TILE};

    tickF_add(gameTickInternal);
    renderF_add(gameRenderInternal);
}
