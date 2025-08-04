#include "game.h"
#include "../MENGINE/renderer.h"
#include "../MENGINE/tick.h"
#include "../MENGINE/res.h"
#include "../MENGINE/keys.h"
#include "../MENGINE/ui.h"
#include <SDL.h>
#include <SDL_ttf.h>
#include <stdlib.h>
#include <stdio.h>

// Tile types for the simple island editor
enum {
    TILE_GRASS = 0,
    TILE_WATER,
    TILE_SAND,
    TILE_STONE,
    TILE_DIRT,
    TILE_TOTAL
};

enum {
    OBJ_NONE = 0,
    OBJ_COCONUT,
    OBJ_SAPLING,
    OBJ_TREE,
    OBJ_WITHER
};

static int currentTile = TILE_SAND;
static int tileW, tileH;
static int tilesX, tilesY;

typedef struct {
    unsigned char type;
    unsigned char obj;
    double timer;
} Tile;

static Tile *tiles; // 2D array flattened: tiles[y * tilesX + x]

static int coconutCount = 0;

typedef struct {
    Elem base;
    int tileType;
    const char *label;
    int hovered;
    int pressed;
} TileButton;

static void tileButtonPress(Elem *e) {
    TileButton *tb = (TileButton *)e;
    currentTile = tb->tileType;
}

static void tileButtonUpdate(Elem *e) {
    TileButton *tb = (TileButton *)e;
    tb->hovered = INSQ(mpos, tb->base.area);
    tb->pressed = tb->hovered && Held(INP_CLICK);
}

static void tileButtonRender(Elem *e, SDL_Renderer *r) {
    TileButton *tb = (TileButton *)e;

    SDL_Rect img = { e->area.x + 16, e->area.y + 10, e->area.w - 32, e->area.w - 32 };

    SDL_Texture *tex = resGetTexture(tb->tileType == TILE_WATER ? "water" : "sand");
    Uint8 rCol = 255, gCol = 255, bCol = 255;
    switch (tb->tileType) {
        case TILE_GRASS: rCol = 50;  gCol = 200; bCol = 50;  break;
        case TILE_STONE: rCol = 130; gCol = 130; bCol = 130; break;
        case TILE_DIRT:  rCol = 150; gCol = 75;  bCol = 0;   break;
        case TILE_WATER:
        case TILE_SAND:
        default: break;
    }
    SDL_SetTextureColorMod(tex, rCol, gCol, bCol);
    SDL_RenderCopy(r, tex, NULL, &img);
    SDL_SetTextureColorMod(tex, 255, 255, 255);

    SDL_Color c = {255, 255, 255, 255};
    drawText("default_font", e->area.x + 5, e->area.y + e->area.h - 18, c, tb->label);

    if (tb->hovered) {
        SDL_SetRenderDrawColor(r, 255, 255, 255, 50);
        SDL_RenderFillRect(r, &e->area);
    }
    if (tb->pressed) {
        SDL_SetRenderDrawColor(r, 0, 0, 0, 100);
        SDL_RenderFillRect(r, &e->area);
    }

    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
    SDL_RenderDrawRect(r, &e->area);
}

static TileButton *newTileButton(RECT area, int tileType, const char *label) {
    TileButton *tb = malloc(sizeof(TileButton));
    tb->base.area = area;
    tb->base.onPress = tileButtonPress;
    tb->base.onUpdate = tileButtonUpdate;
    tb->base.onRender = tileButtonRender;
    tb->tileType = tileType;
    tb->label = label;
    tb->hovered = tb->pressed = 0;
    return tb;
}

