
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
#include <math.h>

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

// Camera and zoom variables - using renderer's global variables
static const float MIN_ZOOM = 0.1f;
static const float MAX_ZOOM = 4.0f;
static const float ZOOM_SPEED = 0.1f;
static const float PAN_SPEED = 300.0f; // pixels per second
static const int EDGE_PAN_MARGIN = 10; // pixels from edge to start panning

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
    POINT mousePoint = {mpos.x, mpos.y};
    tb->hovered = INSQ(mousePoint, tb->base.area);
    tb->pressed = tb->hovered && Held(INP_CLICK);
}

static void tileButtonRender(Elem *e, SDL_Renderer *r) {
    TileButton *tb = (TileButton *)e;
    SDL_Rect area = e->area;

    // Draw button background based on state
    SDL_Color bgColor = {80, 80, 80, 255};
    if (tb->hovered) {
        bgColor.r = bgColor.g = bgColor.b = 120;
    }
    if (tb->pressed) {
        bgColor.r = bgColor.g = bgColor.b = 60;
    }
    if (currentTile == tb->tileType) {
        bgColor.r = 200; // Highlight selected tile
    }
    
    drawRect(area.x, area.y, area.w, area.h, ANCHOR_TOP_L, bgColor);
    
    // Draw tile preview (centered in button)
    const char* texName = "sand";
    SDL_Color tint = {255, 255, 255, 255};
    switch (tb->tileType) {
        case TILE_WATER: 
            texName = "water";
            tint.r = 100; tint.g = 100; tint.b = 255;
            break;
        case TILE_GRASS:
            texName = "sand";
            tint.r = 50; tint.g = 200; tint.b = 50;
            break;
        case TILE_STONE:
            texName = "sand";
            tint.r = 130; tint.g = 130; tint.b = 130;
            break;
        case TILE_DIRT:
            texName = "sand";
            tint.r = 150; tint.g = 75; tint.b = 0;
            break;
    }
    
    // Apply tint to texture and draw centered in button
    SDL_Texture *tex = resGetTexture(texName);
    if (tex) {
        SDL_SetTextureColorMod(tex, tint.r, tint.g, tint.b);
        drawTexture(texName, area.x + area.w/2, area.y + area.h/2 - 10, ANCHOR_CENTER, NULL);
        SDL_SetTextureColorMod(tex, 255, 255, 255); // Reset
    }
    
    // Draw label at bottom of button
    SDL_Color textColor = {255, 255, 255, 255};
    drawText("default_font", area.x + area.w/2, area.y + area.h - 5, 
             ANCHOR_CENTER, textColor, tb->label);
    
    // Draw border
    SDL_SetRenderDrawColor(r, 200, 200, 200, 255);
    SDL_RenderDrawRect(r, &area);
}

static TileButton *newTileButton(RECT area, int tileType, const char *label) {
    TileButton *tb = malloc(sizeof(TileButton));
    tb->base.area = area;
    tb->base.onPress = (void(*)(Elem*))tileButtonPress;
    tb->base.onUpdate = (void(*)(Elem*))tileButtonUpdate;
    tb->base.onRender = (void(*)(Elem*, SDL_Renderer*))tileButtonRender;
    tb->tileType = tileType;
    tb->label = label;
    tb->hovered = tb->pressed = 0;
    return tb;
}

