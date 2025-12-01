
#ifndef RENDERER_H
#define RENDERER_H

#include <SDL.h>
#include "mutil.h"
#include <SDL_opengl.h>

// Anchor points for UI positioning
typedef enum {
    ANCHOR_NONE,
    ANCHOR_TOP_L,
    ANCHOR_TOP_R,
    ANCHOR_BOT_L,
    ANCHOR_BOT_R,
    ANCHOR_MID_L,
    ANCHOR_MID_R,
    ANCHOR_MID_BOT,
    ANCHOR_MID_TOP,
    ANCHOR_CENTER
} ANCHOR;

#define BASEW 512
#define BASEH 512
#define MAX_DRAW_TEXT_LEN 2048

//==============================================================================================================================
//========================================             WINDOW                 ==================================================
//==============================================================================================================================
E I WINW, WINH; // Window size
E I XOFF, YOFF; // Camera offset
E D ZOOM;       // World zoom
E D UIZOOM;     // UI zoom
E SDL_Window *window;
E SDL_Renderer *renderer;
E SDL_GLContext gl_context;

// Window management
V titleSet(const C* title);              // Set window title
V setWindowflags(U32 flags);             // Set window flags  
V setRenderflags(U32 flags);             // Set renderer flags
B renderInit(I w, I h, const C* title);  // Initialize renderer
V renderFree();                          // Cleanup renderer

// Frame rate control
V setTargetFPS(I fps);                   // Set target frames per second
V capFPS();                              // Cap frame rate to target
I getFPS();                              // Get current FPS

//==============================================================================================================================
//========================================           RENDER                   ==================================================
//==============================================================================================================================
// Function pool for render callbacks
typedef void (*RenderFunction)(SDL_Renderer*);
new_pool_h(renderF, RenderFunction);

V render();                              // Main render function

//==============================================================================================================================
//========================================             DRAWING                ==================================================
//==============================================================================================================================
// Drawing functions
void drawText(const C* fontName, I x, I y, ANCHOR anchor, SDL_Color c, const C* fmt, ...);
void drawTexture(const C* name, I x, I y, ANCHOR anchor, const C* palName);
void drawRect(I x, I y, I w, I h, ANCHOR anchor, SDL_Color c);
void drawLine(I x1, I y1, I x2, I y2, SDL_Color c);

//==============================================================================================================================
//========================================             UTILITIES              ==================================================
//==============================================================================================================================
// Coordinate conversion
V screenToWorld(I sx, I sy, D* wx, D* wy);     // Convert screen to world coordinates
V worldToScreen(D wx, D wy, I* sx, I* sy);     // Convert world to screen coordinates

//==============================================================================================================================
//========================================                                    ==================================================
//==============================================================================================================================
//
#endif
