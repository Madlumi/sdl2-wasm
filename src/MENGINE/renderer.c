
#include <SDL.h>
#include <SDL_image.h>
#include "mutil.h"
#include "renderer.h"
#include "debug.h"
#include "res.h"
#include <SDL_ttf.h>
#include <stdarg.h>
#include <stdlib.h>

// Global variables
I WINW = 0, WINH = 0;
I XOFF = 0, YOFF = 0;
D ZOOM = 1.0;
D UIZOOM = 1.0;
SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;

// FPS control variables
static Uint32 frameStart = 0;
static I targetFPS = 60;
static I currentFPS = 0;
static I frameCount = 0;
static Uint32 fpsLastTime = 0;

// Render function pool
new_pool(renderF, RenderFunction);

// Helper function to calculate anchored position
static void calculateAnchorPosition(I* x, I* y, I w, I h, ANCHOR anchor) {
    switch(anchor) {
        case ANCHOR_TOP_L:
            // No adjustment needed
            break;
        case ANCHOR_TOP_R:
            *x = WINW - w - *x;
            break;
        case ANCHOR_BOT_L:
            *y = WINH - h - *y;
            break;
        case ANCHOR_BOT_R:
            *x = WINW - w - *x;
            *y = WINH - h - *y;
            break;
        case ANCHOR_MID_L:
            *y = (WINH - h) / 2 + *y;
            break;
        case ANCHOR_MID_R:
            *x = WINW - w - *x;
            *y = (WINH - h) / 2 + *y;
            break;
        case ANCHOR_MID_BOT:
            *x = (WINW - w) / 2 + *x;
            *y = WINH - h - *y;
            break;
        case ANCHOR_MID_TOP:
            *x = (WINW - w) / 2 + *x;
            break;
        case ANCHOR_CENTER:
            *x = (WINW - w) / 2 + *x;
            *y = (WINH - h) / 2 + *y;
            break;
        case ANCHOR_NONE:
        default:
            // For world-space rendering, apply camera transform
            *x = (*x - XOFF) * ZOOM;
            *y = (*y - YOFF) * ZOOM;
            break;
    }
}

V renderUpdateWindowSize() {
    if (window) {
        SDL_GetWindowSize(window, &WINW, &WINH);
    }
}

V titleSet(const C* title) {
    if (window) SDL_SetWindowTitle(window, title);
}

V setWindowflags(U32 flags) {
    // Can only set flags before window creation
}

V setRenderflags(U32 flags) {
    // Can only set flags before renderer creation
}

B renderInit(I w, I h, const C* title) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        THROW("SDL_Init Error: %s\n", SDL_GetError());
        return false;
    }
    
    window = SDL_CreateWindow(title, 
                             SDL_WINDOWPOS_CENTERED, 
                             SDL_WINDOWPOS_CENTERED,
                             w, h,
                             SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);
    
    if (!window) {
        THROW("SDL_CreateWindow Error: %s\n", SDL_GetError());
        return false;
    }
    
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        THROW("SDL_CreateRenderer Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        window = NULL;
        return false;
    }
    
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    
    // Initialize window size variables
    renderUpdateWindowSize();
    
    // Load resources AFTER renderer is created
    resLoadAll(renderer);
    
    // Initialize FPS tracking
    frameStart = SDL_GetTicks();
    fpsLastTime = frameStart;
    
    return true;
}

V renderFree() {
    if (renderer) {
        SDL_DestroyRenderer(renderer);
        renderer = NULL;
    }
    
    if (window) {
        SDL_DestroyWindow(window);
        window = NULL;
    }
    
    SDL_Quit();
}

V setTargetFPS(I fps) {
    targetFPS = fps > 0 ? fps : 60;
}