static void handleZoom(double dt) {
    extern int mouseWheelMoved;
    
    if (mouseWheelMoved > 0) {
        float oldZoom = ZOOM;
        ZOOM += ZOOM_SPEED;
        if (ZOOM > MAX_ZOOM) ZOOM = MAX_ZOOM;
        
        // Zoom towards mouse position
        if (ZOOM != oldZoom) {
            float zoomFactor = ZOOM / oldZoom;
            
            // Convert mouse position to world coordinates
            float worldMouseX = mpos.x / oldZoom + XOFF;
            float worldMouseY = mpos.y / oldZoom + YOFF;
            
            XOFF = worldMouseX - (worldMouseX - XOFF) / zoomFactor;
            YOFF = worldMouseY - (worldMouseY - YOFF) / zoomFactor;
        }
        mouseWheelMoved = 0;
    } else if (mouseWheelMoved < 0) {
        float oldZoom = ZOOM;
        ZOOM -= ZOOM_SPEED;
        if (ZOOM < MIN_ZOOM) ZOOM = MIN_ZOOM;
        
        // Zoom towards mouse position
        if (ZOOM != oldZoom) {
            float zoomFactor = ZOOM / oldZoom;
            
            // Convert mouse position to world coordinates
            float worldMouseX = mpos.x / oldZoom + XOFF;
            float worldMouseY = mpos.y / oldZoom + YOFF;
            
            XOFF = worldMouseX - (worldMouseX - XOFF) / zoomFactor;
            YOFF = worldMouseY - (worldMouseY - YOFF) / zoomFactor;
        }
        mouseWheelMoved = 0;
    }
}

static void handleEdgePanning(double dt) {
    float panDistance = PAN_SPEED * dt / ZOOM;
    
    // Check if mouse is near edges and pan accordingly
    if (mpos.x <= EDGE_PAN_MARGIN) {
        XOFF -= panDistance;
    } else if (mpos.x >= WINW - EDGE_PAN_MARGIN) {
        XOFF += panDistance;
    }
    
    if (mpos.y <= EDGE_PAN_MARGIN) {
        YOFF -= panDistance;
    } else if (mpos.y >= WINH - EDGE_PAN_MARGIN) {
        YOFF += panDistance;
    }
}

