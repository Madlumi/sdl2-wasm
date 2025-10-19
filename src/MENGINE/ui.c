
#include "mutilSDL.h"
#include "mutil.h"
#include "renderer.h"
#include "tick.h"
#include "keys.h"
#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <SDL.h>

UiHandler *ui = NULL;
I uiN = 0;


V uiTick(D dt) {
   (void)dt;
   if (ui == NULL) { return; }
   
   // Clamp mouse coordinates to prevent segfault
   I clampedX = mpos.x;
   I clampedY = mpos.y;
   if (clampedX < 0) clampedX = 0;
   if (clampedY < 0) clampedY = 0;
   if (clampedX >= WINW) clampedX = WINW - 1;
   if (clampedY >= WINH) clampedY = WINH - 1;
   
   POINT clampedPos = {clampedX, clampedY};
   
   FORY(uiN, { 
      FORX(ui[y].numElems, {
         if (ui[y].elems == NULL || ui[y].elems[x] == NULL) { continue; }
         if (ui[y].elems[x]->onUpdate == NULL) { continue; }
         ui[y].elems[x]->onUpdate(ui[y].elems[x]);

         if (INSQ(clampedPos, ui[y].elems[x]->area) && Held(INP_ENTER)) { 
            ui[y].elems[x]->onPress(ui[y].elems[x]); 
         }
         if (INSQ(clampedPos, ui[y].elems[x]->area) && Pressed(INP_CLICK)) { 
            ui[y].elems[x]->onPress(ui[y].elems[x]); 
         }
      });
   });
}
V uiRender(SDL_Renderer *r) {
   if (ui == NULL) { return; }
   FORY(uiN, { 
      FORX(ui[y].numElems, {
         if (ui[y].elems == NULL || ui[y].elems[x] == NULL) { continue; }
         if (ui[y].elems[x]->onRender == NULL) { continue; }
         ui[y].elems[x]->onRender(ui[y].elems[x], r);
      });
   });
}

V uiDestroy() { 
   if (ui == NULL) { return; }
   
   FORY(uiN, {
      FORX(ui[y].numElems, {
         if (ui[y].elems[x] != NULL) {
            free(ui[y].elems[x]);
            ui[y].elems[x] = NULL;
         }
      });
      if (ui[y].elems != NULL) {
         free(ui[y].elems);
         ui[y].elems = NULL;
      }
      ui[y].numElems = 0;
   });
   
   free(ui);
   ui = NULL;
   uiN = 0;
}

V uiDestroyHandler(int index) {
   if (ui == NULL || index < 0 || index >= uiN) { return; }
   
   FORX(ui[index].numElems, {
      if (ui[index].elems[x] != NULL) {
         free(ui[index].elems[x]);
         ui[index].elems[x] = NULL;
      }
   });
   
   if (ui[index].elems != NULL) {
      free(ui[index].elems);
      ui[index].elems = NULL;
   }
   ui[index].numElems = 0;
   
   for (int i = index; i < uiN - 1; i++) {
      ui[i] = ui[i + 1];
   }
   
   uiN--;
   ui = (UiHandler *)realloc(ui, uiN * sizeof(UiHandler));
}

I initUiHandler(RECT r) {
    if (ui == NULL) {
        tickF_add(uiTick);
        renderF_add(uiRender);
        ui = (UiHandler *)malloc(sizeof(UiHandler));
        uiN = 1;
        ui[0].area = r;
        ui[0].elems = NULL;
        ui[0].numElems = 0;
        return 0;
    }
    
    uiN++;
    ui = (UiHandler *)realloc(ui, uiN * sizeof(UiHandler));
    ui[uiN-1].area = r;
    ui[uiN-1].elems = NULL;
    ui[uiN-1].numElems = 0;
    return uiN-1;
}

I addElem(UiHandler *uh, Elem *b) {
   if (uh == NULL) { return -1; }
   
   uh->elems = (Elem **)realloc(uh->elems, (uh->numElems + 1) * sizeof(Elem *));
   if (uh->elems != NULL) {
       uh->elems[uh->numElems] = b;
       uh->numElems++;
       return uh->numElems - 1;
   } else {
       return -1;
   }
}

Elem *newElem(RECT r, void (*onPress)(Elem*), void (*onUpdate)(Elem*), void (*onRender)(Elem*, SDL_Renderer*)) {
    Elem *b = (Elem *)malloc(sizeof(Elem));
    if (b != NULL) {
        b->area = r;
        b->onPress = onPress;
        b->onUpdate = onUpdate;
        b->onRender = onRender;
    }
    return b;
}

// Helper function to convert RECT to SDL_Rect
static SDL_Rect rectToSdlRect(RECT r) {
    SDL_Rect sdlRect = {r.x, r.y, r.w, r.h};
    return sdlRect;
}

// Updated test functions for new renderer
V onPressFunction(Elem *e) {
    printf("Elem pressed! Position: (%d, %d)\n", e->area.x, e->area.y);
}

V onUpdateFunction(Elem *e) {
    // Simple hover effect - can add logic here
}

V onRenderFunction(Elem *e, SDL_Renderer *r) {
    (void)r; // We're using our renderer functions
    
    // Draw a button with different states
    SDL_Color bgColor = {100, 100, 100, 255}; // Default gray
    SDL_Color borderColor = {200, 200, 200, 255};
    
    // Change color if hovered
    if (INSQ(mpos, e->area)) {
        bgColor.r = 150;
        bgColor.g = 150;
        bgColor.b = 150;
        
        // Change color if pressed
        if (Held(INP_CLICK) && INSQ(mpos, e->area)) {
            bgColor.r = 80;
            bgColor.g = 80;
            bgColor.b = 80;
        }
    }
    
    // Draw button background
    drawRect(e->area.x, e->area.y, e->area.w, e->area.h, ANCHOR_TOP_L, bgColor);
    
    // Draw button border
    SDL_SetRenderDrawColor(renderer, borderColor.r, borderColor.g, borderColor.b, borderColor.a);
    SDL_Rect borderRect = rectToSdlRect(e->area);
    SDL_RenderDrawRect(renderer, &borderRect);
    
    // Draw texture
    drawTexture("noise_a", e->area.x + 10, e->area.y + 10, ANCHOR_TOP_L, NULL);
    
    // Draw text label
    SDL_Color textColor = {255, 255, 255, 255};
    drawText("default_font", e->area.x + e->area.w/2, e->area.y + e->area.h/2, 
             ANCHOR_CENTER, textColor, "Button");
}

// Additional UI utility functions
V drawUiRect(RECT area, SDL_Color color) {
    drawRect(area.x, area.y, area.w, area.h, ANCHOR_TOP_L, color);
}

V drawUiTexture(const C* name, RECT area, ANCHOR anchor) {
    drawTexture(name, area.x, area.y, anchor, NULL);
}

V drawUiText(const C* fontName, RECT area, ANCHOR anchor, SDL_Color color, const C* fmt, ...) {
    char buffer[MAX_DRAW_TEXT_LEN];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    
    drawText(fontName, area.x, area.y, anchor, color, buffer);
}

// Function to create a simple button
Elem *createButton(RECT area, const C* label, void (*onPress)(Elem*)) {
    Elem *button = newElem(area, onPress, onUpdateFunction, onRenderFunction);
    return button;
}
