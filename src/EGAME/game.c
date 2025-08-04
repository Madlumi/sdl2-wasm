#include "game.h"
#include "../MENGINE/renderer.h"
#include "../MENGINE/tick.h"
#include "../MENGINE/res.h"
#include "../MENGINE/keys.h"
#include <SDL.h>
#include <stdlib.h>

// Tile types for the simple island editor
enum { TILE_WATER = 0, TILE_SAND = 1 };

static int tileW, tileH;
static int tilesX, tilesY;
static unsigned char *tiles; // 2D array flattened: tiles[y * tilesX + x]

static void gameTick(double dt) {
    (void)dt;
    int tx = mpos.x / tileW;
    int ty = mpos.y / tileH;
    if (tx >= 0 && tx < tilesX && ty >= 0 && ty < tilesY) {
        if (Held(INP_LCLICK)) {
            tiles[ty * tilesX + tx] = TILE_SAND;
        } else if (Held(INP_RCLICK)) {
            tiles[ty * tilesX + tx] = TILE_WATER;
        }
    }
}

static void gameRender(SDL_Renderer *r) {
    SDL_Texture *waterTex = resGetTexture("water");
    SDL_Texture *sandTex = resGetTexture("sand");

    int hoverX = mpos.x / tileW;
    int hoverY = mpos.y / tileH;

    for (int ty = 0; ty < tilesY; ty++) {
        for (int tx = 0; tx < tilesX; tx++) {
            unsigned char t = tiles[ty * tilesX + tx];
            SDL_Texture *tex = (t == TILE_SAND) ? sandTex : waterTex;
            int baseShade = (t == TILE_SAND) ? 220 : 180;
            int shade = baseShade + ((tx * 23 + ty * 17) % 51) - 25;
            if (shade < 0) shade = 0;
            if (shade > 255) shade = 255;
            SDL_SetTextureColorMod(tex, shade, shade, shade);
            SDL_Rect dst = { tx * tileW, ty * tileH, tileW, tileH };
            drawTexture((t == TILE_SAND) ? "sand" : "water", NULL, &dst, 1);
            if (tx == hoverX && ty == hoverY) {
                SDL_SetRenderDrawColor(r, 255, 255, 255, 80);
                SDL_RenderFillRect(r, &dst);
            }
        }
    }

    SDL_SetTextureColorMod(waterTex, 255, 255, 255);
    SDL_SetTextureColorMod(sandTex, 255, 255, 255);

    int palmW, palmH;
    SDL_Texture *palmTex = resGetTexture("palm");
    SDL_QueryTexture(palmTex, NULL, NULL, &palmW, &palmH);
    SDL_Rect palmDst = { w / 2 - palmW / 2, h / 2 - palmH / 2, palmW, palmH };
    drawTexture("palm", NULL, &palmDst, 1);
}

void gameInit() {
    SDL_Texture *waterTex = resGetTexture("water");
    SDL_QueryTexture(waterTex, NULL, NULL, &tileW, &tileH);
    tilesX = (w + tileW - 1) / tileW;
    tilesY = (h + tileH - 1) / tileH;
    tiles = malloc(tilesX * tilesY);
    const float radius = 3.5f;
    for (int ty = 0; ty < tilesY; ty++) {
        for (int tx = 0; tx < tilesX; tx++) {
            float dx = tx - tilesX / 2.0f + 0.5f;
            float dy = ty - tilesY / 2.0f + 0.5f;
            float noise = ((tx * 7 + ty * 3) % 3) * 0.3f;
            float effectiveRadius = radius - noise;
            tiles[ty * tilesX + tx] = (dx * dx + dy * dy <= effectiveRadius * effectiveRadius) ? TILE_SAND : TILE_WATER;
        }
    }

    tickF_add(gameTick);
    renderF_add(gameRender);
}