static void gameTick(double dt) {
    handleZoom(dt);
    handleEdgePanning(dt);
    
    // Convert mouse position to world coordinates
    float worldMouseX = mpos.x / ZOOM + XOFF;
    float worldMouseY = mpos.y / ZOOM + YOFF;
    
    // Convert world coordinates to tile coordinates
    int tx = (int)(worldMouseX / tileW);
    int ty = (int)(worldMouseY / tileH);
    
    if (tx >= 0 && tx < tilesX && ty >= 0 && ty < tilesY) {
        if (Held(INP_LCLICK)) {
            Tile *t = &tiles[ty * tilesX + tx];
            if (currentTile == TILE_GRASS) {
                if (t->type != TILE_GRASS && coconutCount >= 5) {
                    coconutCount -= 5;
                    t->type = TILE_GRASS;
                }
            } else {
                t->type = currentTile;
            }
        }
    }

    for (int i = 0; i < tilesX * tilesY; i++) {
        Tile *t = &tiles[i];
        if (t->type == TILE_GRASS || t->type == TILE_SAND) {
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

static void renderWorld(SDL_Renderer *r) {
    // Convert mouse position to world coordinates for hover detection
    float worldMouseX = mpos.x / ZOOM + XOFF;
    float worldMouseY = mpos.y / ZOOM + YOFF;
    int hoverX = (int)(worldMouseX / tileW);
    int hoverY = (int)(worldMouseY / tileH);

    // Calculate visible tile range for culling (XOFF/YOFF are world top-left)
    float viewWidth = WINW / ZOOM;
    float viewHeight = WINH / ZOOM;

    float worldLeft   = XOFF;
    float worldRight  = XOFF + viewWidth;
    float worldTop    = YOFF;
    float worldBottom = YOFF + viewHeight;
    
    int startTileX = (int)fmaxf(0.0f, floorf(worldLeft  / tileW));
    int endTileX   = (int)fminf((float)tilesX, ceilf(worldRight / tileW));
    int startTileY = (int)fmaxf(0.0f, floorf(worldTop   / tileH));
    int endTileY   = (int)fminf((float)tilesY, ceilf(worldBottom / tileH));

    // Render terrain
    for (int ty = startTileY; ty < endTileY; ty++) {
        for (int tx = startTileX; tx < endTileX; tx++) {
            Tile *tile = &tiles[ty * tilesX + tx];
            
            // Determine texture and color based on tile type
            const char* texName = "sand";
            SDL_Color tint = {255, 255, 255, 255};
            switch (tile->type) {
                case TILE_WATER: 
                    texName = "water";
                    tint.r = 100; tint.g = 100; tint.b = 255;
                    break;
                case TILE_GRASS:
                    texName = "sand";
                    tint.r = 50; tint.g = 200; tint.b = 50;
                    break;
                case TILE_STONE:
                    texName = "sand";
                    tint.r = 130; tint.g = 130; tint.b = 130;
                    break;
                case TILE_DIRT:
                    texName = "sand";
                    tint.r = 150; tint.g = 75; tint.b = 0;
                    break;
                default:
                    break;
            }
            
            SDL_Texture *tex = resGetTexture(texName);
            if (tex) {
                SDL_SetTextureColorMod(tex, tint.r, tint.g, tint.b);

                int sx, sy;
                worldToScreen(tx * tileW, ty * tileH, &sx, &sy);
                SDL_Rect dst = { sx, sy, (int)(tileW * ZOOM), (int)(tileH * ZOOM) };

                SDL_RenderCopy(r, tex, NULL, &dst);
                SDL_SetTextureColorMod(tex, 255, 255, 255); // Reset
            }
            
            // Draw hover highlight
            if (tx == hoverX && ty == hoverY &&
                hoverX >= 0 && hoverY >= 0 &&
                hoverX < tilesX && hoverY < tilesY) {

                SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(r, 255, 255, 255, 80);
                
                int screenX, screenY;
                worldToScreen(tx * tileW, ty * tileH, &screenX, &screenY);
                SDL_Rect highlightRect = {
                    screenX,
                    screenY,
                    (int)(tileW * ZOOM),
                    (int)(tileH * ZOOM)
                };
                SDL_RenderFillRect(r, &highlightRect);
            }
        }
    }
    
    // Render objects on top of terrain
    for (int ty = startTileY; ty < endTileY; ty++) {
        for (int tx = startTileX; tx < endTileX; tx++) {
            Tile *tile = &tiles[ty * tilesX + tx];
            
            if (tile->obj != OBJ_NONE) {
                const char* objName = NULL;
                SDL_Color objTint = {255, 255, 255, 255};
                float objScale = 1.0f;
                
                switch (tile->obj) {
                    case OBJ_COCONUT:
                        objName = "coconut";
                        objScale = 0.5f;
                        break;
                    case OBJ_SAPLING:
                        objName = "palm";
                        objScale = 0.5f;
                        objTint.r = 50; objTint.g = 200; objTint.b = 50;
                        break;
                    case OBJ_TREE:
                        objName = "palm";
                        break;
                    case OBJ_WITHER:
                        objName = "palm";
                        objTint.r = 100; objTint.g = 100; objTint.b = 100;
                        break;
                    default:
                        break;
                }
                
                if (objName) {
                    SDL_Texture *objTex = resGetTexture(objName);
                    if (objTex) {
                        SDL_SetTextureColorMod(objTex, objTint.r, objTint.g, objTint.b);
                        
                        // World position at tile center
                        int objWorldX = tx * tileW + tileW / 2;
                        int objWorldY = ty * tileH + tileH / 2;

                        int sx, sy;
                        worldToScreen(objWorldX, objWorldY, &sx, &sy);

                        int w = (int)(tileW * objScale * ZOOM);
                        int h = (int)(tileH * objScale * ZOOM);
                        SDL_Rect dst = {
                            sx - w / 2,
                            sy - h / 2,
                            w,
                            h
                        };
                        
                        SDL_RenderCopy(r, objTex, NULL, &dst);
                        SDL_SetTextureColorMod(objTex, 255, 255, 255);
                    }
                }
            }
        }
    }
}

static void renderUI(SDL_Renderer *r) {
    // UI elements (drawn on top of everything)
    SDL_Color white = {255, 255, 255, 255};
    
    // Coconut counter (top right)
    drawTexture("coconut", WINW - 30, 20, ANCHOR_TOP_R, NULL);
    drawText("default_font", WINW - 40, 20, ANCHOR_TOP_R, white, "%d", coconutCount);
    
    // Zoom level indicator (top left)
    drawText("default_font", 10, 10, ANCHOR_TOP_L, white, "Zoom: %.1fx", ZOOM);
    
    // Instructions (bottom center)
    drawText("default_font", WINW/2, WINH - 10, ANCHOR_MID_BOT, white, 
             "WASD/Edge pan - Move | Mouse Wheel - Zoom | Click - Place tiles");
}

static void gameRender(SDL_Renderer *r) {
    // Render world (terrain + objects)
    renderWorld(r);
    
    // Render UI on top
    renderUI(r);
}

void gameInit() {
    // Get tile dimensions from water texture
    SDL_Texture *waterTex = resGetTexture("water");
    SDL_QueryTexture(waterTex, NULL, NULL, &tileW, &tileH);
    
    // Create a large world
    tilesX = 100;
    tilesY = 100;
    
    tiles = malloc(sizeof(Tile) * tilesX * tilesY);
    coconutCount = 10; // Start with some coconuts
    
    // Set initial zoom
    ZOOM = 1.0f;

    // Initialize camera so that the view is centered on the world.
    // XOFF/YOFF are treated as world-space top-left of the screen.
    float viewWidth  = WINW / ZOOM;
    float viewHeight = WINH / ZOOM;
    float worldWidth  = tilesX * tileW;
    float worldHeight = tilesY * tileH;

    XOFF = worldWidth  * 0.5f - viewWidth  * 0.5f;
    YOFF = worldHeight * 0.5f - viewHeight * 0.5f;
    
    // Initialize world with water and a central island
    for (int ty = 0; ty < tilesY; ty++) {
        for (int tx = 0; tx < tilesX; tx++) {
            Tile *t = &tiles[ty * tilesX + tx];
            
            // Create island in the center
            float dx = (float)tx - (float)(tilesX / 2);
            float dy = (float)ty - (float)(tilesY / 2);
            float distFromCenter = sqrtf(dx * dx + dy * dy);
            
            if (distFromCenter < 5) {
                t->type = TILE_SAND;
            } else if (distFromCenter < 15) {
                t->type = (rand() % 3 == 0) ? TILE_SAND : TILE_WATER;
            } else {
                t->type = TILE_WATER;
            }
            
            t->obj = OBJ_NONE;
            t->timer = 0;
        }
    }
    
    // Add a tree in the center
    int centerX = tilesX / 2;
    int centerY = tilesY / 2;
    Tile *center = &tiles[centerY * tilesX + centerX];
    center->type = TILE_SAND;
    center->obj = OBJ_TREE;
    center->timer = 0;

    // Register game functions
    tickF_add(gameTick);
    renderF_add(gameRender);

    // Create UI buttons (positioned properly)
    int handler = initUiHandler((RECT){10, WINH - 90, WINW - 20, 80});
    int buttonSize = 70;
    int gap = 10;
    const char *labels[] = {"Grass", "Water", "Sand", "Stone", "Dirt"};
    int types[] = {TILE_GRASS, TILE_WATER, TILE_SAND, TILE_STONE, TILE_DIRT};
    
    // Calculate total width needed for buttons
    int totalWidth = 5 * buttonSize + 4 * gap;
    int startX = (WINW - totalWidth) / 2;
    
    for (int i = 0; i < 5; i++) {
        RECT br = (RECT){startX + i * (buttonSize + gap), WINH - 80, buttonSize, buttonSize};
        TileButton *tb = newTileButton(br, types[i], labels[i]);
        addElem(&ui[handler], (Elem *)tb);
    }
}

