#include "game.h"
#include "../MENGINE/renderer.h"
#include "../MENGINE/keys.h"
#include "../MENGINE/tick.h"
#include <SDL.h>
#include <stdio.h>

#define TILE 32
#define MAP_W 64
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
static SDL_Rect blocks[32];
static int block_count = 0;
static const int ground_y = (MAP_H - 1) * TILE;
static float camera_x = 0.0f;
static Uint32 start_ticks = 0;

typedef struct {
    float x, y;
    float vx;
    int w, h;
} Enemy;

static Enemy enemies[8];
static int enemy_count = 0;

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

    for(int i=0;i<enemy_count;i++){
        Enemy *e = &enemies[i];
        e->x += e->vx;
        if(e->x < 0 || e->x + e->w > MAP_W*TILE){
            e->vx = -e->vx;
        }
        SDL_Rect r = {(int)e->x, (int)e->y, e->w, e->h};
        if(player.x < r.x + r.w && player.x + player.w > r.x &&
           player.y < r.y + r.h && player.y + player.h > r.y){
            player.x = 64;
            player.y = (MAP_H - 2)*TILE;
            player.vx = player.vy = 0;
        }
    }

    camera_x = player.x - w/2;
    if(camera_x < 0) camera_x = 0;
    int maxCam = MAP_W*TILE - w;
    if(camera_x > maxCam) camera_x = maxCam;
}

static void gameRenderInternal(SDL_Renderer *r) {
    SDL_Rect bgRect = {(int)-camera_x, 0, MAP_W * TILE, MAP_H * TILE};
    drawTexture("bg", NULL, &bgRect);

    for (int i = 0; i < MAP_W; ++i) {
        SDL_Rect d = {i * TILE - (int)camera_x, ground_y, TILE, TILE};
        drawTexture("ground", NULL, &d);
    }

    for (int i = 0; i < block_count; ++i) {
        SDL_Rect d = {blocks[i].x - (int)camera_x, blocks[i].y, blocks[i].w, blocks[i].h};
        drawTexture("block", NULL, &d);
    }

    for(int i=0;i<enemy_count;i++){
        SDL_Rect d = {(int)enemies[i].x - (int)camera_x, (int)enemies[i].y, enemies[i].w, enemies[i].h};
        drawTexture("block", NULL, &d);
    }

    SDL_Rect p = {(int)player.x - (int)camera_x, (int)player.y, player.w, player.h};
    drawTexture("player", NULL, &p);

    char buf[32];
    float secs = (SDL_GetTicks() - start_ticks)/1000.0f;
    snprintf(buf, sizeof(buf), "%.1fs", secs);
    drawText("default_font", buf, 10, 10, (SDL_Color){255,255,255,255});
}

void gameInit() {
    player.x = 64;
    player.y = (MAP_H - 2) * TILE;
    player.w = TILE;
    player.h = TILE;
    player.vx = 0;
    player.vy = 0;
    player.onGround = 0;

    block_count = 5;
    blocks[0] = (SDL_Rect){5 * TILE, (MAP_H - 3) * TILE, TILE, TILE};
    blocks[1] = (SDL_Rect){12 * TILE, (MAP_H - 4) * TILE, TILE, TILE};
    blocks[2] = (SDL_Rect){20 * TILE, (MAP_H - 6) * TILE, TILE, TILE};
    blocks[3] = (SDL_Rect){30 * TILE, (MAP_H - 3) * TILE, TILE, TILE};
    blocks[4] = (SDL_Rect){40 * TILE, (MAP_H - 5) * TILE, TILE, TILE};

    enemy_count = 2;
    enemies[0] = (Enemy){15 * TILE, (MAP_H - 2) * TILE, 1.0f, TILE, TILE};
    enemies[1] = (Enemy){35 * TILE, (MAP_H - 2) * TILE, -1.2f, TILE, TILE};

    start_ticks = SDL_GetTicks();
    camera_x = 0;

    tickF_add(gameTickInternal);
    renderF_add(gameRenderInternal);
}