static void gameTick(double dt) {
    int tx = mpos.x / tileW;
    int ty = mpos.y / tileH;
    if (tx >= 0 && tx < tilesX && ty >= 0 && ty < tilesY) {
        if (Held(INP_LCLICK)) {
            tiles[ty * tilesX + tx].type = currentTile;
        }
    }

    for (int i = 0; i < tilesX * tilesY; i++) {
        Tile *t = &tiles[i];
        if (t->type == TILE_GRASS) {
            if (t->obj == OBJ_NONE) {
                t->obj = OBJ_COCONUT;
                t->timer = 0;
            } else if (t->obj == OBJ_COCONUT) {
                t->timer += dt;
                if (t->timer >= 1) { t->obj = OBJ_SAPLING; t->timer = 0; }
            } else if (t->obj == OBJ_SAPLING) {
                t->timer += dt;
                if (t->timer >= 1) { t->obj = OBJ_TREE; t->timer = 0; }
            } else if (t->obj == OBJ_TREE) {
                t->timer += dt;
                if (t->timer >= 1) {
                    coconutCount++;
                    t->timer -= 1;
                }
            } else if (t->obj == OBJ_WITHER) {
                t->obj = OBJ_NONE;
                t->timer = 0;
            }
        } else {
            if (t->obj == OBJ_COCONUT || t->obj == OBJ_SAPLING || t->obj == OBJ_TREE) {
                t->obj = OBJ_WITHER;
                t->timer = 0;
            } else if (t->obj == OBJ_WITHER) {
                t->timer += dt;
                if (t->timer >= 1) { t->obj = OBJ_NONE; t->timer = 0; }
            }
        }
    }
}

static void gameRender(SDL_Renderer *r) {
    SDL_Texture *waterTex = resGetTexture("water");
    SDL_Texture *sandTex = resGetTexture("sand");

    int hoverX = mpos.x / tileW;
    int hoverY = mpos.y / tileH;

    SDL_Texture *palmTex = resGetTexture("palm");
    int palmW, palmH;
    SDL_QueryTexture(palmTex, NULL, NULL, &palmW, &palmH);

    for (int ty = 0; ty < tilesY; ty++) {
        for (int tx = 0; tx < tilesX; tx++) {
            Tile *tile = &tiles[ty * tilesX + tx];
            unsigned char t = tile->type;
            SDL_Texture *tex = (t == TILE_WATER) ? waterTex : sandTex;
            int baseShade = 200;
            Uint8 baseR = 255, baseG = 255, baseB = 255;
            switch (t) {
                case TILE_WATER: baseShade = 180; break;
                case TILE_SAND:  baseShade = 220; break;
                case TILE_GRASS: baseR = 50;  baseG = 200; baseB = 50;  break;
                case TILE_STONE: baseR = 130; baseG = 130; baseB = 130; break;
                case TILE_DIRT:  baseR = 150; baseG = 75;  baseB = 0;   break;
            }
            int shade = baseShade + ((tx * 23 + ty * 17) % 51) - 25;
            if (shade < 0) shade = 0;
            if (shade > 255) shade = 255;
            Uint8 rCol = (Uint8)((baseR * shade) / 255);
            Uint8 gCol = (Uint8)((baseG * shade) / 255);
            Uint8 bCol = (Uint8)((baseB * shade) / 255);
            SDL_SetTextureColorMod(tex, rCol, gCol, bCol);
            SDL_Rect dst = { tx * tileW, ty * tileH, tileW, tileH };
            SDL_RenderCopy(r, tex, NULL, &dst);
            if (tx == hoverX && ty == hoverY) {
                SDL_SetRenderDrawColor(r, 255, 255, 255, 80);
                SDL_RenderFillRect(r, &dst);
            }

            SDL_Rect objDst;
            switch (tile->obj) {
                case OBJ_COCONUT:
                    objDst.w = palmW / 4;
                    objDst.h = palmH / 4;
                    objDst.x = dst.x + tileW / 2 - objDst.w / 2;
                    objDst.y = dst.y + tileH / 2 - objDst.h / 2;
                    SDL_SetTextureColorMod(palmTex, 150, 75, 0);
                    SDL_RenderCopy(r, palmTex, NULL, &objDst);
                    SDL_SetTextureColorMod(palmTex, 255, 255, 255);
                    break;
                case OBJ_SAPLING:
                    objDst.w = palmW / 2;
                    objDst.h = palmH / 2;
                    objDst.x = dst.x + tileW / 2 - objDst.w / 2;
                    objDst.y = dst.y + tileH / 2 - objDst.h / 2;
                    SDL_SetTextureColorMod(palmTex, 50, 200, 50);
                    SDL_RenderCopy(r, palmTex, NULL, &objDst);
                    SDL_SetTextureColorMod(palmTex, 255, 255, 255);
                    break;
                case OBJ_TREE:
                    objDst = (SDL_Rect){ dst.x + tileW / 2 - palmW / 2, dst.y + tileH / 2 - palmH / 2, palmW, palmH };
                    SDL_RenderCopy(r, palmTex, NULL, &objDst);
                    break;
                case OBJ_WITHER:
                    objDst = (SDL_Rect){ dst.x + tileW / 2 - palmW / 2, dst.y + tileH / 2 - palmH / 2, palmW, palmH };
                    SDL_SetTextureColorMod(palmTex, 100, 100, 100);
                    SDL_RenderCopy(r, palmTex, NULL, &objDst);
                    SDL_SetTextureColorMod(palmTex, 255, 255, 255);
                    break;
                default:
                    break;
            }
        }
    }
    SDL_SetTextureColorMod(waterTex, 255, 255, 255);
    SDL_SetTextureColorMod(sandTex, 255, 255, 255);

    SDL_Texture *cocoTex = resGetTexture("coconut");
    if (cocoTex) {
        int iconW, iconH;
        SDL_QueryTexture(cocoTex, NULL, NULL, &iconW, &iconH);

        char buf[32];
        snprintf(buf, sizeof(buf), "%d", coconutCount);

        TTF_Font *font = resGetFont("default_font");
        int textW = 0, textH = 0;
        if (font) {
            TTF_SizeUTF8(font, buf, &textW, &textH);
        }

        SDL_Rect iconDst = { w - iconW - textW - 20, 10, iconW, iconH };
        SDL_RenderCopy(r, cocoTex, NULL, &iconDst);

        SDL_Color white = {255, 255, 255, 255};
        drawText("default_font", iconDst.x + iconW + 5, iconDst.y + (iconH - textH) / 2, white, "%s", buf);
    }
}