V capFPS() {
    Uint32 frameTime = SDL_GetTicks() - frameStart;
    Uint32 targetFrameTime = 1000 / targetFPS;
    
    if (frameTime < targetFrameTime) {
        SDL_Delay(targetFrameTime - frameTime);
    }
    
    // Update FPS counter
    frameCount++;
    Uint32 currentTime = SDL_GetTicks();
    if (currentTime - fpsLastTime >= 1000) {
        currentFPS = frameCount;
        frameCount = 0;
        fpsLastTime = currentTime;
    }
    
    frameStart = SDL_GetTicks();
}

I getFPS() {
    return currentFPS;
}

V render() {
    // Always check current window size before rendering
    renderUpdateWindowSize();
    
    // Clear with a dark background
    SDL_SetRenderDrawColor(renderer, 0, 20, 40, 255);
    SDL_RenderClear(renderer);
    
    // Execute all render callbacks
    FOR(renderF_num, { renderF_pool[i](renderer); });
    
    // Present the rendered frame
    SDL_RenderPresent(renderer);
    
    // Cap frame rate if needed
    capFPS();
}

// Helper function to avoid code duplication  
static void renderText(TTF_Font *font, I x, I y, SDL_Color c, ANCHOR anchor, const C* text) {
    if (!font || !text) return;
    
    SDL_Surface *s = TTF_RenderUTF8_Blended(font, text, c);
    if (!s) {
        THROW("TTF_RenderUTF8_Blended Error: %s\n", TTF_GetError());
        return;
    }
    
    SDL_Texture *t = SDL_CreateTextureFromSurface(renderer, s);
    if (!t) {
        SDL_FreeSurface(s);
        THROW("SDL_CreateTextureFromSurface Error: %s\n", SDL_GetError());
        return;
    }
    
    I w = s->w, h = s->h;
    calculateAnchorPosition(&x, &y, w, h, anchor);
    
    SDL_Rect dst = {x, y, w, h};
    SDL_RenderCopy(renderer, t, NULL, &dst);
    
    SDL_DestroyTexture(t);
    SDL_FreeSurface(s);
}

void drawText(const C* fontName, I x, I y, ANCHOR anchor, SDL_Color c, const C* fmt, ...) {
    TTF_Font *font = resGetFont(fontName);
    if (!font) {
        THROW("Font not found: %s\n", fontName);
        return;
    }
    
    va_list args;
    va_start(args, fmt);
    
    // First, try with stack buffer
    char stack_buf[MAX_DRAW_TEXT_LEN];
    int required_len = vsnprintf(stack_buf, sizeof(stack_buf), fmt, args);
    
    // If it fits in stack buffer, use it directly
    if (required_len < (int)sizeof(stack_buf)) {
        va_end(args);
        renderText(font, x, y, c, anchor, stack_buf);
        return;
    }
    
    // Otherwise, use heap allocation
    va_end(args);
    va_start(args, fmt);
    
    char *heap_buf = malloc(required_len + 1);
    if (!heap_buf) {
        va_end(args);
        THROW("Memory allocation failed for text rendering\n");
        return;
    }
    
    vsnprintf(heap_buf, required_len + 1, fmt, args);
    va_end(args);
    
    renderText(font, x, y, c, anchor, heap_buf);
    free(heap_buf);
}

