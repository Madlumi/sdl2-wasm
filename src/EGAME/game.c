#include "game.h"
#include "../MENGINE/renderer.h"
#include "../MENGINE/tick.h"
#include "../MENGINE/res.h"
#include <SDL.h>

// Render a texture-based island scene on a blue ocean with one palm tree.

static void gameTick(double dt) {
    // No game logic yet.
    (void)dt;
}

static void gameRender(SDL_Renderer *r) {
    (void)r; // drawTexture uses the global renderer

    int tileW, tileH;
    SDL_Texture *waterTex = resGetTexture("water");
    SDL_Texture *sandTex = resGetTexture("sand");
    SDL_QueryTexture(waterTex, NULL, NULL, &tileW, &tileH);

    int tilesX = (w + tileW - 1) / tileW;
    int tilesY = (h + tileH - 1) / tileH;
    const float radius = 3.5f; // base radius in tiles for island

    for (int ty = 0; ty < tilesY; ty++) {
        for (int tx = 0; tx < tilesX; tx++) {
            float dx = tx - tilesX / 2.0f + 0.5f;
            float dy = ty - tilesY / 2.0f + 0.5f;
            float noise = ((tx * 7 + ty * 3) % 3) * 0.3f; // simple jitter for organic edge
            float effectiveRadius = radius - noise;
            SDL_Texture *tex = waterTex;
            int baseShade = 180;
            if (dx * dx + dy * dy <= effectiveRadius * effectiveRadius) {
                tex = sandTex;
                baseShade = 220;
            }
            int shade = baseShade + ((tx * 23 + ty * 17) % 51) - 25; // range baseShade-25..baseShade+25
            if (shade < 0) shade = 0;
            if (shade > 255) shade = 255;
            SDL_SetTextureColorMod(tex, shade, shade, shade);
            SDL_Rect dst = { tx * tileW, ty * tileH, tileW, tileH };
            drawTexture(tex == sandTex ? "sand" : "water", NULL, &dst, 1);
        }
    }

    // reset texture color modulation
    SDL_SetTextureColorMod(waterTex, 255, 255, 255);
    SDL_SetTextureColorMod(sandTex, 255, 255, 255);

    // Palm tree centered on the island
    int palmW, palmH;
    SDL_Texture *palmTex = resGetTexture("palm");
    SDL_QueryTexture(palmTex, NULL, NULL, &palmW, &palmH);
    SDL_Rect palmDst = { w / 2 - palmW / 2, h / 2 - palmH / 2, palmW, palmH };
    drawTexture("palm", NULL, &palmDst, 1);
}

void gameInit() {
    tickF_add(gameTick);
    renderF_add(gameRender);
}