void gameInit() {
    SDL_Texture *waterTex = resGetTexture("water");
    SDL_QueryTexture(waterTex, NULL, NULL, &tileW, &tileH);
    tilesX = (w + tileW - 1) / tileW;
    tilesY = (h + tileH - 1) / tileH;
    tiles = malloc(sizeof(Tile) * tilesX * tilesY);
    coconutCount = 0;
    const float radius = 3.5f;
    for (int ty = 0; ty < tilesY; ty++) {
        for (int tx = 0; tx < tilesX; tx++) {
            float dx = tx - tilesX / 2.0f + 0.5f;
            float dy = ty - tilesY / 2.0f + 0.5f;
            float noise = ((tx * 7 + ty * 3) % 3) * 0.3f;
            float effectiveRadius = radius - noise;
            Tile *t = &tiles[ty * tilesX + tx];
            t->type = (dx * dx + dy * dy <= effectiveRadius * effectiveRadius) ? TILE_SAND : TILE_WATER;
            t->obj = OBJ_NONE;
            t->timer = 0;
        }
    }

    tickF_add(gameTick);
    renderF_add(gameRender);

    int handler = initUiHandler((RECT){0, 0, 100, h});
    int buttonSize = 80;
    int gap = 10;
    const char *labels[] = {"Grass", "Water", "Sand", "Stone", "Dirt"};
    int types[] = {TILE_GRASS, TILE_WATER, TILE_SAND, TILE_STONE, TILE_DIRT};
    for (int i = 0; i < 5; i++) {
        RECT br = {gap, gap + i * (buttonSize + gap), buttonSize, buttonSize};
        TileButton *tb = newTileButton(br, types[i], labels[i]);
        addElem(&ui[handler], (Elem *)tb);
    }
}