void drawTexture(const C* name, I x, I y, ANCHOR anchor, const C* palName) {
    if (!name) {
        THROW("drawTexture called with NULL name\n");
        return;
    }
    
    SDL_Texture *tex = resGetTexture(name);
    if (!tex) {
        THROW("Texture not found: %s\n", name);
        return;
    }
    
    // Get texture dimensions
    I w, h;
    if (SDL_QueryTexture(tex, NULL, NULL, &w, &h) != 0) {
        THROW("Failed to query texture dimensions: %s\n", SDL_GetError());
        return;
    }
    
    if (w == 0 || h == 0) {
        THROW("Texture has zero dimensions: %s (%dx%d)\n", name, w, h);
        return;
    }
    
    // Handle palette if specified
    if (palName) {
        SDL_Surface *surf = resGetSurface(name);
        PalRes *pal = resGetPalette(palName);
        if (surf && pal) {
            SDL_Surface *tmp = SDL_ConvertSurfaceFormat(surf, SDL_PIXELFORMAT_RGBA32, 0);
            if (tmp) {
                SDL_LockSurface(tmp);
                Uint32 *pix = (Uint32 *)tmp->pixels;
                int count = (tmp->w * tmp->h);
                for (int i = 0; i < count; i++) {
                    Uint8 r, g, b, a;
                    SDL_GetRGBA(pix[i], tmp->format, &r, &g, &b, &a);
                    int idx = (r + g + b) * pal->count / (3 * 256);
                    if (idx >= pal->count) idx = pal->count - 1;
                    Uint32 col = pal->cols[idx];
                    Uint8 nr = (col >> 24) & 0xFF;
                    Uint8 ng = (col >> 16) & 0xFF;
                    Uint8 nb = (col >> 8) & 0xFF;
                    Uint8 na = col & 0xFF;
                    pix[i] = SDL_MapRGBA(tmp->format, nr, ng, nb, (Uint8)(a * (na / 255.0f)));
                }
                SDL_UnlockSurface(tmp);
                
                // Create temporary texture from modified surface
                SDL_Texture *palTex = SDL_CreateTextureFromSurface(renderer, tmp);
                if (palTex) {
                    tex = palTex;
                    // Update dimensions for the new texture
                    w = tmp->w;
                    h = tmp->h;
                }
                SDL_FreeSurface(tmp);
            }
        }
    }
    
    // Calculate position based on anchor
    calculateAnchorPosition(&x, &y, w, h, anchor);
    
    // Draw the texture
    SDL_Rect dst = {x, y, w, h};
    SDL_RenderCopy(renderer, tex, NULL, &dst);
    
    // Clean up temporary palette texture if we created one
    if (palName && tex != resGetTexture(name)) {
        SDL_DestroyTexture(tex);
    }
}

void drawRect(I x, I y, I w, I h, ANCHOR anchor, SDL_Color c) {
    calculateAnchorPosition(&x, &y, w, h, anchor);

    SDL_Rect rect = {x, y, w, h};
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
    SDL_RenderFillRect(renderer, &rect);
}

SDL_Rect worldRect(F left, F top, F w, F h) {
    F fx = (left - XOFF) * ZOOM;
    F fy = (top - YOFF) * ZOOM;
    F fx2 = ((left + w) - XOFF) * ZOOM;
    F fy2 = ((top + h) - YOFF) * ZOOM;

    SDL_Rect r;
    r.x = (I)floorf(fx);
    r.y = (I)floorf(fy);
    r.w = (I)ceilf(fx2) - r.x;
    r.h = (I)ceilf(fy2) - r.y;
    return r;
}

void drawWorldRect(F left, F top, F w, F h, SDL_Color c) {
    SDL_Rect r = worldRect(left, top, w, h);
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
    SDL_RenderFillRect(renderer, &r);
}

void drawLine(I x1, I y1, I x2, I y2, SDL_Color c) {
    // For world-space lines, apply camera transform
    if (XOFF != 0 || ZOOM != 1.0) {
        x1 = (x1 - XOFF) * ZOOM;
        x2 = (x2 - XOFF) * ZOOM;
    }
    if (YOFF != 0 || ZOOM != 1.0) {
        y1 = (y1 - YOFF) * ZOOM;
        y2 = (y2 - YOFF) * ZOOM;
    }
    
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
    SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
}

V screenToWorld(I sx, I sy, D* wx, D* wy) {
    if (wx) *wx = (sx / ZOOM) + XOFF;
    if (wy) *wy = (sy / ZOOM) + YOFF;
}

V worldToScreen(D wx, D wy, I* sx, I* sy) {
    if (sx) *sx = (wx - XOFF) * ZOOM;
    if (sy) *sy = (wy - YOFF) * ZOOM;
}
